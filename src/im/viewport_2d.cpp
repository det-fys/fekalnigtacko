#include "viewport_2d.hpp"

#include "utils.hpp"

im::Viewport2D::Viewport2D(std::string title) : Viewport(std::move(title)) {}

void im::Viewport2D::Update()
{
    auto& io = ImGui::GetIO();

    const glm::vec2 mouse_pos_in_canvas = VecFromImGui(io.MousePos) - GetCanvasP0();

    // zooming
    if (IsHovered() && io.MouseWheel != 0.0f)
    {
        glm::vec2 canvas_center_offset = mouse_pos_in_canvas - (GetCanvasSize() * 0.5f);
        float old_zoom = zoom_ws_to_px_;
        zoom_ws_to_px_ *= glm::pow(1.25f, io.MouseWheel);
        center_ws_ += canvas_center_offset * (1.0f / old_zoom - 1.0f / zoom_ws_to_px_);
    }

    // const auto mouse_pos_ws = CanvasToWs(mouse_pos_in_canvas);

    // panning
    if (IsActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        center_ws_ -= VecFromImGui(io.MouseDelta) / zoom_ws_to_px_;
    }

    // calc ws coords
    size_ws_ = GetCanvasSize() / zoom_ws_to_px_;
    pos_ws_min_ = center_ws_ - size_ws_ * 0.5f;
    pos_ws_max_ = pos_ws_min_ + size_ws_;

}
