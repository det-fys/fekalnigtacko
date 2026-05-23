#pragma once

#include <vector>

#include "assets/skeleton.hpp"
#include "surface.hpp"
#include "uniform_buffer.hpp"
#include "surface_render_flags.hpp"

namespace gfx
{

struct DrawSurfaceCmd
{
    const Surface* surface = nullptr;
    const glm::mat4* matrices = nullptr; // model matrix
    const glm::vec4* color = nullptr;    // optional tint
    uint32_t first = 0;                  // first triangle index
    uint32_t count = 0;                  // num triangles
    float dist = 0.0f;                   // distance to camera - for transparnt sorting
    const UniformBuffer<glm::mat4>* skinning = nullptr; // skinning matrices for skeletal meshes
    SurfaceRenderFlags rflags = 0;
};

struct DrawBeamCmd
{
    glm::vec3 start;
    glm::vec3 end;
    uint32_t color;
    float radius;
    size_t num_segments;
    float max_offset;

    DrawBeamCmd(const glm::vec3& start, const glm::vec3& end, uint32_t color, float radius,
                size_t num_segments, float max_offset)
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
};

struct DrawList
{
    std::vector<DrawSurfaceCmd> surfaces;
    std::vector<DrawBeamCmd> beams;
    std::vector<DrawHudCmd> huds;

    void AddSurface(const DrawSurfaceCmd& cmd) { surfaces.emplace_back(cmd); }

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
        beams.clear();
        huds.clear();
    }
};

} // namespace gfx