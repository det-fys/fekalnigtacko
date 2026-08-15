#include "map_viewport_2d.hpp"

#include "im/utils.hpp"

edit::MapViewport2D::MapViewport2D(MapEditContext& context)
    : Super("2D viewport", context.cam_pos_2d, context.zoom_2d), context_(context)
{
}

static gfx::CameraParams GetCameraParams(const glm::vec2& p0, const glm::vec2& p1, float fov)
{
    gfx::CameraParams cam{};
    glm::vec2 center = (p0 + p1) * 0.5f;

    float height = std::abs(p1.y - p0.y);
    float half_fov_radians = glm::radians(fov * 0.5f);
    float distance = (height * 0.5f) / std::tan(half_fov_radians);
    cam.eye = glm::vec3(center.x, center.y, distance);

    cam.dir = glm::vec3(0.0f, 0.0f, -1.0f);
    cam.up = glm::vec3(0.0f, 1.0f, 0.0f);
    cam.fov = fov * 2.0f; // TODO: fix incorrect fov calculation in renderers

    return cam;
}

void edit::MapViewport2D::Draw(ImDrawList& draw_list)
{
    if (!context_.project)
    {
        return;
    }

    auto cam = GetCameraParams(GetPosWsP0(), GetPosWsP1(), context_.cam_fov_2d);
    scene_view_.Draw(draw_list, im::VecToImGui(GetCanvasP0()), im::VecToImGui(GetCanvasSize()), *context_.project, cam);

    // draw rect at world space (0, 0) to (100, 100)
    draw_list.AddRect(im::VecToImGui(WsToCanvas(glm::vec2(0.0f, 0.0f))),
                      im::VecToImGui(WsToCanvas(glm::vec2(100.0f, 100.0f))),
                      IM_COL32(255, 0, 0, 255));

    // draw circles at corners to verify
    draw_list.AddCircle(im::VecToImGui(WsToCanvas(GetPosWsP0())), 5.0f, IM_COL32(0, 255, 0, 255));
    draw_list.AddCircle(im::VecToImGui(WsToCanvas(GetPosWsP1())), 5.0f, IM_COL32(0, 255, 0, 255));

}
