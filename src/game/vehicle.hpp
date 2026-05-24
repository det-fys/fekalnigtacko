#pragma once

#include <array>
#include <cstddef>

#include "assets/vehiclemdl.hpp"
#include "collision/motionstate.hpp"
#include "collision/raycastvehicle.hpp"
#include "deform_grid.hpp"
#include "entity.hpp"
#include "vehicle_sync.hpp"
#include "vehicle_tuning.hpp"
#include "world.hpp"

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

class VehiclePhysics
{
public:
    VehiclePhysics(collision::DynamicsWorld& world, Transform& transform, collision::ObjectCallback& obj_cb,
                   const assets::VehicleModel& model, const VehicleTuningContext& tuning);

    DELETE_COPY_MOVE(VehiclePhysics)

    btRigidBody& GetBtBody() { return *body_; }
    collision::RaycastVehicle& GetBtVehicle() { return *vehicle_; }

    ~VehiclePhysics();

private:
    collision::DynamicsWorld& world_;
    collision::MotionState motion_;
    std::unique_ptr<btRigidBody> body_;
    std::unique_ptr<collision::RaycastVehicle> vehicle_;
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

    void SetPosition(const glm::vec3& pos);

    float GetSpeed() const;

    void SetSteering(bool analog, float value = 0.0f);

    void SetLightsOn(bool lights_on) { lights_on_ = lights_on; }
    bool GetLightsOn() const { return lights_on_; }

    virtual void SetTuning(const VehicleTuning& tuning);

    const std::string& GetModelName() const { return tuning_.model; }
    const std::shared_ptr<const assets::VehicleModel>& GetModel() const { return model_; }
    
    const VehicleTuning& GetTuning() const { return tuning_; }
    const std::shared_ptr<const VehicleTuningList>& GetTuningList() const { return tuninglist_; }
    const VehicleTuningContext& GetTuningResult() const { return tuning_ctx_; }

private:
    void ProcessInput();
    void UpdateCrash();
    void UpdateWheels();
    void UpdateLights();
    void UpdateSyncState();

    VehicleSyncFieldFlags WriteState(net::OutMessage& msg, const VehicleSyncState& base) const;
    void SendUpdateMsg();

    void WriteDeformSync(net::OutMessage& msg) const;
    void Deform(const glm::vec3& pos, const glm::vec3& deform, float radius);
    void SendDeformMsg(const net::PositionQ& pos, const net::PositionQ& deform);

    void ApplyTuning(const VehicleTuning& tuning);

    void WriteTuning(net::OutMessage& msg) const;

protected:
    VehiclePhysics* GetPhysics() { return physics_.get(); }
    virtual void OnPhysicsChanged() {}

private:
    VehicleTuning tuning_;
    std::shared_ptr<const assets::VehicleModel> model_;
    std::shared_ptr<const VehicleTuningList> tuninglist_;

    VehicleTuningContext tuning_ctx_;

    std::unique_ptr<VehiclePhysics> physics_;

    float steering_ = 0.0f;
    bool steering_analog_ = false;
    float target_steering_ = 0.0f;

    std::vector<VehicleWheelState> wheels_;

    VehicleFlags flags_ = VF_NONE;
    VehicleSyncState sync_[2];
    size_t sync_current_ = 0;

    VehicleInputFlags in_ = 0;

    float window_health_ = 10000.0f;

    float crash_intensity_ = 0.0f;
    size_t no_crash_frames_ = 0;

    std::unique_ptr<DeformGrid> deformgrid_;

    size_t wheels_on_ground_ = 0;
    size_t can_roll_frames_ = 0;

    bool lights_on_ = false;

};

} // namespace game