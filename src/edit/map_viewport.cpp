#include "map_viewport.hpp"

#include <glm/gtc/type_ptr.hpp>

#include "im/utils.hpp"
#include "object.hpp"

edit::MapViewport::MapViewport(std::string title, MapEditContext& context)
    : Super(std::move(title)), context_(context), frustum_(glm::mat4(1.0f))
{
}

void edit::MapViewport::Update()
{
    auto& io = ImGui::GetIO();
    auto& properties = GetProperties();

    if (context_.project && IsHovered())
    {
        auto& project = *context_.project;

        // hover & selection
        if (!IsGizmoHovered())
        {
            auto inv_vp = glm::inverse(view_proj_);

            auto pos = (im::VecFromImGui(io.MousePos) - GetCanvasP0()) / GetCanvasSize();
            float ndc_x = pos.x * 2.0f - 1.0f;
            float ndc_y = 1.0f - (pos.y * 2.0f);

            glm::vec4 ndc_start(ndc_x, ndc_y, -1.0f, 1.0f);
            glm::vec4 ndc_end(ndc_x, ndc_y, 1.0f, 1.0f);

            glm::vec4 world_start = inv_vp * ndc_start;
            glm::vec4 world_end = inv_vp * ndc_end;

            glm::vec3 start = glm::vec3(world_start) / world_start.w;
            glm::vec3 end = glm::vec3(world_end) / world_end.w;

            project.SetHover(start, end);

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                project.MakeSelection(start, end, ImGui::IsKeyDown(ImGuiKey_LeftShift));
            }
        }
    }

    // context menu
    ImVec2 drag_delta_r = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    if (drag_delta_r.x == 0.0f && drag_delta_r.y == 0.0f)
    {
        ImGui::OpenPopupOnItemClick("Context Menu", ImGuiPopupFlags_MouseButtonRight);
    }

    context_menu_shown_ = ImGui::BeginPopup("Context Menu");
    ShowContextMenuContent(); // call this anyways for shortcut keys to work
    if (context_menu_shown_)
    {
        ImGui::EndPopup();
    }
}

void edit::MapViewport::Draw(ImDrawList& draw_list)
{
    if (!context_.project)
    {
        return;
    }

    auto& project = *context_.project;

    auto& properties = GetProperties();
    if (properties.draw_world)
    {
        // draw scene
        scene_view_.Draw(draw_list, im::VecToImGui(GetCanvasP0()), im::VecToImGui(GetCanvasSize()), project, cam_);
    }
    else
    {
        // draw empty background
        draw_list.AddRectFilled(im::VecToImGui(GetCanvasP0()), im::VecToImGui(GetCanvasP0() + GetCanvasSize()),
                                IM_COL32(128, 128, 128, 255));
    }

    // draw gizmo
    glm::vec3 snap = glm::vec3(0.1f);
    if (properties.gizmo_operation == ImGuizmo::ROTATE)
    {
        snap = glm::vec3(5.0f);
    }
    else if (properties.gizmo_operation == ImGuizmo::SCALE)
    {
        snap = glm::vec3(0.01f);
    }

    auto selection_matrix = project.GetSelectionMatrix();
    if (selection_matrix)
    {
        ImGuizmo::PushID("Selection Gizmo");

        auto matrix = *selection_matrix;
        glm::mat4 delta_matrix{1.0f};

        if (ImGuizmo::Manipulate(glm::value_ptr(view_), glm::value_ptr(proj_), properties.gizmo_operation,
                                 ImGuizmo::LOCAL, glm::value_ptr(matrix), glm::value_ptr(delta_matrix),
                                 ImGui::IsKeyDown(ImGuiKey_LeftCtrl) ? glm::value_ptr(snap) : nullptr))
        {
            project.ApplySelectionTransform(delta_matrix);
        }

        ImGuizmo::PopID();
    }

    project.DrawOverlay(DrawOverlayContext(*this, draw_list));
}

void edit::MapViewport::ShowContextMenuContent()
{
    auto& properties = GetProperties();
    
    if (context_menu_shown_)
    {
        ImGui::SeparatorText("transform");
    }

    if (ContextMenuItem("Translate", "T", ImGuiKey_T))
        properties.gizmo_operation = ImGuizmo::TRANSLATE;

    if (ContextMenuItem("Rotate", "R", ImGuiKey_R))
        properties.gizmo_operation = ImGuizmo::ROTATE;

    if (ContextMenuItem("Translate/Rotate Z", "Z", ImGuiKey_Z))
        properties.gizmo_operation = ImGuizmo::ROTATE_Z | ImGuizmo::TRANSLATE;   

    if (context_.project)
    {
        auto& project = *context_.project;

        // if (ContextMenuItem("Duplicate/Extrude", "Space", ImGuiKey_Space, project.HasSelection()))
        //     project.DuplicateSelection();

        if (context_menu_shown_)
        {
            ImGui::SeparatorText("selection");
        }

        if (ContextMenuItem("Delete", "Del", ImGuiKey_Delete, project.HasSelection()))
            project.DeleteSelection();

        if (ContextMenuItem("Link", "C", ImGuiKey_C, project.HasSelection()))
            project.LinkSelection(true);

        if (ContextMenuItem("Unlink", "X", ImGuiKey_X, project.HasSelection()))
            project.LinkSelection(false);
    }
}

bool edit::MapViewport::ContextMenuItem(const char* label, const char* shortcut_str, ImGuiKey shortcut_key,
                                        bool enabled) const
{
    return ((context_menu_shown_ && ImGui::MenuItem(label, shortcut_str, false, enabled)) ||
            (IsFocused() && ImGui::IsKeyPressed(shortcut_key))) &&
           enabled;
}
