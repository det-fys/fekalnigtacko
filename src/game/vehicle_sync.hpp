#pragma once

#include <cstdint>

#include "net/defs.hpp"

namespace game
{

constexpr size_t MAX_WHEELS = 4;

using VehicleFlags = uint8_t;

enum VehicleFlag : VehicleFlags
{
    VF_NONE,
    VF_ACCELERATING = 0x01,
    VF_BREAKING = 0x02,
    VF_BROKENWINDOWS = 0x04,
};

struct VehicleSyncState
{
    VehicleFlags flags = VF_NONE;
    
    net::PositionQ pos;
    net::QuatQ rot;

    net::AngleQ steering;

    struct
    {
        net::WheelZOffsetQ z_offset; // how much compressed
        net::RotationSpeedQ speed;   // rad/s
    } wheels[MAX_WHEELS];
};

using VehicleSyncFieldFlags = uint8_t;

enum VehicleSyncFieldFlag
{
    VSF_FLAGS = 0x01,
    VSF_POSITION = 0x02,
    VSF_ROTATION = 0x04,
    VSF_STEERING = 0x08,
    VSF_WHEELS = 0x10,
};


} // namespace game