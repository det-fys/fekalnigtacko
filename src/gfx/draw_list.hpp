#pragma once

#include <vector>

#include "assets/skeleton.hpp"
#include "light_cache.hpp"
#include "surface.hpp"
#include "surface_render_flags.hpp"
#include "uniform_buffer.hpp"
#include "light_cell.hpp"

namespace gfx
{

struct DrawSurfaceCmd
{
    const Surface* surface = nullptr;
    const glm::mat4* matrices = nullptr;                // model matrix
    const glm::vec4* color = nullptr;                   // optional tint
    const UniformBuffer<glm::mat4>* skinning = nullptr; // skinning matrices for skeletal meshes
    uint32_t first = 0;                                 // first triangle index
    uint32_t count = 0;                                 // num triangles
    float dist = 0.0f;                                  // distance to camera - for transparnt sorting
    SurfaceRenderFlags rflags = 0;
    uint8_t num_colors = 0;                             // >0 for multicolor, requires array of colors in "color"
    LightCellCoordHash map_chunk_hash = 0;              // 0 for regular objects, >0 for map chunk meshes
};

struct DrawLightCmd
{
    LightData light;
    float dist2 = 0.0f;

    DrawLightCmd(const glm::vec3& position, const glm::vec3& color, float radius)
    {
        light.position = position;
        light.color = color;
        light.radius = radius;

        light.dir = glm::vec3(0.0f);
        light.cos_inner = -1.0f;
        light.cos_outer = 0.0f;
    }

    DrawLightCmd(const glm::vec3& position, const glm::vec3& color, float radius, const glm::vec3& dir, float angle_inner, float angle_outer)
    {
        light.position = position;
        light.color = color;
        light.radius = radius;

        light.dir = dir;
        light.cos_inner = glm::cos(angle_inner);
        light.cos_outer = glm::cos(angle_outer);
    }
};

struct DrawCoronaCmd
{
    glm::vec3 pos;
    glm::vec3 dir;
    glm::vec3 color;
    float size;

    DrawCoronaCmd(const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& color, float size)
        : pos(pos), dir(dir), color(color), size(size)
    {
    }
};

struct DrawBeamCmd
{
    glm::vec3 start;
    glm::vec3 end;
    uint32_t color;
    float radius;
    size_t num_segments;
    float max_offset;

    DrawBeamCmd(const glm::vec3& start, const glm::vec3& end, uint32_t color, float radius, size_t num_segments,
                float max_offset)
        : start(start), end(end), color(color), radius(radius), num_segments(num_segments), max_offset(max_offset)
    {
    }
};

struct DrawHudCmd
{
    const VertexArray* va = nullptr;
    const Texture* texture = nullptr;
    size_t first = 0;
    size_t count = 0;
    const glm::mat3* matrix = nullptr;
};

struct DrawList
{
    std::vector<DrawSurfaceCmd> surfaces;
    std::vector<DrawLightCmd> lights;
    std::vector<DrawCoronaCmd> coronas;
    std::vector<DrawBeamCmd> beams;
    std::vector<DrawHudCmd> huds;
    float chunk_size = 1.0f;

    void AddSurface(const DrawSurfaceCmd& cmd) { surfaces.emplace_back(cmd); }

    void AddLight(const DrawLightCmd& cmd) { lights.emplace_back(cmd); }
    void AddLight(const glm::vec3& position, const glm::vec3& color, float radius)
    {
        lights.emplace_back(position, color, radius);
    }

    void AddSpotLight(const glm::vec3& position, const glm::vec3& color, float radius, const glm::vec3& dir,
                      float angle_inner, float angle_outer)
    {
        lights.emplace_back(position, color, radius, dir, angle_inner, angle_outer);
    }

    void AddCorona(const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& color, float size)
    {
        coronas.emplace_back(pos, dir, color, size);
    }

    void AddBeam(const DrawBeamCmd& cmd) { beams.emplace_back(cmd); }
    void AddBeam(const glm::vec3& start, const glm::vec3& end, uint32_t color = 0xFFFFFFFF, float radius = 0.1f,
                 size_t num_segments = 1, float max_offset = 0.0f)
    {
        beams.emplace_back(start, end, color, radius, num_segments, max_offset);
    }

    void AddHUD(const DrawHudCmd& cmd) { huds.emplace_back(cmd); }

    void SetMapChunkSize(float size) { chunk_size = size; }

    void Clear()
    {
        surfaces.clear();
        lights.clear();
        coronas.clear();
        beams.clear();
        huds.clear();
        chunk_size = 1.0f;
    }
};

} // namespace gfx