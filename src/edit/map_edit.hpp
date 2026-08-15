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

    void ShowListWindow(bool* open);
    void ShowPropertiesWindow(bool* open);

    // commands
    void NewMap();
    void MapClose();

private:
    MapEditContext context_;

    MapViewport2D viewport_2d_;
    MapViewport3D viewport_3d_;
};

} // namespace edit
