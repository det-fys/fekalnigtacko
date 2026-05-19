#pragma once

#include "gfx/draw_list.hpp"
#include "gfx/renderer.hpp"
#include "gfx/frustum.hpp"
#include "gui/context.hpp"

namespace game::view
{

struct DrawArgs
{
    gfx::DrawList& dlist;
    gfx::DrawListEnvironmentParams& env;
    gui::Context& gui;

    const glm::mat4 view_proj;
    const glm::vec3 eye;
    const gfx::Frustum frustum;
    const glm::ivec2 screen_size;
    const float farplane;
    const float render_distance;

    DrawArgs(gfx::DrawList& dlist, gfx::DrawListEnvironmentParams& env, gui::Context& gui, const glm::mat4& view_proj,
             const glm::vec3& eye, const glm::ivec2& screen_size, float farplane, float render_distance)
        : dlist(dlist), env(env), gui(gui), view_proj(view_proj), eye(eye), frustum(view_proj),
          screen_size(screen_size), farplane(farplane), render_distance(render_distance)
    {
    }
};
} // namespace game::view