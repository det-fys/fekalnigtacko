#include "map_edit.hpp"

#include <imgui_internal.h>

edit::MapEdit::MapEdit() : viewport_2d_(context_), viewport_3d_(context_) {}

void edit::MapEdit::Show(bool* open)
{
    Update();

    ShowMainWindow(open);

    // show sub windows
    viewport_2d_.Show(nullptr);
    viewport_3d_.Show(nullptr);
    ShowListWindow(nullptr);
    ShowPropertiesWindow(nullptr);
}

void edit::MapEdit::Update()
{
    if (context_.project)
    {
        context_.project->SetDayTime(context_.day_time);
        context_.project->Update();
    }
}

void edit::MapEdit::ShowMainWindow(bool* open)
{
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    ImGuiWindowFlags window_flags = 0;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    auto show = ImGui::Begin("Map Editor", open, window_flags);
    ImGui::PopStyleVar(2);

    auto dockspace_id = ImGui::GetID("Map Editor Dockspace");

    if (show)
    {
        if (ImGui::BeginMainMenuBar())
        {
            ImGui::Separator();

            if (ImGui::BeginMenu("Map"))
            {
                if (ImGui::MenuItem("New"))
                {
                    NewMap();
                }

                if (ImGui::MenuItem("Close"))
                {
                    MapClose();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                ImGui::Separator();

                if (ImGui::MenuItem("Reset Map Editor layout"))
                {
                    ImGui::DockBuilderRemoveNode(dockspace_id); // Clear existing layout
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    if (!ImGui::DockBuilderGetNode(dockspace_id))
    {
        ImGui::DockBuilderRemoveNode(dockspace_id); // Clear existing layout
        ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetContentRegionAvail());

        ImGuiID dock_main_id = dockspace_id;

        ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.4f, nullptr, &dock_main_id);

        ImGuiID dock_bottom_id =
            ImGui::DockBuilderSplitNode(dock_right_id, ImGuiDir_Down, 0.3f, nullptr, &dock_right_id);

        ImGuiID dock_bottom_left_id =
            ImGui::DockBuilderSplitNode(dock_bottom_id, ImGuiDir_Left, 0.4f, nullptr, &dock_bottom_id);

        ImGui::DockBuilderDockWindow("2D viewport", dock_main_id);
        ImGui::DockBuilderDockWindow("3D viewport", dock_right_id);
        ImGui::DockBuilderDockWindow("List", dock_bottom_left_id);
        ImGui::DockBuilderDockWindow("Properties", dock_bottom_id);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

    ImGui::End();
}

void edit::MapEdit::ShowListWindow(bool* open)
{
    if (ImGui::Begin("List", open))
    {
        ImGui::Text("List window content");
    }
    ImGui::End();
}

void edit::MapEdit::ShowPropertiesWindow(bool* open)
{
    if (ImGui::Begin("Properties", open))
    {
        // 2D camera widgets
        ImGui::Text("2D Camera");
        ImGui::PushID("2D Camera Properties");
        ImGui::DragFloat2("Position", &context_.cam_pos_2d.x, 0.1f);
        ImGui::DragFloat("FOV", &context_.cam_fov_2d, 0.1f, 1.0f, 179.0f);
        ImGui::PopID();

        // 3D camera widgets
        ImGui::Text("3D Camera");
        ImGui::PushID("3D Camera Properties");
        ImGui::DragFloat3("Position", &context_.cam_pos_3d.x, 0.1f);
        ImGui::DragFloat("Pitch", &context_.cam_pitch_3d, 0.01f);
        ImGui::DragFloat("Yaw", &context_.cam_yaw_3d, 0.01f);
        ImGui::PopID();

        // world
        ImGui::Text("World");
        ImGui::PushID("World Properties");
        if (ImGui::DragFloat("Day Time", &context_.day_time, 0.1f))
        {
            context_.day_time = glm::mod(context_.day_time, 24.0f);
        }
        ImGui::PopID();

    }
    ImGui::End();
}

void edit::MapEdit::NewMap()
{
    MapClose();
    context_.project.emplace();
}

void edit::MapEdit::MapClose()
{
    context_.project.reset();
}
