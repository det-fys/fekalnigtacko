#pragma once

#include <map>
#include <memory>

#include <SDL.h>

#include "gl.hpp"
#include "../renderer.hpp"
#include "../environment.hpp"
#include "../draw_list.hpp"
#include "../light_data.hpp"
#include "../light_cell.hpp"
#include "surface_shader.hpp"
#include "surface_render_flags.hpp"
#include "buffer_object.hpp"
#include "vertex_array.hpp"
#include "../texture.hpp"
#include "../common/resource_array.hpp"
#include "texture_gl.hpp"
#include "mesh_gl.hpp"
#include "material_gl.hpp"
#include "skeleton_pose_gl.hpp"
#include "deform_texture_gl.hpp"
#include "shader_defs.hpp"
#include "../scene.hpp"

namespace gfx
{

struct DrawInfo
{
    const DrawContext& ctx;
    Environment env;

    DrawInfo(const DrawContext& ctx) : ctx(ctx) {}
};

constexpr static size_t LIGHT_GRID_CELL_LIGHTS = SD_MAX_LIGHTS;
using LightGridCell = LightArray<LIGHT_GRID_CELL_LIGHTS>;
using LightGrid = std::map<LightCellCoordHash, LightGridCell>;

class RendererGL : public Renderer
{
public:
    RendererGL(SDL_Window* window);

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

    virtual ~RendererGL() override;

private:
    void Load();
    void SetupShaders();
    void SetupBeamVA();
    void SetupCoronaVA();
    void Unload();

    void InvalidateShaders();

    SurfaceShader& GetSurfaceShader(SurfaceRenderFlags flags);
    void SetupSurfaceShader(SurfaceShader& sshader, const DrawInfo& info);
    void InvalidateSurfaceShader(SurfaceShader& sshader);

    void CreateLightGrid(std::span<DrawLightCmd> light_cmds, const DrawInfo& info);
    void AddLightToGrid(const LightData& light, LightGrid& grid, float cell_size);

    void DrawSurfaceList(std::span<DrawSurfaceCmd> list, const DrawInfo& info);
    void DrawBeamList(std::span<DrawBeamCmd> queue, const DrawInfo& info);
    void DrawCoronaList(std::span<DrawCoronaCmd> queue, const DrawInfo& info);
    void DrawHudList(std::span<DrawHudCmd> queue, const DrawInfo& info);

private:
    SDL_GLContext gl_context_ = nullptr;

    // resources
    ResourceArray<MeshGL> meshes_;
    ResourceArray<TextureGL> textures_;
    ResourceArray<MaterialGL> materials_;
    ResourceArray<SkeletonPoseGL> poses_;
    ResourceArray<DeformTextureGL> deform_textures_;

    bool loaded_ = false;

    std::map<SurfaceRenderFlags, SurfaceShader> surface_shaders_;
    std::unique_ptr<Shader> solid_shader_;

    std::unique_ptr<BufferObject> beam_segments_vbo_;
    std::unique_ptr<VertexArray> beam_va_;
    std::unique_ptr<Shader> beam_shader_;

    std::unique_ptr<VertexArray> corona_va_;
    std::shared_ptr<const Texture> corona_tex_;

    std::unique_ptr<Shader> hud_shader_;

    DrawList dlist_;

    const Shader* current_shader_ = nullptr;

    LightGrid light_grid_;
    float light_grid_size_ = 20.0f;
    LightGrid light_grid_chunks_;
    float light_grid_chunks_size_ = 1.0f;

};

}