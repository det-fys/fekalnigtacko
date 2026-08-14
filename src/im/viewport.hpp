#pragma once

#include <string>

#include <glm/glm.hpp>
#include <imgui.h>

namespace im
{

class Viewport
{
public:
    Viewport(std::string title);

    void Show(bool* open);

protected:
    virtual void Update();

    virtual void DrawWindowLayout();
    virtual void Draw(ImDrawList& draw_list) {}

    bool IsActive() const { return active_; }
    bool IsHovered() const { return hovered_; }
    bool IsFocused() const { return focused_; }

    const glm::vec2& GetCanvasP0() const { return canvas_p0_; }
    const glm::vec2& GetCanvasSize() const { return canvas_sz_; }

private:
    std::string title_;

    // frame state
    bool active_ = false;
    bool hovered_ = false;
    bool focused_ = false;

    glm::vec2 canvas_p0_{};
    glm::vec2 canvas_sz_{};
};

} // namespace im
