#include "object.hpp"

#include "map_project.hpp"
#include "map_viewport.hpp"
#include "im/utils.hpp"

edit::Object::Object(Project& project, const glm::mat4& trans) : project_(&project), trans_(trans) {}

void edit::Object::DrawOverlay(const DrawOverlayContext& ctx)
{
    if (IsSelected() || IsHovered())
    {
        uint32_t color = IsSelected() ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 255, 255, 128);
        float width = IsHovered() ? 3.0f : 1.5f;
        DrawAABBOverlay(ctx, color, width);
    }
}

void edit::Object::InvalidateChunks(const AABB3& aabb)
{
    auto& map_cfg = project_->GetMapConfig();
    auto [min_chunk, max_chunk] = mg::GetChunkRange(map_cfg, AABB2(aabb.min, aabb.max), true);

    for (int chunk_y = min_chunk.y; chunk_y <= max_chunk.y; ++chunk_y)
    {
        for (int chunk_x = min_chunk.x; chunk_x <= max_chunk.x; ++chunk_x)
        {
            project_->InvalidateChunk(glm::ivec2(chunk_x, chunk_y));
        }
    }
}

bool edit::Object::GetScreenPos(const edit::MapViewport& viewport, const glm::vec3& world_pos,
                                glm::vec2& out_screen_pos)
{
    glm::vec4 clip_pos = viewport.GetViewProj() * glm::vec4(world_pos, 1.0f);
    if (clip_pos.w == 0.0f)
        return false;

    glm::vec3 ndc_pos = glm::vec3(clip_pos) / clip_pos.w;
    if (ndc_pos.z < -1.0f || ndc_pos.z > 1.0f)
        return false; // behind camera

    glm::vec2 anchor = ndc_pos * 0.5f + 0.5f;
    anchor.y = 1.0f - anchor.y;
    anchor *= viewport.GetCanvasSize();
    out_screen_pos = viewport.GetCanvasP0() + anchor;
    return true;
}

void edit::Object::DrawLineWs(const DrawOverlayContext& ctx, const glm::vec3& p0, const glm::vec3& p1, uint32_t color,
                              float thickness)
{
    glm::vec2 sp0, sp1;
    if (!GetScreenPos(ctx.viewport, p0, sp0))
        return;
    if (!GetScreenPos(ctx.viewport, p1, sp1))
        return;

    ctx.draw_list.AddLine(ImVec2(sp0.x, sp0.y), ImVec2(sp1.x, sp1.y), color, thickness);
}

glm::mat4 edit::Object::GetTranslationOnly(const glm::mat4& trans)
{
    return glm::mat4{glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
                     glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), glm::vec4(glm::vec3(trans[3]), 1.0f)};
}

void edit::Object::DrawAABBOverlay(const DrawOverlayContext& ctx, uint32_t color, float width) const
{
    // draw AABB
    auto& aabb = GetAABB();

    std::array<glm::vec2, 4> p;
    if (!GetScreenPos(ctx.viewport, glm::vec3(aabb.min.x, aabb.min.y, aabb.min.z), p[0]))
        return;
    if (!GetScreenPos(ctx.viewport, glm::vec3(aabb.max.x, aabb.min.y, aabb.min.z), p[1]))
        return;
    if (!GetScreenPos(ctx.viewport, glm::vec3(aabb.max.x, aabb.max.y, aabb.min.z), p[2]))
        return;
    if (!GetScreenPos(ctx.viewport, glm::vec3(aabb.min.x, aabb.max.y, aabb.min.z), p[3]))
        return;

    std::array<ImVec2, 4> imgui_points;
    for (size_t i = 0; i < 4; ++i)
    {
        imgui_points[i] = im::VecToImGui(p[i]);
    }

    ctx.draw_list.AddPolyline(imgui_points.data(), static_cast<int>(imgui_points.size()), color, width, ImDrawFlags_Closed);
}
