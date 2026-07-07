#pragma once

#include <memory>
#include <span>

#include "draw_list.hpp"
#include "shader.hpp"
#include "surface_shader.hpp"
#include "light_cell.hpp"

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
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 view_proj;
    size_t screen_width = 0;
    size_t screen_height = 0;
    //float map_chunk_size = 1.0f;
};

struct SurfaceShader
{
    std::unique_ptr<Shader> shader;
    SurfaceShaderInputFlags iflags = 0;

    // cached state to avoid redundant uniform updates which are expensive especially on WebGL
    bool global_setup = false;
    const glm::vec4* color = nullptr;
    size_t num_lights = 0;
    bool prev_decal = false;
};

constexpr static size_t LIGHT_GRID_CELL_LIGHTS = SD_MAX_LIGHTS;
using LightGridCell = LightArray<LIGHT_GRID_CELL_LIGHTS>;
using LightGrid = std::map<LightCellCoordHash, LightGridCell>;

class Renderer
{
public:
    Renderer();
    void DrawList(gfx::DrawList& list, const DrawListParams& params);

private:
    void SetupBeamVA();
    void SetupCoronaVA();

    void InvalidateShaders();

    SurfaceShader& GetSurfaceShader(SurfaceRenderFlags flags);
    void SetupSurfaceShader(SurfaceShader& sshader, const DrawListParams& params);
    void InvalidateSurfaceShader(SurfaceShader& sshader);

    void CreateLightGrid(std::span<DrawLightCmd> lights, const DrawListParams& params);
    void AddLightToGrid(const LightData& light, LightGrid& grid, float cell_size);

    void DrawSurfaceList(std::span<DrawSurfaceCmd> queue, const DrawListParams& params);
    void DrawBeamList(std::span<DrawBeamCmd> queue, const DrawListParams& params);
    void DrawCoronaList(std::span<DrawCoronaCmd> queue, const DrawListParams& params);
    void DrawHudList(std::span<DrawHudCmd> queue, const DrawListParams& params);

private:
    std::map<SurfaceRenderFlags, SurfaceShader> surface_shaders_;
    std::unique_ptr<Shader> solid_shader_;

    std::unique_ptr<BufferObject> beam_segments_vbo_;
    std::unique_ptr<VertexArray> beam_va_;
    std::unique_ptr<Shader> beam_shader_;

    std::unique_ptr<VertexArray> corona_va_;
    std::shared_ptr<const Texture> corona_tex_;

    std::unique_ptr<Shader> hud_shader_;

    const Shader* current_shader_ = nullptr;

    LightGrid light_grid_;
    float light_grid_size_ = 20.0f;
    LightGrid light_grid_chunks_;
    float light_grid_chunks_size_ = 1.0f;

    size_t frame_ = 0;
};

} // namespace gfx