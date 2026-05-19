#pragma once

#include <memory>
#include <span>

#include "draw_list.hpp"
#include "shader.hpp"

namespace gfx
{

struct DrawListEnvironmentParams
{
    glm::vec3 clear_color;
    glm::vec3 ambient_light;
    glm::vec3 sun_color;
    glm::vec3 sun_direction;
    glm::vec4 fog; // alpha = distance
};

struct DrawListParams
{
    DrawListEnvironmentParams env;
    glm::vec3 cam_pos;
    glm::mat4 view_proj;
    size_t screen_width = 0;
    size_t screen_height = 0;
};

struct MeshShader
{
    std::unique_ptr<Shader> shader;

    // cached state to avoid redundant uniform updates which are expensive especially on WebGL
    bool global_setup = false;
    glm::vec4 color = glm::vec4(-1.0f); // invalid to force initial setup
    int flags = 0;
};

class Renderer
{
public:
    Renderer();
    void DrawList(gfx::DrawList& list, const DrawListParams& params);

private:
    void SetupBeamVA();

    void InvalidateShaders();
    void InvalidateMeshShader(MeshShader& mshader);
    void SetupMeshShader(MeshShader& mshader, const DrawListParams& params);

    void DrawSurfaceList(std::span<DrawSurfaceCmd> queue, const DrawListParams& params);
    void DrawBeamList(std::span<DrawBeamCmd> queue, const DrawListParams& params);
    void DrawHudList(std::span<DrawHudCmd> queue, const DrawListParams& params);

private:
    MeshShader mesh_shader_;
    MeshShader skel_mesh_shader_;
    MeshShader deform_mesh_shader_;
    std::unique_ptr<Shader> solid_shader_;

    std::unique_ptr<BufferObject> beam_segments_vbo_;
    std::unique_ptr<VertexArray> beam_va_;
    std::unique_ptr<Shader> beam_shader_;

    std::unique_ptr<Shader> hud_shader_;

    const Shader* current_shader_ = nullptr;
};

} // namespace gfx