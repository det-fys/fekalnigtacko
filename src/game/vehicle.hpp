#pragma once

#include "entity.hpp"
#include "world.hpp"
#include "controllable.hpp"
#include "assets/vehiclemdl.hpp"
#include "collision/motionstate.hpp"

namespace game
{

class Vehicle : public Entity, public Controllable
{
public:
    using Super = Entity;

    Vehicle(World& world, std::string model_name);

    virtual void Update() override;
    virtual void SendInitData(Player& player, net::OutMessage& msg) const override;

    // Controllable
    Entity& GetEntity() override { return *this; };

    virtual ~Vehicle();

private:
    void ProcessInput();
    void SendUpdateMsg();

private:
    std::string model_name_;
    std::shared_ptr<const assets::VehicleModel> model_;

    collision::MotionState motion_;
    std::unique_ptr<btRigidBody> body_;
    std::unique_ptr<btRaycastVehicle> vehicle_;

    float steering_ = 0.0f;
};

}