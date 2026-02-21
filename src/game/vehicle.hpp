#pragma once

#include <array>
#include <cstddef>

#include "assets/vehiclemdl.hpp"
#include "collision/motionstate.hpp"
#include "entity.hpp"
#include "world.hpp"
#include "vehicle_sync.hpp"

namespace game
{

struct VehicleWheelState
{
    float rotation = 0.0f; // [rad]
    float speed = 0.0f;    // [rad/s]
    float z_offset = 0.0f; // [m] against model definition
};

using VehicleInputFlags = uint8_t;

enum VehicleInputType
{
    VIN_FORWARD,
    VIN_BACKWARD,
    VIN_LEFT,
    VIN_RIGHT,
    VIN_HANDBRAKE,
};

class Vehicle : public Entity
{
public:
    using Super = Entity;

    Vehicle(World& world, std::string model_name, const glm::vec3& color);

    virtual void Update() override;
    virtual void SendInitData(Player& player, net::OutMessage& msg) const override;

    void SetInput(VehicleInputType type, bool enable);
    void SetInputs(VehicleInputFlags inputs) { in_ = inputs; }

    glm::vec3 GetPosition() const;
    void SetPosition(const glm::vec3& pos);

    glm::quat GetRotation() const;
    float GetSpeed() const;

    void SetSteering(bool analog, float value = 0.0f);

    const std::string& GetModelName() const { return model_name_; }
    const std::shared_ptr<const assets::VehicleModel>& GetModel() const { return model_; }
    const glm::vec3& GetColor() const { return color_; }

    virtual ~Vehicle();

private:
    void ProcessInput();
    void UpdateWheels();
    void UpdateSyncState();
    VehicleSyncFieldFlags WriteState(net::OutMessage& msg, const VehicleSyncState& base) const;
    void SendUpdateMsg();

private:
    std::string model_name_;
    std::shared_ptr<const assets::VehicleModel> model_;
    glm::vec3 color_;

    collision::MotionState motion_;
    std::unique_ptr<btRigidBody> body_;
    std::unique_ptr<btRaycastVehicle> vehicle_;

    float steering_ = 0.0f;
    bool steering_analog_ = false;
    float target_steering_ = 0.0f;
    float wheel_z_offset_ = 0.0f;

    size_t num_wheels_ = 0;
    std::array<VehicleWheelState, MAX_WHEELS> wheels_;

    VehicleFlags flags_ = VF_NONE;
    VehicleSyncState sync_[2];
    size_t sync_current_ = 0;

    VehicleInputFlags in_ = 0;
};

} // namespace game