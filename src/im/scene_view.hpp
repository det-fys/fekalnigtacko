#pragma once

#include "gfx/viewport.hpp"

#include <imgui.h>

namespace im
{

class SceneView
{
public:
    SceneView(gfx::Scene& scene);

    void Draw(ImDrawList& draw_list, ImVec2 p0, ImVec2 sz, const gfx::CameraParams& cam);

private:
    gfx::Scene& scene_;
    gfx::Viewport viewport_;
};

} // namespace im
