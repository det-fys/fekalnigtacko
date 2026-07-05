#pragma once

#include <glm/glm.hpp>

namespace gfx
{

using LightCellCoord = glm::u16vec2;
using LightCellCoordHash = uint32_t;

inline LightCellCoord NormalizeCellCoord(int x, int y)
{
    return LightCellCoord(x + 32768, y + 32768);
}

inline LightCellCoordHash HashCellCoord(LightCellCoord coord)
{
    return (static_cast<uint32_t>(coord.y) << 16) | static_cast<uint32_t>(coord.x);
}

inline LightCellCoord GetCellCoord(const glm::vec3& pos, float cell_size)
{
    auto x = static_cast<int>(glm::floor(pos.x / cell_size));
    auto y = static_cast<int>(glm::floor(pos.y / cell_size));
    return NormalizeCellCoord(x, y);
}

} // namespace gfx
