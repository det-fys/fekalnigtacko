#pragma once

#include <cstdint>
#include <span>
#include <optional>

#include "object.hpp"

#include "mapgen/resources.hpp"

namespace edit
{

using WaypointID = uint32_t;

class Waypoint : public Object
{
public:
    using Super = Object;

    Waypoint(Project& project, const FarMatrix& trans, WaypointID id);

    virtual void DrawOverlay(const DrawOverlayContext& ctx) override;

    virtual void ShowProperties() override;

    virtual void SetTransform(const FarMatrix& trans) override;
    virtual void Clone(const FarMatrix& trans) override;
    virtual void Link(Object& other, bool link) override;
    void Link(Waypoint& other, bool link);
    void SetLink(uint32_t idx, WaypointID other_id);
    void SwapLinks(uint32_t i0, uint32_t i1);
    void UnlinkIndex(uint32_t idx);
    virtual void Delete() override;

    void SetMeshID(uint32_t mesh_id);
    uint32_t GetMeshID() const { return mesh_id_; }

    WaypointID GetID() const { return id_; }
    WaypointID GetLink(uint32_t index) const { return links_[index]; }
    std::span<const WaypointID> GetLinks() const { return links_; }

private:
    void UpdateAABB();

    void DrawLinksOverlay(const DrawOverlayContext& ctx) const;
    static void GetPath(const Waypoint& wp0, const Waypoint& wp1, std::span<glm::dvec3> out_positions);
    static void DrawPath(const DrawOverlayContext& ctx, const Waypoint& wp0, const Waypoint& wp1, uint32_t color0,
                         uint32_t color1);
    static void DrawPathTypeMarker(const DrawOverlayContext& ctx, std::span<const glm::dvec3> positions,
                                   std::span<const std::optional<ImVec2>> positions_2d, bool from_end, uint32_t color,
                                   float length);

    void UnlinkAll();

    uint32_t FindLinkIndex(WaypointID other_id) const;
    uint32_t FindFreeLinkIndex() const;
    bool HasSideLinks() const { return links_[2] != 0 || links_[3] != 0; }
    void FixLinksToPrioritizeMainPath();

    WaypointID GetMainPathContinuation(WaypointID from_id) const;

    void InvalidatePathsChunks() const;
    void InvalidatePathChunks(WaypointID next_id) const;

private:
    WaypointID id_ = 0;

    uint32_t mesh_id_ = mg::INVALID_RES_ID;

    // 0-1: main path
    // 2-3: side path
    std::array<WaypointID, 4> links_{0, 0, 0, 0};
};

} // namespace edit
