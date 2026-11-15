#pragma once

#include <cstdint>

#include "net/defs.hpp"

namespace game
{

constexpr size_t MAX_WHEELS = 4;

using VehicleFlags = uint8_t;

enum VehicleFlag : VehicleFlags
{
    VF_NONE = 0,
    VF_ENGINE_ON = 1,
    VF_BRAKING = 2,
    VF_BROKENWINDOWS = 4,
    VF_LIGHTS_ON = 8,
    VF_REVERSING = 16,
    VF_ORANGE_LIGHTS_ON = 32,
    VF_NO_WHEELS = 64,
    VF_EXPLODED = 128,
};

struct VehicleSyncState
{
    VehicleFlags flags = VF_NONE;
    
    net::FarPositionQ pos;
    net::QuatQ rot;

    net::AngleQ steering;
    net::VehicleSpeedQ speed;
    net::VehicleEngineRPMQ engine_rpm;
    net::VehicleGear gear;

    struct
    {
        net::WheelZOffsetQ z_offset; // how much compressed
        net::RotationSpeedQ speed;   // rad/s
    } wheels[MAX_WHEELS];
};

using VehicleSyncFieldFlags = uint8_t;
enum VehicleSyncFieldFlag
{
    VSF_FLAGS = 1,
    VSF_POSITION = 2,
    VSF_ROTATION = 4,
    VSF_STEERING = 8,
    VSF_WHEELS = 16,
    VSF_SPEED_RPM = 32,
    VSF_GEAR = 64,
};


} // namespace game