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

    bool IsActive() const { return active_; }
    bool IsHovered() const { return hovered_; }
    bool IsFocused() const { return focused_; }
    bool IsGizmoHovered() const { return gizmo_hovered_; }

    const glm::vec2& GetCanvasP0() const { return canvas_p0_; }
    const glm::vec2& GetCanvasSize() const { return canvas_sz_; }

protected:
    virtual void Update();

    virtual void DrawWindowLayout();
    virtual void Draw(ImDrawList& draw_list) {}

private:
    std::string title_;

    // frame state
    bool active_ = false;
    bool hovered_ = false;
    bool focused_ = false;
    bool gizmo_hovered_ = false;

    glm::vec2 canvas_p0_{};
    glm::vec2 canvas_sz_{};
};

} // namespace im
