#pragma once

#include "gfx/draw_list.hpp"
#include "gfx/frustum.hpp"

namespace game::view
{
struct DrawArgs
{
    gfx::DrawList& dlist;
    const glm::mat4& view_proj;
    gfx::Frustum frustum;
    glm::ivec2 screen_size;

    DrawArgs(gfx::DrawList& dlist, const glm::mat4& view_proj, glm::ivec2 screen_size)
        : dlist(dlist), view_proj(view_proj), frustum(view_proj), screen_size(screen_size)
    {
    }
};
} // namespace game::view