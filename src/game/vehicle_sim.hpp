#pragma once

#include <memory>
#include <cstddef>
#include <vector>

#include "assets/vehiclemdl.hpp"
#include "collision/dynamicsworld.hpp"
#include "collision/motionstate.hpp"
#include "collision/object_info.hpp"
#include "collision/raycastvehicle.hpp"
#include "vehicle_tuning.hpp"
#include "vehicle_drivetrain.hpp"
#include "utils/defs.hpp"

namespace game
{

struct VehicleWheelState
{
    float rotation = 0.0f; // [rad]
    float speed = 0.0f; // [rad/s]
    float z_offset = 0.0f; // [m] against model definition
    bool touching_ground = false;
};

struct VehicleSimulationInput
{
    bool any_input = false;
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
    bool analog_steering = false;
    float target_steering = 0.0f;
};

// Physics-only vehicle simulation. Tuning must already be evaluated by the caller.
class VehiclePhysics
{
public:
    VehiclePhysics(collision::DynamicsWorld& world, FarTransform& transform, const assets::VehicleModel& model,
                   const VehicleTuningContext& tuning, collision::ObjectCallback* obj_cb = nullptr);

    DELETE_COPY_MOVE(VehiclePhysics)

    void Update(float dt);

    void SetTuning(const VehicleTuningContext& tuning);
    
    void SetSteering(float steering) { steering_ = steering; }
    float GetSteering() const { return steering_; }
    
    void SetInput(const VehicleSimulationInput& input) { input_ = input; }

    void ApplyImpulse(const glm::vec3& impulse, const glm::dvec3& world_position);
    void SetPosition(const glm::dvec3& position);
    glm::vec3 GetVelocity() const;
    void AddBodyFlags(collision::ObjectFlags flags) { collision::AddObjectFlags(body_.get(), flags); }
    const std::vector<VehicleWheelState>& GetWheelStates() const { return wheel_states_; }
    
    bool IsAccelerating() const { return accelerating_; }
    bool IsBraking() const { return braking_; }
    bool IsReversing() const { return reversing_; }

    collision::RaycastVehicle& GetBtVehicle() { return *vehicle_; }

    void DisableAction();
    bool IsActionEnabled() const { return action_enabled_; }

    void Shift(int dir) { gear_ = glm::clamp(gear_ + dir, transmission_.GetMinGear(), transmission_.GetMaxGear()); }

    float GetCurrentSpeedMs() const { return current_speed_; }
    float GetCurrentSpeedKmh() const { return current_speed_ * 3.6f; }
    float GetThrottle() const { return throttle_and_dir_; }
    float GetBrake() const { return brake_; }
    float GetCurrentRPM() const { return engine_rpm_; }
    int GetCurrentGear() const { return gear_; }
    
    ~VehiclePhysics();

private:
    void UpdateBulletHitboxTransform();
    void UpdateInput(float dt);
    void UpdateDrivetrain(float dt);
    void UpdateWheels(float dt);

private:
    collision::DynamicsWorld& world_;
    collision::MotionState motion_;
    std::unique_ptr<btRigidBody> body_;
    std::unique_ptr<collision::RaycastVehicle> vehicle_;
    std::unique_ptr<btCollisionObject> bullet_hitbox_;

    // tuning
    float max_braking_force_ = 0.0f;
    float steering_speed_ = 0.0f;
    
    struct WheelSimulationTuning
    {
        float engine_factor = 0.0f;
        float braking_factor = 0.0f;
        float steering_factor = 0.0f;
        float z_offset = 0.0f;
    };
    
    std::vector<WheelSimulationTuning> wheel_tuning_;
    
    VehicleEngine engine_{};
    VehicleTransmission transmission_{};
    
    VehicleSimulationInput input_{};
    
    std::vector<VehicleWheelState> wheel_states_;
    size_t wheels_on_ground_ = 0;
    
    float driven_wheel_radius_ = 0.35f;
    
    bool action_enabled_ = false;

    float steering_ = 0.0f;
    float current_speed_ = 0.0f; // [m/s]
    float throttle_and_dir_ = 0.0f; // [-1, 1] negative = reverse, positive = forward
    float brake_ = 0.0f; // [0, 1]
    int gear_ = 0;
    float engine_rpm_ = 0.0f;

    size_t can_roll_frames_ = 0;
    bool accelerating_ = false;
    bool braking_ = false;
    bool reversing_ = false;
};

} // namespace game
