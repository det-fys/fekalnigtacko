#include "map_edit.hpp"

#include <imgui_internal.h>

constexpr static std::string_view MAP_PROJ_PREFIX = "data/";
constexpr static std::string_view MAP_PROJ_SUFFIX = ".mapl";

edit::MapEdit::MapEdit() : viewport_2d_(context_), viewport_3d_(context_) {}

void edit::MapEdit::Show(bool* open)
{
    Update();

    ShowMainWindow(open);

    // show sub windows
    viewport_2d_.Show(nullptr);
    viewport_3d_.Show(nullptr);
    ShowSelectionWindow(nullptr);
    ShowPropertiesWindow(nullptr);

    // show dialogs
    ShowOpenProjectDialog();
    ShowSaveAsDialog();
}

void edit::MapEdit::Update()
{
    if (context_.project)
    {
        auto& project = *context_.project;
        project.Update();
    }
}

void edit::MapEdit::OpenOpenProjectDialog()
{
    LoadProjectList();
    selected_project_index_ = -1;
    open_open_dialog = true;
}

void edit::MapEdit::LoadProjectList()
{
    map_projects_.clear();

    fs::FileSystem::GetInstance().ForEachFile([this](const std::string& path) {
        if (path.starts_with(MAP_PROJ_PREFIX) && path.ends_with(MAP_PROJ_SUFFIX))
        {
            auto name =
                path.substr(MAP_PROJ_PREFIX.size(), path.size() - MAP_PROJ_PREFIX.size() - MAP_PROJ_SUFFIX.size());
            map_projects_.push_back(name);
        }
    });
}

void edit::MapEdit::ShowOpenProjectDialog()
{
    if (open_open_dialog)
    {
        ImGui::OpenPopup("Open Project");
        open_open_dialog = false;
    }

    if (BeginDialogPopupModal("Open Project"))
    {
        if (ImGui::BeginListBox("##project_list", ImVec2(-FLT_MIN, 10 * ImGui::GetTextLineHeightWithSpacing())))
        {

            for (int i = 0; i < map_projects_.size(); ++i)
            {
                const auto& project_name = map_projects_[i];
                bool selected = (selected_project_index_ == i);
                if (ImGui::Selectable(project_name.c_str(), selected))
                {
                    selected_project_index_ = i;
                }
            }

            ImGui::EndListBox();
        }

        const bool can_open = selected_project_index_ >= 0 && selected_project_index_ < map_projects_.size();
        ImGui::BeginDisabled(!can_open);
        if (ImGui::Button("Open") && can_open)
        {
            ImGui::CloseCurrentPopup();
            OpenProject(map_projects_[selected_project_index_]);
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

void edit::MapEdit::OpenProject(const std::string& project_name)
{
    auto file_name = std::string(MAP_PROJ_PREFIX) + project_name + std::string(MAP_PROJ_SUFFIX);
    CmdClose();
    context_.project.emplace(context_.properties, project_name, file_name);
}

void edit::MapEdit::OpenSaveAsDialog()
{
    if (!context_.project)
        return;

    save_as_name_buf_[0] = '\0';
    open_save_as_dialog_ = true;
}

void edit::MapEdit::ShowSaveAsDialog()
{
    if (open_save_as_dialog_)
    {
        ImGui::OpenPopup("Save As");
        open_save_as_dialog_ = false;
    }

    if (BeginDialogPopupModal("Save As"))
    {
        ImGui::InputText("Name", save_as_name_buf_.data(), save_as_name_buf_.size());

        // validate name
        bool valid_name = save_as_name_buf_[0] != '\0';
        if (valid_name)
        {
            for (const auto& c : save_as_name_buf_)
            {
                if (c == '\0')
                    break;

                if (!std::isalnum(c) && c != '_' && c != '-')
                {
                    valid_name = false;
                    break;
                }
            }
        }

        if (!valid_name)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid name [a-zA-Z0-9_-] length > 0");
        }

        ImGui::BeginDisabled(!valid_name);
        if (ImGui::Button("Save") && valid_name)
        {
            SaveProject(save_as_name_buf_.data());
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        
        ImGui::EndPopup();
    }
}

void edit::MapEdit::SaveProject(const std::string& name)
{
    if (!context_.project)
        return;

    auto file_name = std::string(MAP_PROJ_PREFIX) + name + std::string(MAP_PROJ_SUFFIX);
    context_.project->SetName(name, file_name);
    context_.project->Save();
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
                    CmdNew();

                if (ImGui::MenuItem("Open..."))
                    CmdOpen();

                if (ImGui::MenuItem("Save"))
                    CmdSave();

                if (ImGui::MenuItem("Save As..."))
                    CmdSaveAs();

                if (ImGui::MenuItem("Close"))
                    CmdClose();

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
        ImGui::DockBuilderDockWindow("Selection", dock_bottom_left_id);
        ImGui::DockBuilderDockWindow("Properties", dock_bottom_id);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

    ImGui::End();
}

void edit::MapEdit::ShowSelectionWindow(bool* open)
{
    if (ImGui::Begin("Selection", open))
    {
        if (context_.project)
        {
            context_.project->ShowSelectionProperties();
        }
    }
    ImGui::End();
}

void edit::MapEdit::ShowPropertiesWindow(bool* open)
{
    auto& properties = context_.properties;

    if (ImGui::Begin("Properties", open))
    {
        // show fps
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

        // 2D camera widgets
        ImGui::Text("2D Camera");
        ImGui::PushID("2D Camera Properties");
        ImGui::DragScalarN("Position", ImGuiDataType_Double, &properties.cam_pos_2d.x, 2, 0.1f);
        ImGui::DragFloat("FOV", &properties.cam_fov_2d, 0.1f, 1.0f, 179.0f);
        ImGui::PopID();

        // 3D camera widgets
        ImGui::Text("3D Camera");
        ImGui::PushID("3D Camera Properties");
        ImGui::DragScalarN("Position", ImGuiDataType_Double, &properties.cam_pos_3d.x, 3, 0.1f);
        ImGui::DragFloat("Pitch", &properties.cam_pitch_3d, 0.01f);
        ImGui::DragFloat("Yaw", &properties.cam_yaw_3d, 0.01f);
        ImGui::PopID();

        // world
        ImGui::Text("World");
        ImGui::PushID("World Properties");
        if (ImGui::DragFloat("Day Time", &properties.day_time, 0.1f))
        {
            properties.day_time = glm::mod(properties.day_time, 24.0f);
        }
        ImGui::Checkbox("Draw World Env", &properties.draw_world_env);
        ImGui::Checkbox("Draw World", &properties.draw_world);
        ImGui::Checkbox("Draw Chunk Mesh", &properties.draw_chunk_mesh);
        ImGui::Checkbox("Draw Chunk State", &properties.draw_chunk_state);
        ImGui::Checkbox("Draw Waypoints Overlay", &properties.draw_waypoints_overlay);
        ImGui::PopID();

        // editor
        ImGui::Text("Editor");
        ImGui::PushID("Editor Properties");
        ImGui::Checkbox("Snap to Terrain Height", &properties.snap_to_terrain_height);
        ImGui::PopID();
    }
    ImGui::End();
}

bool edit::MapEdit::BeginDialogPopupModal(const char* name) const
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_Appearing);
    return ImGui::BeginPopupModal(name, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
}

void edit::MapEdit::CmdNew()
{
    CmdClose();
    context_.project.emplace(context_.properties);
}

void edit::MapEdit::CmdOpen()
{
    OpenOpenProjectDialog();
}

void edit::MapEdit::CmdSave()
{
    if (!context_.project)
        return;

    if (context_.project->Save())
        return; // success

    // project has no file name, show save as dialog
    CmdSaveAs();
}

void edit::MapEdit::CmdSaveAs()
{
    OpenSaveAsDialog();
}

void edit::MapEdit::CmdClose()
{
    context_.project.reset();
}
