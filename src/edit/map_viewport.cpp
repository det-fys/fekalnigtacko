#include "map_viewport.hpp"

#include <glm/gtc/type_ptr.hpp>

#include "im/utils.hpp"
#include "object.hpp"

edit::MapViewport::MapViewport(std::string title, MapEditContext& context)
    : Super(std::move(title)), context_(context), frustum_(glm::mat4(1.0f))
{
}

void edit::MapViewport::Draw(ImDrawList& draw_list)
{
    if (!context_.project)
    {
        return;
    }

    auto& project = *context_.project;

    // draw scene
    scene_view_.Draw(draw_list, im::VecToImGui(GetCanvasP0()), im::VecToImGui(GetCanvasSize()), project, cam_);

    // draw gizmo
    if (ImGui::IsKeyPressed(ImGuiKey_E))
    {
        context_.gizmo_operation = ImGuizmo::TRANSLATE;
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_R))
    {
        context_.gizmo_operation = ImGuizmo::ROTATE;
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_T))
    {
        context_.gizmo_operation = ImGuizmo::SCALE;
    }

    glm::vec3 snap = glm::vec3(0.1f);
    if (context_.gizmo_operation == ImGuizmo::ROTATE)
    {
        snap = glm::vec3(5.0f);
    }
    else if (context_.gizmo_operation == ImGuizmo::SCALE)
    {
        snap = glm::vec3(0.01f);
    }

    auto selection_matrix = project.GetSelectionMatrix();
    if (selection_matrix)
    {
        ImGuizmo::PushID("Selection Gizmo");

        auto matrix = *selection_matrix;
        glm::mat4 delta_matrix{1.0f};

        if (ImGuizmo::Manipulate(glm::value_ptr(view_), glm::value_ptr(proj_), context_.gizmo_operation,
                                 ImGuizmo::LOCAL, glm::value_ptr(matrix), glm::value_ptr(delta_matrix),
                                 ImGui::IsKeyDown(ImGuiKey_LeftCtrl) ? glm::value_ptr(snap) : nullptr))
        {
            project.ApplySelectionTransform(delta_matrix);
        }

        ImGuizmo::PopID();
    }

    project.DrawOverlay(DrawOverlayContext(*this, draw_list));
}
