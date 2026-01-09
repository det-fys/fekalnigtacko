#pragma once

#include <cstdint>

namespace game
{

using VehicleFlags = uint8_t;

enum VehicleFlag : VehicleFlags
{
    VF_NONE,
    VF_ACCELERATING = 1,
    VF_BREAKING = 2,
};

}