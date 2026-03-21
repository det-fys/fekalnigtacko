#pragma once

#include <array>
#include <cstddef>

#include "assets/vehiclemdl.hpp"
#include "collision/motionstate.hpp"
#include "collision/raycastvehicle.hpp"
#include "entity.hpp"
#include "world.hpp"
#include "vehicle_sync.hpp"
#include "deform_grid.hpp"
#include "vehicle_tuning.hpp"

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

    Vehicle(World& world, const VehicleTuning& tuning);

    virtual void Update() override;
    virtual void SendInitData(Player& player, net::OutMessage& msg) const override;

    virtual void OnContact(const collision::ContactInfo& info) override;

    void SetInput(VehicleInputType type, bool enable);
    void SetInputs(VehicleInputFlags inputs) { in_ = inputs; }

    glm::vec3 GetPosition() const;
    void SetPosition(const glm::vec3& pos);

    glm::quat GetRotation() const;
    float GetSpeed() const;

    void SetSteering(bool analog, float value = 0.0f);

    const std::string& GetModelName() const { return tuning_.model; }
    const std::shared_ptr<const assets::VehicleModel>& GetModel() const { return model_; }
    const VehicleTuning& GetTuning() const { return tuning_; }

    virtual ~Vehicle();

private:
    void ProcessInput();
    void UpdateCrash();
    void UpdateWheels();
    void UpdateSyncState();
    
    VehicleSyncFieldFlags WriteState(net::OutMessage& msg, const VehicleSyncState& base) const;
    void SendUpdateMsg();

    void WriteDeformSync(net::OutMessage& msg) const;
    void Deform(const glm::vec3& pos, const glm::vec3& deform, float radius);
    void SendDeformMsg(const net::PositionQ& pos, const net::PositionQ& deform);

    void WriteTuning(net::OutMessage& msg) const;

private:
    VehicleTuning tuning_;
    std::shared_ptr<const assets::VehicleModel> model_;

    collision::MotionState motion_;
    std::unique_ptr<btRigidBody> body_;
    std::unique_ptr<collision::RaycastVehicle> vehicle_;

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

    float window_health_ = 10000.0f;

    float crash_intensity_ = 0.0f;
    size_t no_crash_frames_ = 0;

    std::unique_ptr<DeformGrid> deformgrid_;
};

} // namespace game