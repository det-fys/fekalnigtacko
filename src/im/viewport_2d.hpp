#pragma once

#include "viewport.hpp"

namespace im
{

class Viewport2D : public Viewport
{
public:
    Viewport2D(std::string title);

    const glm::vec2& GetPosWsMin() const { return pos_ws_min_; }
    const glm::vec2& GetPosWsMax() const { return pos_ws_max_; }
    const glm::vec2& GetSizeWs() const { return size_ws_; }

    glm::vec2 WsToCanvas(const glm::vec2& pos_ws) const { return (pos_ws - pos_ws_min_) * zoom_ws_to_px_ + GetCanvasP0(); }
    glm::vec2 CanvasToWs(const glm::vec2& pos) const { return (pos - GetCanvasP0()) / zoom_ws_to_px_ + pos_ws_min_; }

protected:
    virtual void Update();

private:
    // panning & zoom
    glm::vec2 center_ws_{};
    float zoom_ws_to_px_ = 1.0f;
    
    glm::vec2 pos_ws_min_{};
    glm::vec2 pos_ws_max_{};
    glm::vec2 size_ws_{};
};

}