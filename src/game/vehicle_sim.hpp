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

    void Update();
    void ProcessInput(const VehicleSimulationInput& input);

    void SetTuning(const VehicleTuningContext& tuning);
    
    void SetSteering(float steering) { steering_ = steering; }
    float GetSteering() const { return steering_; }

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

    ~VehiclePhysics();

private:
    void UpdateBulletHitboxTransform();
    void UpdateWheels();

private:
    collision::DynamicsWorld& world_;
    collision::MotionState motion_;
    std::unique_ptr<btRigidBody> body_;
    std::unique_ptr<collision::RaycastVehicle> vehicle_;
    std::unique_ptr<btCollisionObject> bullet_hitbox_;

    // tuning
    float engine_force_ = 0.0f;
    float braking_force_ = 0.0f;
    float steering_speed_ = 0.0f;
    
    struct WheelSimulationTuning
    {
        float engine_factor = 0.0f;
        float braking_factor = 0.0f;
        float steering_factor = 0.0f;
        float z_offset = 0.0f;
    };
    
    std::vector<WheelSimulationTuning> wheel_tuning_;
    std::vector<VehicleWheelState> wheel_states_;
    size_t wheels_on_ground_ = 0;
    
    bool action_enabled_ = false;
    float steering_ = 0.0f;
    
    size_t can_roll_frames_ = 0;
    bool accelerating_ = false;
    bool braking_ = false;
    bool reversing_ = false;
};

} // namespace game
