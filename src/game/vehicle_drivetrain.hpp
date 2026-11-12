#pragma once

#include <array>
#include <cstddef>

#include <glm/glm.hpp>

namespace game
{

struct VehicleEngineTorqueCurvePoint
{
    float rpm;
    float torque;
};

struct VehicleEngine
{
    std::array<VehicleEngineTorqueCurvePoint, 16> torque_curve;
    size_t num_torque_curve_points = 0;

    float GetTorqueAtRPM(float rpm) const
    {
        for (int i = 1; i < num_torque_curve_points; ++i)
        {
            const auto& point = torque_curve[i];
            if (point.rpm < rpm)
                continue;

            const auto& prev_point = torque_curve[i - 1];
            float t = (rpm - prev_point.rpm) / (point.rpm - prev_point.rpm);
            return glm::mix(prev_point.torque, point.torque, t);
        }

        return 0.0f;
    }
};

struct VehicleTransmission
{
    // ... R N 1 2 3 4 5 ...
    std::array<float, 16> gear_ratios;
    int num_gears = 0;
    int neutral_gear = 1;

    float final_drive_ratio = 1.0f;

    int GetMinGear() const { return -neutral_gear; }
    int GetMaxGear() const { return num_gears - neutral_gear - 1; }

    // -1 R
    //  0 N
    //  1 1 ...
    float GetGearRatio(int gear) const { return gear_ratios[gear + neutral_gear]; }

    float GetRatio(int gear) const { return GetGearRatio(gear) * final_drive_ratio; }
};

} // namespace game
