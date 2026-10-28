#pragma once

#include "map_layout.hpp"

namespace mg
{

void SaveMapLayout(const std::string& file_name, const ResourceSet& res, const MapLayout& map);
MapLayout LoadMapLayout(const std::string& file_name, const ResourceSet& res);

} // namespace mg
