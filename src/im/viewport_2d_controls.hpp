#pragma once

#include <glm/glm.hpp>

#include "viewport.hpp"

namespace im
{

class Viewport2DControls
{
public:
    Viewport2DControls(Viewport& viewport, glm::vec2& center_ws, float& zoom_ws_to_px);

    void Update();

    const glm::vec2& GetPosWsP0() const { return pos_ws_p0_; }
    const glm::vec2& GetPosWsP1() const { return pos_ws_p1_; }
    const glm::vec2& GetSizeWs() const { return size_ws_; }

    glm::vec2 WsToCanvas(const glm::vec2& pos_ws) const
    {
        glm::vec2 offset = pos_ws - pos_ws_p0_;
        offset.y = -offset.y; // Invert Y-axis
        return offset * zoom_ws_to_px_ + viewport_.GetCanvasP0();
    }

    glm::vec2 CanvasToWs(const glm::vec2& pos) const
    {
        glm::vec2 offset = (pos - viewport_.GetCanvasP0()) / zoom_ws_to_px_;
        offset.y = -offset.y; // Invert Y-axis
        return pos_ws_p0_ + offset;
    }

    const glm::vec2& GetMousePosWs() const { return mouse_pos_ws_; }

private:
    Viewport& viewport_;

    // panning & zoom
    glm::vec2& center_ws_;
    float& zoom_ws_to_px_;

    glm::vec2 pos_ws_p0_{0.0f};
    glm::vec2 pos_ws_p1_{0.0f};
    glm::vec2 size_ws_{0.0f};

    glm::vec2 mouse_pos_ws_{0.0f};
};

} // namespace im