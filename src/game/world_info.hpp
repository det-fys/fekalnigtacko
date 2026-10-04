#pragma once

#include <cstdint>

namespace game
{

enum WorldType : uint8_t
{
    WORLD_TYPE_MAP = 0,
    WORLD_TYPE_PROCEDURAL = 1,
};

} // namespace game