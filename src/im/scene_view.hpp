#pragma once

#include "gfx/viewport.hpp"

#include <imgui.h>

namespace im
{

class SceneView
{
public:
    SceneView() = default;

    void Draw(ImDrawList& draw_list, ImVec2 p0, ImVec2 sz, gfx::Scene& scene, const gfx::CameraParams& cam);

private:
    gfx::Viewport viewport_;
};

} // namespace im
