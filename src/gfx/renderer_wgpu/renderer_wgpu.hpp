#pragma once

#include <webgpu/webgpu_cpp.h>

#include "../renderer.hpp"
#include "../common/resource_array.hpp"
#include "../texture.hpp"
#include "../draw_list.hpp"
#include "../scene.hpp"
#include "surface_pipeline_wgpu.hpp"

namespace gfx
{

struct SurfaceViewData
{
    wgpu::SurfaceTexture surface_texture;
    wgpu::TextureView view;
};

struct DynamicBuffer
{
    wgpu::BufferUsage usage = wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer;
    size_t capacity = 0;
};

struct MeshWGPU
{
    MeshDescriptor desc{};
    DynamicBuffer vertex_buffer;
    DynamicBuffer index_buffer;
    bool has_data = false;
};

struct TextureWGPU
{
    TextureDescriptor desc{};
    wgpu::Texture texture;
    uint32_t mip_levels = 1;
    wgpu::TextureView view;
    wgpu::BindGroup gui_bind_group; // generated if used in HUD cmd
};

struct MaterialWGPU
{
    MaterialDescriptor desc{};
    wgpu::Sampler sampler;
    wgpu::Texture color_texture;
    wgpu::TextureView color_texture_view;
    wgpu::BindGroup bind_group;
};

struct SkeletonPoseWGPU
{
    uint32_t num_bones;
    wgpu::Buffer bones_buffer;
    wgpu::BindGroup bind_group;
};

struct DeformInfoWGPU
{
    glm::vec3 deform_min;
    float max_offset;
    glm::vec3 deform_max;
    float _pad0;
};

struct DeformTextureWGPU
{
    DeformTextureDescriptor desc{};
    wgpu::Texture texture;
    wgpu::TextureView view;
    wgpu::Buffer info_buffer;
    wgpu::BindGroup bind_group;
};

struct VertexBufferLayout
{
    std::vector<wgpu::VertexAttribute> attrs;
    wgpu::VertexBufferLayout layout{};
};

struct GlobalUniformData
{
    glm::mat4 view_proj;
    glm::vec3 ambient_color;
    float _pad0;
    glm::vec3 sun_color;
    float _pad1;
    glm::vec3 sun_direction;
    float _pad2;
    glm::vec3 camera_pos;
    float _pad3;
    glm::vec4 fog;
};

struct InstanceUniformData
{
    glm::mat4 matrix;
    std::array<uint32_t, 8> colors;
};

struct GlobalGUIUniformData
{
    glm::mat3x4 matrix;
};

class RendererWGPU : public Renderer
{
public:
    RendererWGPU(SDL_Window* window);

    virtual MeshID CreateMesh(const MeshDescriptor& desc) override;
    virtual void SetMeshVertexData(MeshID mesh_id, const MeshVertexData& data) override;
    virtual void SetMeshTriangleData(MeshID mesh_id, const MeshTriangleData& data) override;
    virtual void ReleaseMesh(MeshID mesh_id) override;

    virtual TextureID CreateTexture(const TextureDescriptor& desc) override;
    virtual void SetTextureData(TextureID texture_id, std::span<const uint8_t> data) override;
    virtual void ReleaseTexture(TextureID texture_id) override;

    virtual MaterialID CreateMaterial(const MaterialDescriptor& desc) override;
    virtual void ReleaseMaterial(MaterialID material_id) override;

    virtual SkeletonPoseID CreateSkeletonPose(const SkeletonPoseDescriptor& desc) override;
    virtual void SetSkeletonPoseTransforms(SkeletonPoseID pose_id, std::span<const glm::mat4> transforms) override;
    virtual void ReleaseSkeletonPose(SkeletonPoseID pose_id) override;

    virtual DeformTextureID CreateDeformTexture(const DeformTextureDescriptor& desc) override;
    virtual void SetDeformTextureData(DeformTextureID deform_id, std::span<const glm::i8vec3> data) override;
    virtual void ReleaseDeformTexture(DeformTextureID deform_id) override;

    virtual void Draw(Scene& scene, const CameraParams& camera) override;

    virtual ~RendererWGPU();

private:
    void InitWGPU();
    void CreateGlobalResources();
    void CreateMipmapPipeline();
    void CreateGuiPipeline();
    void ConfigureSurface();
    SurfaceViewData GetNextSurfaceViewData();
    bool ReserveBufferCapacity(DynamicBuffer& buffer, size_t capacity);
    void SetBufferData(DynamicBuffer& buffer, std::span<const uint8_t> data);
    TextureID GetWhiteTexture();
    wgpu::Sampler GetSampler(bool linear, bool mipmaps);
    wgpu::Sampler GetSamplerForTexture(const TextureWGPU& texture);
    void CreateTextureGuiBindGroup(TextureWGPU& texture);
    VertexBufferLayout GetVertexBufferLayout(MeshVertexAttributeFlags attrs);
    const wgpu::RenderPipeline& GetSurfacePipeline(SurfacePipelineFlags flags);
    void InvalidateSurfacePipelines();
    void CreateInstanceBufferBindGroup();
    void UpdateSettings();

    void RenderMainPass(Scene& scene, const CameraParams& camera);
    void DrawSurfaceList(wgpu::RenderPassEncoder& pass, std::span<DrawSurfaceCmd> queue, const DrawContext& ctx, const Environment& env);
    void DrawHudList(wgpu::RenderPassEncoder& pass, std::span<DrawHudCmd> queue, const DrawContext& ctx);

    void Unload();

private:
    // webgpu device stuff
    wgpu::Instance instance_;
    wgpu::Adapter adapter_;
    wgpu::Device device_;
    wgpu::Queue queue_;

    // mipmap rendering stuff
    wgpu::BindGroupLayout mipmap_bind_group_layout_;
    wgpu::RenderPipeline mipmap_pipeline_;

    // surface drawing resources
    wgpu::BindGroupLayout global_bind_group_layout_;
    wgpu::BindGroupLayout material_bind_group_layout_;
    wgpu::BindGroupLayout instance_bind_group_layout_;
    wgpu::BindGroupLayout skeletal_bind_group_layout_;
    wgpu::BindGroupLayout deform_bind_group_layout_;
    wgpu::Buffer global_buffer_;
    wgpu::BindGroup global_bind_group_;
    DynamicBuffer instance_buffer_;
    wgpu::BindGroup instance_bind_group_;
    std::map<SurfacePipelineFlags, wgpu::RenderPipeline> surface_pipelines_;
    std::map<SurfacePipelineFlags, wgpu::ShaderModule> surface_shaders_;

    // GUI drawing resources
    wgpu::BindGroupLayout gui_global_bind_group_layout_;
    wgpu::BindGroupLayout gui_texture_bind_group_layout_;
    wgpu::Buffer gui_global_buffer_;
    wgpu::BindGroup gui_global_bind_group_;
    wgpu::RenderPipeline gui_pipeline_;

    // surface
    glm::u32vec2 surface_size_{0};
    wgpu::TextureFormat surface_format_;
    wgpu::Surface surface_;
    wgpu::TextureFormat depth_format_;
    wgpu::Texture depth_texture_;
    wgpu::TextureView depth_texture_view_;
    wgpu::Texture color_texture_;
    wgpu::TextureView color_texture_view_;

    // resources
    ResourceArray<MeshWGPU> meshes_;
    ResourceArray<TextureWGPU> textures_;
    ResourceArray<MaterialWGPU> materials_;
    ResourceArray<SkeletonPoseWGPU> poses_;
    ResourceArray<DeformTextureWGPU> deforms_;

    // system resources
    std::shared_ptr<const Texture> white_tex_;

    // caches
    std::map<uint8_t, wgpu::Sampler> samplers_;

    // drawing
    DrawList main_dlist_;

    // settings
    uint32_t msaa_samples_ = 1;
};

} // namespace gfx
