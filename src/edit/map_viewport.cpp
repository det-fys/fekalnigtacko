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
    
    // hover & selection
    if (context_.project && IsHovered() && !IsGizmoHovered())
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
        
        context_.project->SetHover(start, end);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            context_.project->MakeSelection(start, end, ImGui::IsKeyDown(ImGuiKey_LeftShift));
        }
    }

}

void edit::MapViewport::Draw(ImDrawList& draw_list)
{
    if (!context_.project)
    {
        return;
    }

    auto& project = *context_.project;


    if (context_.draw_world)
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
