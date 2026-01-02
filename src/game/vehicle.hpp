#pragma once

#include "entity.hpp"
#include "world.hpp"
#include "assets/vehiclemdl.hpp"
#include "collision/motionstate.hpp"

namespace game
{

class Vehicle : public Entity
{
public:
    Vehicle(World& world, std::string model_name);

private:
    std::string model_name_;
    std::shared_ptr<const assets::VehicleModel> model_;

    collision::MotionState motion_;
    std::unique_ptr<btRigidBody> body_;
    std::unique_ptr<btRaycastVehicle> vehicle_;
};

}