#include "viewport.hpp"

#include "utils.hpp"
#include <ImGuizmo.h>

im::Viewport::Viewport(std::string title) : title_(std::move(title)) {}

void im::Viewport::Show(bool* open)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    bool show =
        ImGui::Begin(title_.c_str(), open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    if (show)
    {
        DrawWindowLayout();
    }

    ImGui::End();
}

void im::Viewport::Update() {}

void im::Viewport::DrawWindowLayout()
{
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();    // ImDrawList API uses screen coordinates!
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail(); // Resize canvas to what's available
    // ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    if (canvas_sz.x < 1.0f || canvas_sz.y < 1.0f)
    {
        return;
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    if (ImGuizmo::IsOver() || ImGuizmo::IsUsing())
    {
        ImGui::Dummy(canvas_sz);

        active_ = false;
        hovered_ = ImGui::IsItemHovered();
        focused_ = ImGui::IsWindowFocused();
        gizmo_hovered_ = true;
    }
    else
    {
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("vp", canvas_sz,
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                                   ImGuiButtonFlags_MouseButtonMiddle);

        active_ = ImGui::IsItemActive();
        hovered_ = ImGui::IsItemHovered();
        focused_ = ImGui::IsWindowFocused();
        gizmo_hovered_ = false;
    }

    canvas_p0_ = VecFromImGui(canvas_p0);
    canvas_sz_ = VecFromImGui(canvas_sz);

    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(canvas_p0_.x, canvas_p0_.y, canvas_sz_.x, canvas_sz_.y);

    ImGuizmo::PushID(title_.c_str());

    Update();
    Draw(*draw_list);

    ImGuizmo::PopID();
}
