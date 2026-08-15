#pragma once

#include "viewport.hpp"

namespace im
{

class Viewport2D : public Viewport
{
public:
    Viewport2D(std::string title, glm::vec2& center_ws, float& zoom_ws_to_px);

    const glm::vec2& GetPosWsP0() const { return pos_ws_p0_; }
    const glm::vec2& GetPosWsP1() const { return pos_ws_p1_; }
    const glm::vec2& GetSizeWs() const { return size_ws_; }

    glm::vec2 WsToCanvas(const glm::vec2& pos_ws) const
    {
        glm::vec2 offset = pos_ws - pos_ws_p0_;
        offset.y = -offset.y; // Invert Y-axis
        return offset * zoom_ws_to_px_ + GetCanvasP0();
    }

    glm::vec2 CanvasToWs(const glm::vec2& pos) const
    {
        glm::vec2 offset = (pos - GetCanvasP0()) / zoom_ws_to_px_;
        offset.y = -offset.y; // Invert Y-axis
        return pos_ws_p0_ + offset;
    }

protected:
    virtual void Update();

private:
    // panning & zoom
    glm::vec2& center_ws_;
    float& zoom_ws_to_px_;

    glm::vec2 pos_ws_p0_{};
    glm::vec2 pos_ws_p1_{};
    glm::vec2 size_ws_{};
};

} // namespace im
