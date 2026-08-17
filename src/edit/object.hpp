#pragma once

#include <glm/glm.hpp>
#include <imgui.h>

#include "gfx/scene.hpp"

namespace edit
{

class MapViewport;

struct DrawOverlayContext
{
    const MapViewport& viewport;
    ImDrawList& draw_list;

    DrawOverlayContext(const MapViewport& viewport, ImDrawList& draw_list) : viewport(viewport), draw_list(draw_list) {}
};

class Object
{
public:
    Object() = default;

    virtual void Draw(const gfx::DrawContext& ctx) {}
    virtual void DrawOverlay(const DrawOverlayContext& ctx) {}

    void SetVisible(bool visible) { visible_ = visible; }
    bool IsVisible() const { return visible_; }

    virtual void SetSelected(bool selected) { selected_ = selected; }
    bool IsSelected() const { return selected_; }

    virtual void SetTransform(const glm::mat4& trans) { trans_ = trans; }
    const glm::mat4& GetTransform() const { return trans_; }

    glm::vec3 GetPosition() const { return trans_[3]; }

    virtual ~Object() = default;

private:
    glm::mat4 trans_{1.0f};
    bool visible_ = false;
    bool selected_ = false;
};

} // namespace edit
