#pragma once

#include <webgpu/webgpu_cpp.h>

#include "../renderer.hpp"
#include "../common/resource_array.hpp"
#include "../texture.hpp"
#include "../material.hpp"
#include "../draw_list.hpp"
#include "../scene.hpp"
#include "surface_pipeline_wgpu.hpp"

namespace gfx
{

constexpr uint32_t MAX_CSM_CASCADES = 4;
constexpr uint32_t MAX_SPOTLIGHT_SHADOWMAPS = 8;
constexpr uint32_t MAX_PASSES = 2 + MAX_CSM_CASCADES + MAX_SPOTLIGHT_SHADOWMAPS;
constexpr uint32_t GLOBAL_BUFFER_STRIDE = 512;

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
    bool fully_opaque = true; // no alpha culling needed
};

struct MaterialWGPU
{
    MaterialDescriptor desc{};
    bool alpha_culling = false;
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
    glm::mat4 view;
    glm::mat4 view_proj;
    glm::vec3 ambient_color;
    float _pad0;
    glm::vec3 sun_color;
    float _pad1;
    glm::vec3 sun_direction;
    float csm_texel_size;
    glm::vec3 camera_pos;
    uint32_t csm_cascade_count = 0;
    glm::vec4 fog;
    std::array<float, MAX_CSM_CASCADES> csm_splits;
    std::array<glm::mat4, MAX_CSM_CASCADES> csm_matrices;
};

struct InstanceUniformData
{
    glm::mat4 matrix;
    std::array<uint32_t, 8> colors;
};

struct PreparedCmd
{
    const DrawSurfaceCmd* cmd = nullptr;
    MeshID mesh_id = 0;
    uint32_t tri_offset = 0;
    uint32_t tri_count = 0;
    MaterialID material_id = 0;
    float dist = 0.0f;
    SurfacePipelineFlags pflags = 0;
};

struct CSMCascadeData
{
    wgpu::TextureView texture_view;
    glm::mat4 proj;
    glm::mat4 view;
    glm::mat4 view_proj;
    DrawList dlist;
    std::vector<PreparedCmd> pcmds;
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
    void CreateMipmapResources();
    void CreateMipmapPipeline();
    void CreateShadowResources();
    void CreateSurfaceGlobalResources();
    void CreateSurfaceMaterialResources();
    void CreateSurfaceInstanceResources();
    void CreateDepthResources();
    void CreateGuiResources();
    void CreateGuiPipeline();
    void ConfigureSurface();
    SurfaceViewData GetNextSurfaceViewData();
    bool ReserveBufferCapacity(DynamicBuffer& buffer, size_t capacity);
    void SetBufferData(DynamicBuffer& buffer, std::span<const uint8_t> data);
    TextureID GetWhiteTexture();
    MaterialID GetDummyMaterial();
    wgpu::Sampler GetSampler(bool linear, bool mipmaps);
    wgpu::Sampler GetSamplerForTexture(const TextureWGPU& texture);
    void CreateTextureGuiBindGroup(TextureWGPU& texture);
    VertexBufferLayout GetVertexBufferLayout(MeshVertexAttributeFlags attrs);
    const wgpu::RenderPipeline& GetSurfacePipeline(SurfacePipelineFlags flags);
    void InvalidateSurfacePipelines();
    void CreateInstanceBufferBindGroup();
    void UpdateSettings();

    void Render(Scene& scene, const CameraParams& camera);
    void ComputeCSMMatrices(const glm::mat4& view, float fov, float aspect, const glm::vec3& sun_dir);
    void PrepareSurfaceCmds(std::span<DrawSurfaceCmd> cmds, std::vector<PreparedCmd>& pcmds, SurfacePipelineFlags pflags);
    void EncodePreparedCmds(wgpu::RenderPassEncoder& pass, std::span<PreparedCmd> pcmds,
                            const GlobalUniformData& globals, uint32_t globals_index, SurfacePipelineFlags pflags);
    void EncodeHudCmds(wgpu::RenderPassEncoder& pass, std::span<DrawHudCmd> queue, const DrawContext& ctx);

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

    // depth prepass / shadows / CSM
    wgpu::BindGroupLayout global_depth_bind_group_layout_; // does not contain shadowmap info
    wgpu::BindGroup global_depth_bind_group_;
    wgpu::Sampler shadow_sampler_;

    // CSM
    uint32_t csm_resolution_ = 1024;
    uint32_t csm_num_cascades_ = 3;
    wgpu::Texture csm_texture_;
    wgpu::TextureView csm_texture_view_;
    std::array<float, MAX_CSM_CASCADES> csm_splits_;
    std::array<CSMCascadeData, MAX_CSM_CASCADES> csm_cascades_;

    // temporary
    std::vector<InstanceUniformData> instances_;


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
    std::shared_ptr<const Material> dummy_material_;

    // caches
    std::map<uint8_t, wgpu::Sampler> samplers_;

    // drawing
    DrawList main_dlist_;
    std::vector<PreparedCmd> pcmds_prepass_;
    std::vector<PreparedCmd> pcmds_main_;

    // settings
    uint32_t msaa_samples_ = 1;
};

} // namespace gfx
