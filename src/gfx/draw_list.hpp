#pragma once

#include <vector>

#include "assets/skeleton.hpp"
#include "surface.hpp"

namespace gfx
{

struct DrawSurfaceCmd
{
    const Surface* surface = nullptr;
    const glm::mat4* matrices = nullptr; // model matrix, continues in array of matrices for skeletal meshes
    const glm::vec4* color = nullptr;    // optional tint
    uint32_t first = 0;                  // first triangle index
    uint32_t count = 0;                  // num triangles
    float dist = 0.0f;                   // distance to camera - for transparnt sorting
};

struct DrawList
{
    std::vector<DrawSurfaceCmd> surfaces;

    void AddSurface(const DrawSurfaceCmd& cmd) { surfaces.emplace_back(cmd); }

    void Clear() { surfaces.clear(); }
};

} // namespace gfx