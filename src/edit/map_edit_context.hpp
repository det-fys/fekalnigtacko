#pragma once

#include <optional>

#include "map_project.hpp"
#include "map_edit_properties.hpp"

namespace edit
{

struct MapEditContext
{
    MapEditProperties properties;
    std::optional<Project> project;
};

} // namespace edit
