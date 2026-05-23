#pragma once

#include <memory>
#include <span>

#include "draw_list.hpp"
#include "shader.hpp"
#include "surface_shader.hpp"

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

struct SurfaceShader
{
    std::unique_ptr<Shader> shader;
    SurfaceShaderInputFlags iflags;

    // cached state to avoid redundant uniform updates which are expensive especially on WebGL
    bool global_setup = false;
    glm::vec4 color = glm::vec4(-1.0f); // invalid to force initial setup
};

class Renderer
{
public:
    Renderer();
    void DrawList(gfx::DrawList& list, const DrawListParams& params);

private:
    void SetupBeamVA();

    void InvalidateShaders();

    SurfaceShader& GetSurfaceShader(SurfaceRenderFlags flags);
    void SetupSurfaceShader(SurfaceShader& sshader, const DrawListParams& params);
    void InvalidateSurfaceShader(SurfaceShader& sshader);

    void DrawSurfaceList(std::span<DrawSurfaceCmd> queue, const DrawListParams& params);
    void DrawBeamList(std::span<DrawBeamCmd> queue, const DrawListParams& params);
    void DrawHudList(std::span<DrawHudCmd> queue, const DrawListParams& params);

private:
    std::map<SurfaceRenderFlags, SurfaceShader> surface_shaders_;
    std::unique_ptr<Shader> solid_shader_;

    std::unique_ptr<BufferObject> beam_segments_vbo_;
    std::unique_ptr<VertexArray> beam_va_;
    std::unique_ptr<Shader> beam_shader_;

    std::unique_ptr<Shader> hud_shader_;

    const Shader* current_shader_ = nullptr;
};

} // namespace gfx