#include "viewport_2d.hpp"
#include "utils.hpp"

im::Viewport2D::Viewport2D(std::string title, glm::vec2& center_ws, float& zoom_ws_to_px)
    : Viewport(std::move(title)), center_ws_(center_ws), zoom_ws_to_px_(zoom_ws_to_px)
{
}

void im::Viewport2D::Update()
{
    auto& io = ImGui::GetIO();

    const glm::vec2 mouse_pos_in_canvas = VecFromImGui(io.MousePos) - GetCanvasP0();

    // zooming
    if (IsHovered() && io.MouseWheel != 0.0f)
    {
        glm::vec2 canvas_center_offset = mouse_pos_in_canvas - (GetCanvasSize() * 0.5f);

        // Invert Y offset for world space shift calculations
        glm::vec2 ws_center_offset = canvas_center_offset;
        ws_center_offset.y = -ws_center_offset.y;

        float old_zoom = zoom_ws_to_px_;
        zoom_ws_to_px_ *= glm::pow(1.25f, io.MouseWheel);
        center_ws_ += ws_center_offset * (1.0f / old_zoom - 1.0f / zoom_ws_to_px_);
    }

    // panning
    if (IsActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        glm::vec2 drag_delta = VecFromImGui(io.MouseDelta) / zoom_ws_to_px_;
        drag_delta.y = -drag_delta.y; // Invert Y dragging
        center_ws_ -= drag_delta;
    }

    // calc ws coords
    size_ws_ = GetCanvasSize() / zoom_ws_to_px_;

    // p0 is top-left canvas corner -> (-X, +Y) in world space relative to center
    pos_ws_p0_ = center_ws_ + glm::vec2(-size_ws_.x, size_ws_.y) * 0.5f;
    // p1 is bottom-right canvas corner -> (+X, -Y) in world space relative to center
    pos_ws_p1_ = center_ws_ + glm::vec2(size_ws_.x, -size_ws_.y) * 0.5f;
}
