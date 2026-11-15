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

    float idle_rpm = 800.0f;
    float max_rpm = 7000.0f;

    float inertia = 0.1f; // kg*m^2

    float rpm = 0.0f;

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

    void UpdateFreeRunning(float dt, float throttle)
    {
        constexpr float rpm_to_rad = 2.0f * 3.14159265f / 60.0f;
        constexpr float rad_to_rpm = 60.0f / (2.0f * 3.14159265f);

        float omega = rpm * rpm_to_rad;

        // Full-throttle torque curve.
        float combustion_torque = GetTorqueAtRPM(rpm) * throttle;

        // Basic internal friction model.
        float friction_torque = 20.0f + 0.02f * omega;

        // Idle speed controller.
        // Adds enough torque to maintain idle.
        float idle_error = idle_rpm - rpm;
        float idle_torque = std::clamp(idle_error * 0.5f, 0.0f, 100.0f);

        combustion_torque += idle_torque;

        // Simple rev limiter.
        if (rpm >= max_rpm)
            combustion_torque = 0.0f;

        float net_torque = combustion_torque - friction_torque;
        float angular_acceleration = net_torque / inertia;
        omega += angular_acceleration * dt;

        omega = std::max(0.0f, omega);
        rpm = omega * rad_to_rpm;
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
