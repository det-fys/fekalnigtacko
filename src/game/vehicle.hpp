#pragma once

#include <array>
#include <cstddef>

#include "assets/vehiclemdl.hpp"
#include "collision/motionstate.hpp"
#include "controllable.hpp"
#include "entity.hpp"
#include "world.hpp"
#include "vehicleflags.hpp"

namespace game
{

static constexpr size_t MAX_WHEELS = 4;

struct VehicleWheelState
{
    float rotation = 0.0f; // [rad]
    float speed = 0.0f;    // [rad/s]
    float z_offset = 0.0f; // [m] against model definition
};

class Vehicle : public Entity, public Controllable
{
public:
    using Super = Entity;

    Vehicle(World& world, std::string model_name, const glm::vec3& color);

    virtual void Update() override;
    virtual void SendInitData(Player& player, net::OutMessage& msg) const override;

    // Controllable
    Entity& GetEntity() override { return *this; };

    virtual ~Vehicle();

private:
    void ProcessInput();
    void UpdateWheels();
    void SendUpdateMsg();

private:
    std::string model_name_;
    std::shared_ptr<const assets::VehicleModel> model_;
    glm::vec3 color_;

    collision::MotionState motion_;
    std::unique_ptr<btRigidBody> body_;
    std::unique_ptr<btRaycastVehicle> vehicle_;

    float steering_ = 0.0f;
    float wheel_z_offset_ = 0.0f;

    size_t num_wheels_ = 0;
    std::array<VehicleWheelState, MAX_WHEELS> wheels_;

    VehicleFlags flags_;
};

} // namespace game