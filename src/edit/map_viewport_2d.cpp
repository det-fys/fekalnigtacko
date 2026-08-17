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
    Super::Update();

    controls_.Update();

    // update cam
    SetCameraParams(GetCameraParams(controls_.GetPosWsP0(), controls_.GetPosWsP1(), GetContext().cam_fov_2d));

    auto half_size_ws = controls_.GetSizeWs() * 0.5f;
    auto center_ws = glm::vec3((controls_.GetPosWsP0() + controls_.GetPosWsP1()) * 0.5f, 100.0f);
    auto proj = glm::ortho(-half_size_ws.x, half_size_ws.x, -half_size_ws.y, half_size_ws.y, -1000.0f, 1000.0f);
    auto view = glm::lookAt(center_ws, center_ws + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    SetOverlayMatrices(view, proj);

    if (!GetContext().project)
    {
        return;
    }

    auto& project = *GetContext().project;

    // selection
    if (IsHovered() && !IsGizmoHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        project.MakeSelection2D(controls_.GetMousePosWs(), nullptr);
    }

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
    draw_list.AddRect(im::VecToImGui(controls_.WsToCanvas(glm::vec2(0.0f, 0.0f))),
                      im::VecToImGui(controls_.WsToCanvas(glm::vec2(100.0f, 100.0f))), IM_COL32(255, 0, 0, 255));
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
