#include "map_viewport_2d.hpp"

#include <functional>

#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

#include "im/utils.hpp"
#include "map_project.hpp"

edit::MapViewport2D::MapViewport2D(MapEditContext& context)
    : Super("2D viewport", context), controls_(*this, GetContext().cam_pos_2d, GetContext().zoom_2d)
{
}

static gfx::CameraParams GetCameraParams(const glm::vec2& p0, const glm::vec2& p1, float fov)
{
    gfx::CameraParams cam{};
    glm::vec2 center = (p0 + p1) * 0.5f;

    float height = std::abs(p1.y - p0.y);
    float half_fov_radians = glm::radians(fov * 0.5f);
    float distance = (height * 0.5f) / std::tan(half_fov_radians);
    cam.eye = glm::vec3(center.x, center.y, distance);

    cam.dir = glm::vec3(0.0f, 0.0f, -1.0f);
    cam.up = glm::vec3(0.0f, 1.0f, 0.0f);
    cam.fov = fov * 2.0f; // TODO: fix incorrect fov calculation in renderers

    return cam;
}

void edit::MapViewport2D::Update()
{
    controls_.Update();

    // update cam
    SetCameraParams(GetCameraParams(controls_.GetPosWsP0(), controls_.GetPosWsP1(), GetContext().cam_fov_2d));

    auto half_size_ws = controls_.GetSizeWs() * 0.5f;
    auto center_ws = glm::vec3((controls_.GetPosWsP0() + controls_.GetPosWsP1()) * 0.5f, 100.0f);
    auto proj = glm::ortho(-half_size_ws.x, half_size_ws.x, -half_size_ws.y, half_size_ws.y, -1000.0f, 1000.0f);
    auto view = glm::lookAt(center_ws, center_ws + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    SetOverlayMatrices(view, proj);

    Super::Update();

    // make world space aabb for current view
    aabb_ = AABB2();
    aabb_.AddPoint(controls_.GetPosWsP0());
    aabb_.AddPoint(controls_.GetPosWsP1());

    if (!GetContext().project)
    {
        return;
    }

    auto& project = *GetContext().project;

    ImVec2 drag_delta_r = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    if (drag_delta_r.x == 0.0f && drag_delta_r.y == 0.0f)
    {
        ImGui::OpenPopupOnItemClick("2d_context", ImGuiPopupFlags_MouseButtonRight);
        if (IsHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            new_obj_pos_ = controls_.GetMousePosWs();
    }

    if (ImGui::BeginPopup("2d_context"))
    {
        ShowContextMenu();
        ImGui::EndPopup();
    }
}

void edit::MapViewport2D::Draw(ImDrawList& draw_list)
{
    Super::Draw(draw_list);

    // draw rect at world space (0, 0) to (100, 100)
    // draw_list.AddRect(im::VecToImGui(controls_.WsToCanvas(glm::vec2(0.0f, 0.0f))),
    //                  im::VecToImGui(controls_.WsToCanvas(glm::vec2(100.0f, 100.0f))), IM_COL32(255, 0, 0, 255));

    // draw world origin
    if (aabb_.CollidesWith(AABB2(glm::vec2(-5.0f, -5.0f), glm::vec2(5.0f, 5.0f))))
    {
        draw_list.AddCircleFilled(im::VecToImGui(controls_.WsToCanvas(glm::vec2(0.0f, 0.0f))), 5.0f,
                                  IM_COL32(255, 255, 0, 255));
    }

    DrawChunks(draw_list);
}

static void ShowStaticModelsMenu(const edit::StaticModelsEntry& entry, std::function<void(const std::string&)> cb)
{
    for (const auto& child : entry.children)
    {
        // leaf
        if (child.children.empty())
        {
            if (ImGui::MenuItem(child.display_name.c_str()))
            {
                cb(child.model_name);
            }

            continue;
        }

        // dir
        if (ImGui::BeginMenu(child.display_name.c_str()))
        {
            ShowStaticModelsMenu(child, cb);
            ImGui::EndMenu();
        }
    }
}

void edit::MapViewport2D::ShowContextMenu()
{
    if (!GetContext().project)
        return;

    auto& project = *GetContext().project;

    ImGui::SeparatorText("add static object");

    ShowStaticModelsMenu(project.GetStaticModelsRoot(),
                         [&](const std::string& model_name) { project.AddStaticObject(new_obj_pos_, model_name); });
}

void edit::MapViewport2D::DrawChunks(ImDrawList& draw_list)
{
    if (!GetContext().project)
        return;

    const auto& project = *GetContext().project;
    const auto& map_cfg = project.GetMapConfig();

    auto half_chunks = static_cast<int>(map_cfg.chunks / 2);
    auto [min_chunk, max_chunk] = mg::GetChunkRange(map_cfg, aabb_);

    auto grid_color = IM_COL32(255, 255, 255, 128);

    auto x0 = glm::max(GetCanvasP0().x, controls_.WsToCanvas(glm::vec2(-half_chunks * map_cfg.chunk_size_m, 0.0f)).x);
    auto x1 = glm::min(GetCanvasP0().x + GetCanvasSize().x, controls_.WsToCanvas(glm::vec2(half_chunks * map_cfg.chunk_size_m, 0.0f)).x);
    for (int chunk_y = min_chunk.y; chunk_y <= max_chunk.y; ++chunk_y)
    {
        auto y_ws = chunk_y * map_cfg.chunk_size_m;
        auto y = controls_.WsToCanvas(glm::vec2(0.0f, y_ws)).y;

        draw_list.AddLine(ImVec2(x0, y), ImVec2(x1, y), grid_color);
    }

    auto y0 = glm::max(GetCanvasP0().y, controls_.WsToCanvas(glm::vec2(0.0f, -half_chunks * map_cfg.chunk_size_m)).y);
    auto y1 = glm::min(GetCanvasP0().y + GetCanvasSize().y, controls_.WsToCanvas(glm::vec2(0.0f, half_chunks * map_cfg.chunk_size_m)).y);
    for (int chunk_x = min_chunk.x; chunk_x <= max_chunk.x; ++chunk_x)
    {
        auto x_ws = chunk_x * map_cfg.chunk_size_m;
        auto x = controls_.WsToCanvas(glm::vec2(x_ws, 0.0f)).x;
        draw_list.AddLine(ImVec2(x, y0), ImVec2(x, y1), grid_color);
    }

    for (int chunk_y = min_chunk.y; chunk_y <= max_chunk.y; ++chunk_y)
    {
        for (int chunk_x = min_chunk.x; chunk_x <= max_chunk.x; ++chunk_x)
        {
            DrawChunk(draw_list, glm::ivec2(chunk_x, chunk_y));
        }
    }
}

void edit::MapViewport2D::DrawChunk(ImDrawList& draw_list, const glm::ivec2& coord)
{
    const auto& context = GetContext();
    const auto& project = *context.project;
    const auto& chunks = project.GetChunks();

    auto it = chunks.find(coord);

    if (it == chunks.end())
        return;

    const auto& chunk = it->second;

    if (context.draw_chunk_state)
    {
        DrawChunkState(draw_list, coord, chunk.state);
    }

    if (context.draw_chunk_mesh)
    {
        DrawChunkMesh(draw_list, coord, chunk);
    }
}

void edit::MapViewport2D::DrawChunkState(ImDrawList& draw_list, const glm::ivec2& coord, ChunkState state)
{
    if (state == CHUNK_STATE_READY)
        return;

    auto chunk_size_m = GetContext().project->GetMapConfig().chunk_size_m;
    uint32_t color = state == CHUNK_STATE_INVALID ? IM_COL32(255, 0, 0, 64) : IM_COL32(255, 255, 0, 64);

    draw_list.AddRectFilled(
        im::VecToImGui(controls_.WsToCanvas(
            glm::vec2(coord.x * chunk_size_m, coord.y * chunk_size_m))),
        im::VecToImGui(controls_.WsToCanvas(glm::vec2((coord.x + 1) * chunk_size_m,
                                                      (coord.y + 1) * chunk_size_m))),
        color);

}

void edit::MapViewport2D::DrawChunkMesh(ImDrawList& draw_list, const glm::ivec2& coord, const Chunk& chunk)
{
    auto color = (coord.x + coord.y) % 2 == 0 ? IM_COL32(128, 0, 0, 255) : IM_COL32(0, 0, 128, 255);
    auto color2 = (coord.x + coord.y) % 2 == 0 ? IM_COL32(128, 0, 0, 32) : IM_COL32(0, 0, 128, 32);

    // draw tris
    for (const auto& [i0, i1, i2] : chunk.vis_tris)
    {
        auto& p0 = chunk.vis_verts[i0];
        auto& p1 = chunk.vis_verts[i1];
        auto& p2 = chunk.vis_verts[i2];
        draw_list.AddTriangleFilled(im::VecToImGui(controls_.WsToCanvas(p0)), im::VecToImGui(controls_.WsToCanvas(p1)),
                                    im::VecToImGui(controls_.WsToCanvas(p2)), color2);
    }

    // draw mesh edges
    for (const auto& [i0, i1] : chunk.vis_edges)
    {
        auto& p0 = chunk.vis_verts[i0];
        auto& p1 = chunk.vis_verts[i1];

        draw_list.AddLine(im::VecToImGui(controls_.WsToCanvas(p0)), im::VecToImGui(controls_.WsToCanvas(p1)), color);
    }

}
