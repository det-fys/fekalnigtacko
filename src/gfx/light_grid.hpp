#pragma once

#include <vector>

#include "assets/skeleton.hpp"
#include "light_cache.hpp"
#include "surface.hpp"
#include "surface_render_flags.hpp"
#include "uniform_buffer.hpp"

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
};

struct DrawLightCmd
{
    LightData light;

    // point light
    DrawLightCmd(const glm::vec3& position, const glm::vec3& color, float radius)
    {
        light.position = position;
        light.color = color;
        light.radius = radius;

        light.dir = glm::vec3(0.0f);
        light.cos_inner = -1.0f;
        light.cos_outer = 0.0f;
    }

    // spot light
    DrawLightCmd(const glm::vec3& position, const glm::vec3& color, float radius, const glm::vec3& dir,
                 float inner_angle, float outer_angle)
    {
        light.position = position;
        light.color = color;
        light.radius = radius;

        light.dir = dir;
        light.cos_inner = glm::cos(inner_angle);
        light.cos_outer = glm::cos(outer_angle);
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
    std::vector<DrawBeamCmd> beams;
    std::vector<DrawHudCmd> huds;

    void AddSurface(const DrawSurfaceCmd& cmd) { surfaces.emplace_back(cmd); }

    void AddLight(const DrawLightCmd& cmd) { lights.emplace_back(cmd); }

    // light - point
    void AddLight(const glm::vec3& position, const glm::vec3& color, float radius)
    {
        lights.emplace_back(position, color, radius);
    }

    // light - spot
    void AddLight(const glm::vec3& position, const glm::vec3& color, float radius, const glm::vec3& dir, float inner_angle, float outer_angle)
    {
        lights.emplace_back(position, color, radius, dir, inner_angle, outer_angle);
    }

    void AddBeam(const DrawBeamCmd& cmd) { beams.emplace_back(cmd); }
    void AddBeam(const glm::vec3& start, const glm::vec3& end, uint32_t color = 0xFFFFFFFF, float radius = 0.1f,
                 size_t num_segments = 1, float max_offset = 0.0f)
    {
        beams.emplace_back(start, end, color, radius, num_segments, max_offset);
    }

    void AddHUD(const DrawHudCmd& cmd) { huds.emplace_back(cmd); }

    void Clear()
    {
        surfaces.clear();
        lights.clear();
        beams.clear();
        huds.clear();
    }
};

} // namespace gfx