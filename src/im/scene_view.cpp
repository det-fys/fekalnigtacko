#include "scene_view.hpp"

void im::SceneView::Draw(ImDrawList& draw_list, ImVec2 p0, ImVec2 sz, gfx::Scene& scene, const gfx::CameraParams& cam)
{
    glm::u32vec2 size(sz.x, sz.y);
    viewport_.Draw(scene, cam, size);

    auto p1 = ImVec2(p0.x + sz.x, p0.y + sz.y);

    auto native_handle = viewport_.GetNativeHandle();
    if (native_handle)
    {
        int y0 = gfx::Viewport::NeedsYFlip() ? 1 : 0;
        draw_list.AddImage(reinterpret_cast<ImTextureID>(native_handle), p0, p1, ImVec2(0, y0), ImVec2(1, 1 - y0));
    }
}
