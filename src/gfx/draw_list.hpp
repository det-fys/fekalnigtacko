#pragma once

#include <vector>

#include "assets/skeleton.hpp"
#include "surface.hpp"
#include "hud.hpp"

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

struct DrawHudCmd
{
    const VertexArray* va = nullptr;
    const Texture* texture = nullptr;
    const HudPosition* pos = nullptr;
    const glm::vec4* color = nullptr;
};

struct DrawList
{
    std::vector<DrawSurfaceCmd> surfaces;
    std::vector<DrawHudCmd> huds;

    void AddSurface(const DrawSurfaceCmd& cmd) { surfaces.emplace_back(cmd); }
    void AddHUD(const DrawHudCmd& cmd) { huds.emplace_back(cmd); }

    void Clear()
    {
        surfaces.clear();
        huds.clear();
    }
};

} // namespace gfx