#pragma once

#include <vector>

#include "assets/map.hpp"
#include "assets/skeleton.hpp"
#include "surface.hpp"

namespace gfx
{

struct DrawSurfaceCmd
{
    const Surface* surface;
    const glm::mat4* matrices; // model matrix, continues in array of matrices for skeletal meshes
    const glm::vec4* color;    // optional tint
    uint32_t first;            // first triangle index
    uint32_t count;            // num triangles
    float dist;                // distance to camera - for transparnt sorting
};

struct DrawList
{
    std::vector<DrawSurfaceCmd> surfaces;

    void AddSurface(const DrawSurfaceCmd& cmd) { surfaces.emplace_back(cmd); }

    void Clear() { surfaces.clear(); }
};

} // namespace gfx