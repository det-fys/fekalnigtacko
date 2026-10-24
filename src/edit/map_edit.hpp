#pragma once

#include "map_edit_context.hpp"
#include "map_viewport_2d.hpp"
#include "map_viewport_3d.hpp"

namespace edit
{

class MapEdit
{
public:
    MapEdit();

    void Show(bool* open);

private:
    void Update();

    void ShowMainWindow(bool* open);

    void ShowSelectionWindow(bool* open);
    void ShowPropertiesWindow(bool* open);

    bool BeginDialogPopupModal(const char* name) const;

    // Open dialog
    void OpenOpenProjectDialog();
    void LoadProjectList();
    void ShowOpenProjectDialog();
    void OpenProject(const std::string& project_name);

    // Save As dialog
    void OpenSaveAsDialog();
    void ShowSaveAsDialog();
    void SaveProject(const std::string& name);

    // commands
    void CmdNew();
    void CmdOpen();
    void CmdSave();
    void CmdSaveAs();
    void CmdClose();

private:
    MapEditContext context_;

    MapViewport2D viewport_2d_;
    MapViewport3D viewport_3d_;

    std::vector<std::string> map_projects_;
    bool open_open_dialog = false;
    int selected_project_index_ = -1;
    bool open_save_as_dialog_ = false;
    std::array<char, 64> save_as_name_buf_;
};

} // namespace edit
