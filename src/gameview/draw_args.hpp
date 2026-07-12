#pragma once

#include "gfx/draw_list.hpp"
#include "gfx/frustum.hpp"
#include "gfx/renderer.hpp"
#include "gfx/scene.hpp"
#include "gui/context.hpp"

namespace game::view
{

struct DrawArgs
{
    const gfx::DrawContext& ctx;
    gui::Context& gui;

    DrawArgs(const gfx::DrawContext& ctx, gui::Context& gui) : ctx(ctx), gui(gui) {}
};

} // namespace game::view
