#pragma once

#include <glm/glm.hpp>

#include "viewport.hpp"

namespace im
{

class Viewport2DControls
{
public:
    Viewport2DControls(Viewport& viewport, glm::dvec2& center_ws, float& zoom_ws_to_px);

    void Update();

    const glm::dvec2& GetPosWsP0() const { return pos_ws_p0_; }
    const glm::dvec2& GetPosWsP1() const { return pos_ws_p1_; }
    const glm::dvec2& GetSizeWs() const { return size_ws_; }

    glm::vec2 WsToCanvas(const glm::dvec2& pos_ws) const
    {
        auto offset = glm::vec2(pos_ws - pos_ws_p0_);
        offset.y = -offset.y; // Invert Y-axis
        return offset * zoom_ws_to_px_ + viewport_.GetCanvasP0();
    }

    glm::dvec2 CanvasToWs(const glm::vec2& pos) const
    {
        auto offset = (pos - viewport_.GetCanvasP0()) / zoom_ws_to_px_;
        offset.y = -offset.y; // Invert Y-axis
        return pos_ws_p0_ + glm::dvec2(offset);
    }

    const glm::dvec2& GetMousePosWs() const { return mouse_pos_ws_; }

private:
    Viewport& viewport_;

    // panning & zoom
    glm::dvec2& center_ws_;
    float& zoom_ws_to_px_;

    glm::dvec2 pos_ws_p0_{0.0f};
    glm::dvec2 pos_ws_p1_{0.0f};
    glm::dvec2 size_ws_{0.0f};

    glm::dvec2 mouse_pos_ws_{0.0f};
};

} // namespace im