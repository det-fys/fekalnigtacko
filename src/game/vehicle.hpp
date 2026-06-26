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

    void Update();

    btRigidBody& GetBtBody() { return *body_; }
    collision::RaycastVehicle& GetBtVehicle() { return *vehicle_; }

    void DisableAction();
    bool IsActionEnabled() const { return action_enabled_; }

    ~VehiclePhysics();

private:
    bool action_enabled_ = false;
    void UpdateBulletHitboxTransform();

private:
    collision::DynamicsWorld& world_;
    collision::MotionState motion_;
    std::unique_ptr<btRigidBody> body_;
    std::unique_ptr<collision::RaycastVehicle> vehicle_;
    std::unique_ptr<btCollisionObject> bullet_hitbox_;
};

struct VehicleSpawnInfo
{
    glm::vec3 position;
    float yaw;
    VehicleTuning tuning;
};

class Vehicle : public Entity
{
public:
    using Super = Entity;

    Vehicle(World& world, const VehicleSpawnInfo& info);

    virtual void Update() override;
    virtual void SendInitData(Player& player, net::OutMessage& msg) const override;

    virtual void OnContact(const collision::ContactInfo& info) override;
    virtual void ReceiveDamage(const DamageInfo& damage) override;

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

    void SetInvulnerable(bool invulnerable) { invulnerable_ = invulnerable; }
    void SetDestroyedRemoveTime(int64_t time) { destroyed_remove_time_ = time; }

private:
    void UpdateDestruction();
    void ProcessInput();
    void UpdateCrash();
    void UpdateWheels();
    void UpdateLights();
    void UpdateSyncState();

    VehicleSyncFieldFlags WriteState(net::OutMessage& msg, const VehicleSyncState& base) const;
    void SendUpdateMsg();

    void ApplyDamage(HumanCharacter* inflictor, float damage, float window_damage);

    void WriteDeformSync(net::OutMessage& msg) const;
    void Deform(const glm::vec3& pos, const glm::vec3& deform, float radius);
    void SendDeformMsg(const net::PositionQ& pos, const net::PositionQ& deform);
    void SendDeformSyncMsg();

    void ApplyTuning(const VehicleTuning& tuning);

    void WriteTuning(net::OutMessage& msg) const;

    void Explode();
    void RandomizeDeform();

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
    float steering_speed_ = 5.0f;

    std::vector<VehicleWheelState> wheels_;

    VehicleFlags flags_ = VF_NONE;
    VehicleSyncState sync_[2];
    size_t sync_current_ = 0;

    VehicleInputFlags in_ = 0;

    float health_ = 100.0f;
    bool exploded_ = false;
    float window_health_ = 100.0f;
    bool invulnerable_ = false;
    int64_t destroyed_remove_time_ = 50000;
    size_t explosion_timer_ = 0;
    net::EntNum destroyer_num_ = 0;

    float crash_intensity_ = 0.0f;
    size_t no_crash_frames_ = 0;
    glm::vec3 prev_velocity_ = glm::vec3(0.0f);

    std::unique_ptr<DeformGrid> deformgrid_;

    size_t wheels_on_ground_ = 0;
    size_t can_roll_frames_ = 0;

    bool lights_on_ = false;

};

} // namespace game