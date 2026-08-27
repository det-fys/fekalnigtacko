#include "waypoint.hpp"

#include <array>
#include <cstring>

#include "map_project.hpp"
#include "map_viewport.hpp"
#include "utils/spline.hpp"

edit::Waypoint::Waypoint(Project& project, const glm::mat4& trans, WaypointID id) : Super(project, trans), id_(id)
{
    UpdateAABB();
}

void edit::Waypoint::DrawOverlay(const DrawOverlayContext& ctx)
{
    Super::DrawOverlay(ctx);

    DrawLinksOverlay(ctx);

    glm::vec2 pos;
    if (!GetScreenPos(ctx.viewport, GetPosition(), pos))
        return;

    ctx.draw_list.AddRectFilled(ImVec2(pos.x - 4, pos.y - 4), ImVec2(pos.x + 4, pos.y + 4), IM_COL32(255, 0, 255, 255));

    // draw num on hover
    if (IsHovered())
    {
        std::array<char, 16> buf;
        snprintf(buf.data(), buf.size(), "%u", id_);
        ctx.draw_list.AddText(ImVec2(pos.x + 10, pos.y - 15), IM_COL32(255, 255, 0, 255), buf.data());
    }
}

void edit::Waypoint::SetTransform(const glm::mat4& trans)
{
    Super::SetTransform(GetTranslationOnly(trans));
    UpdateAABB();
}

void edit::Waypoint::Clone(const glm::mat4& trans)
{
    auto& project = GetProject();
    auto my_id = id_;

    auto new_wp = project.AddWaypoint(trans);
    if (!new_wp)
        return;

    // this can get invalidated when inserting new waypoint
    auto new_me = project.GetWaypoint(my_id);
    if (!new_me)
        return;

    // link to the new waypoint
    new_me->Link(*new_wp, true);
}

void edit::Waypoint::Link(Object& other, bool link)
{
    auto other_wp = dynamic_cast<Waypoint*>(&other);
    if (!other_wp)
        return;

    Link(*other_wp, link);
}

void edit::Waypoint::Link(Waypoint& other, bool link)
{
    auto my_idx = FindLinkIndex(other.id_);
    auto other_idx = other.FindLinkIndex(id_);

    bool linked = (my_idx != UINT32_MAX && other_idx != UINT32_MAX);

    if (link == linked)
        return; // already in desired state

    if (link)
    {
        my_idx = FindFreeLinkIndex();
        other_idx = other.FindFreeLinkIndex();

        if (my_idx == UINT32_MAX || other_idx == UINT32_MAX)
            return; // no free link slots

        links_[my_idx] = other.id_;
        other.links_[other_idx] = id_;
    }
    else
    {
        links_[my_idx] = 0;
        other.links_[other_idx] = 0;
    
        FixLinksToPrioritizeMainPath();
        other.FixLinksToPrioritizeMainPath();
    }
}

void edit::Waypoint::Delete()
{
    UnlinkAll();
}

void edit::Waypoint::UpdateAABB()
{
    constexpr float half_extent = 1.5f;
    const auto& pos = GetPosition();
    SetAABB(AABB3(pos - half_extent, pos + half_extent));
}

void edit::Waypoint::DrawLinksOverlay(const DrawOverlayContext& ctx) const
{
    auto my_pos = GetPosition();

    for (uint32_t i = 0; i < links_.size(); ++i)
    {
        auto other_id = links_[i];
        if (other_id == 0)
            continue;

        auto other_wp = GetProject().GetWaypoint(other_id);
        if (!other_wp)
            continue; // ? 

        auto other_pos = other_wp->GetPosition();

        // if the target is visible, draw only to midpoint
        bool other_visible = ctx.viewport.GetFrustum().IsSphereVisible({other_pos, 0.1f});
        if (!other_visible || id_ < other_id)
        {
            constexpr uint32_t LINK_COLOR_MAIN = IM_COL32(0, 0, 255, 255);
            constexpr uint32_t LINK_COLOR_SIDE = IM_COL32(255, 0, 0, 255);

            auto color0 = HasSideLinks() ? ((i < 2) ? LINK_COLOR_MAIN : LINK_COLOR_SIDE) : 0;
            auto color1 =
                other_wp->HasSideLinks() ? (other_wp->FindLinkIndex(id_) < 2 ? LINK_COLOR_MAIN : LINK_COLOR_SIDE) : 0;

            DrawPath(ctx, *this, *other_wp, color0, color1);
        }
    }

}

void edit::Waypoint::GetPath(const Waypoint& wp0, const Waypoint& wp1, std::span<glm::vec3> out_positions)
{
    glm::vec3 b = wp0.GetPosition();
    glm::vec3 c = wp1.GetPosition();

    // get
    glm::vec3 a = b;
    auto wp_a_id = wp0.GetMainPathContinuation(wp1.id_);
    if (wp_a_id != 0)
    {
        auto wp_a = wp0.GetProject().GetWaypoint(wp_a_id);
        if (wp_a)
            a = wp_a->GetPosition();
    }

    glm::vec3 d = c;
    auto wp_d_id = wp1.GetMainPathContinuation(wp0.id_);
    if (wp_d_id != 0)
    {
        auto wp_d = wp1.GetProject().GetWaypoint(wp_d_id);
        if (wp_d)
            d = wp_d->GetPosition();
    }

    CatmullRomSpline spline(a, b, c, d);

    for (size_t i = 0; i < out_positions.size(); ++i)
    {
        auto t = static_cast<float>(i) / static_cast<float>(out_positions.size() - 1);
        out_positions[i] = spline.Get(t);
    }
}

void edit::Waypoint::DrawPath(const DrawOverlayContext& ctx, const Waypoint& wp0, const Waypoint& wp1, uint32_t color0,
                              uint32_t color1)
{
    constexpr uint32_t MAX_SEGMENTS = 16;
    
    std::array<glm::vec3, MAX_SEGMENTS + 1> positions;
    GetPath(wp0, wp1, positions);

    std::array<std::optional<ImVec2>, MAX_SEGMENTS + 1> positions_2d;

    const uint32_t num_segments = MAX_SEGMENTS;

    for (uint32_t i = 0; i < positions.size(); ++i)
    {
        glm::vec2 screen_pos;
        if (!GetScreenPos(ctx.viewport, positions[i], screen_pos))
            continue;

        positions_2d[i] = ImVec2(screen_pos.x, screen_pos.y);
    }

    for (uint32_t i = 0; i < num_segments; ++i)
    {
        if (!positions_2d[i].has_value() || !positions_2d[i + 1].has_value())
            continue;
        ctx.draw_list.AddLine(*positions_2d[i], *positions_2d[i + 1], IM_COL32(220, 220, 220, 255), 1.5f);

        // draw point
        //ctx.draw_list.AddCircleFilled(*positions_2d[i], 2.0f, IM_COL32(220, 220, 220, 255));
    }

    if (color0)
        DrawPathTypeMarker(ctx, positions, positions_2d, false, color0, 5.0f);
    
    if (color1)
        DrawPathTypeMarker(ctx, positions, positions_2d, true, color1, 5.0f);

    //// draw link type color from wp0
    //if (positions_2d[0] && positions_2d[1])
    //{
    //    ctx.draw_list.AddLine(*positions_2d[0], *positions_2d[1], color0, 3.0f);
    //}

    //// draw link type color from wp1
    //if (positions_2d[num_segments - 1] && positions_2d[num_segments])
    //{
    //    ctx.draw_list.AddLine(*positions_2d[num_segments - 1], *positions_2d[num_segments], color1, 3.0f);
    //}

}

void edit::Waypoint::DrawPathTypeMarker(const DrawOverlayContext& ctx, std::span<const glm::vec3> positions,
                                        std::span<const std::optional<ImVec2>> positions_2d, bool from_end,
                                        uint32_t color, float length)
{
    constexpr float WIDTH = 3.0f;

    // Guard against insufficient data or invalid target length
    if (positions.size() < 2 || positions_2d.size() < positions.size() || length <= 0.0f)
        return;

    int segments = static_cast<int>(positions.size()) - 1;
    int start = from_end ? segments : 0;
    int dir = from_end ? -1 : 1;

    float dist = 0.0f;

    for (int j = 0; j < segments; j++)
    {
        int i = start + j * dir;
        int next_i = i + dir;

        const auto& p0 = positions[i];
        const auto& p1 = positions[next_i];
        float segment_length = glm::length(p1 - p0);

        // Skip degenerate zero-length segments
        if (segment_length <= 0.0001f)
            continue;

        // Full segment fit within total length
        if (dist + segment_length < length)
        {
            dist += segment_length;

            if (positions_2d[i].has_value() && positions_2d[next_i].has_value())
            {
                ctx.draw_list.AddLine(*positions_2d[i], *positions_2d[next_i], color, WIDTH);
            }
        }
        // Partial segment: marker ends inside this segment
        else
        {
            float remaining_length = length - dist;
            float t = remaining_length / segment_length;

            glm::vec3 end_pos = glm::mix(p0, p1, t);
            glm::vec2 end_pos_2d;

            if (positions_2d[i].has_value() && GetScreenPos(ctx.viewport, end_pos, end_pos_2d))
            {
                ctx.draw_list.AddLine(*positions_2d[i], ImVec2(end_pos_2d.x, end_pos_2d.y), color, WIDTH);
            }

            // Target marker length reached
            break;
        }
    }
}

void edit::Waypoint::UnlinkAll()
{
    for (auto other_id : links_)
    {
        if (other_id == 0)
            continue;

        auto other_wp = GetProject().GetWaypoint(other_id);
        if (other_wp)
            Link(*other_wp, false);
    }
}

uint32_t edit::Waypoint::FindLinkIndex(WaypointID other_id) const
{
    for (uint32_t i = 0; i < links_.size(); ++i)
    {
        if (links_[i] == other_id)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

uint32_t edit::Waypoint::FindFreeLinkIndex() const
{
    return FindLinkIndex(0); // 0 is considered free
}

void edit::Waypoint::FixLinksToPrioritizeMainPath()
{
    bool main_path_full = links_[0] != 0 && links_[1] != 0;
    bool has_side_path = links_[2] != 0 || links_[3] != 0;

    if (main_path_full || !has_side_path)
        return; // nothing to do

    auto& main_path_entry = (links_[0] == 0) ? links_[0] : links_[1];
    auto& side_path_entry = (links_[2] != 0) ? links_[2] : links_[3];

    std::swap(main_path_entry, side_path_entry);
}

edit::WaypointID edit::Waypoint::GetMainPathContinuation(WaypointID from_id) const
{
    return (links_[0] == from_id) ? links_[1] : ((links_[1] == from_id) ? links_[0] : 0);
}
