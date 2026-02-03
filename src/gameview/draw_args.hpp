#pragma once

#include "gfx/draw_list.hpp"
#include "gfx/frustum.hpp"

namespace game::view
{

struct DrawArgs
{
    gfx::DrawList& dlist;
    
    const glm::mat4 view_proj;
    const glm::vec3 eye;
    const gfx::Frustum frustum;
    const glm::ivec2 screen_size;
    const float render_distance;

    DrawArgs(gfx::DrawList& dlist, const glm::mat4& view_proj, const glm::vec3& eye, const glm::ivec2& screen_size, float render_distance)
        : dlist(dlist), view_proj(view_proj), eye(eye), frustum(view_proj), screen_size(screen_size), render_distance(render_distance)
    {
    }
};
} // namespace game::view