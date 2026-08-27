#pragma once

#include <cstdint>
#include <span>
#include <optional>

#include "object.hpp"

namespace edit
{

using WaypointID = uint32_t;

class Waypoint : public Object
{
public:
    using Super = Object;

    Waypoint(Project& project, const glm::mat4& trans, WaypointID id);

    virtual void DrawOverlay(const DrawOverlayContext& ctx) override;

    virtual void SetTransform(const glm::mat4& trans) override;
    virtual void Clone(const glm::mat4& trans) override;
    virtual void Link(Object& other, bool link) override;
    void Link(Waypoint& other, bool link);
    virtual void Delete() override;

private:
    void UpdateAABB();

    void DrawLinksOverlay(const DrawOverlayContext& ctx) const;
    static void GetPath(const Waypoint& wp0, const Waypoint& wp1, std::span<glm::vec3> out_positions);
    static void DrawPath(const DrawOverlayContext& ctx, const Waypoint& wp0, const Waypoint& wp1, uint32_t color0,
                         uint32_t color1);
    static void DrawPathTypeMarker(const DrawOverlayContext& ctx, std::span<const glm::vec3> positions,
                                   std::span<const std::optional<ImVec2>> positions_2d, bool from_end,
                                   uint32_t color, float length);

    void UnlinkAll();

    uint32_t FindLinkIndex(WaypointID other_id) const;
    uint32_t FindFreeLinkIndex() const;
    bool HasSideLinks() const { return links_[2] != 0 || links_[3] != 0; }
    void FixLinksToPrioritizeMainPath();

    WaypointID GetMainPathContinuation(WaypointID from_id) const;

private:
    WaypointID id_ = 0;

    // 0-1: main path
    // 2-3: side path
    std::array<WaypointID, 4> links_{0, 0, 0, 0};
};

} // namespace edit
