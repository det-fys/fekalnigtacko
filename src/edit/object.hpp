#pragma once

#include <glm/glm.hpp>
#include <imgui.h>

#include "gfx/scene.hpp"
#include "utils/aabb.hpp"

namespace edit
{

class MapViewport;

struct DrawOverlayContext
{
    const MapViewport& viewport;
    ImDrawList& draw_list;

    DrawOverlayContext(const MapViewport& viewport, ImDrawList& draw_list) : viewport(viewport), draw_list(draw_list) {}
};

class Project;

class Object
{
public:
    Object(Project& project, const FarMatrix& trans);

    virtual void Draw(const gfx::DrawContext& ctx) {}
    virtual void DrawOverlay(const DrawOverlayContext& ctx);

    virtual void ShowProperties();

    void SetVisible(bool visible) { visible_ = visible; }
    bool IsVisible() const { return visible_; }

    void SetHovered(bool hovered) { hovered_ = hovered; }
    bool IsHovered() const { return hovered_; }

    virtual void SetSelected(bool selected) { selected_ = selected; }
    bool IsSelected() const { return selected_; }

    virtual void SetTransform(const FarMatrix& trans) { trans_ = trans; }
    const FarMatrix& GetTransform() const { return trans_; }

    virtual void Clone(const FarMatrix& trans) {}
    virtual void Link(Object& other, bool link) {}

    virtual void Delete() {}

    glm::dvec3 GetPosition() const { return trans_.position; }
    const AABB3& GetAABB() const { return aabb_; }

    Project& GetProject() const { return *project_; }

    virtual ~Object() = default;

protected:
    void SetAABB(const AABB3& aabb) { aabb_ = aabb; }

    void InvalidateChunks(const AABB3& aabb) const;

    static bool GetScreenPos(const edit::MapViewport& viewport, const glm::dvec3& world_pos, glm::vec2& out_screen_pos);
    static void DrawLineWs(const DrawOverlayContext& ctx, const glm::dvec3& p0, const glm::dvec3& p1, uint32_t color,
                           float thickness);

    static FarMatrix GetTranslationOnly(const FarMatrix& trans);

private:
    void DrawAABBOverlay(const DrawOverlayContext& ctx, uint32_t color, float width) const;

private:
    Project* project_ = nullptr;

    FarMatrix trans_{1.0};
    AABB3 aabb_;
    bool visible_ = false;
    bool hovered_ = false;
    bool selected_ = false;
};

} // namespace edit
