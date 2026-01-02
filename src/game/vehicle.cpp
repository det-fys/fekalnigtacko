#include "vehicle.hpp"

#include "assets/cache.hpp"

static std::shared_ptr<const assets::VehicleModel> LoadVehicleModelByName(const std::string& model_name)
{
    return assets::CacheManager::GetVehicleModel("data/" + model_name + ".map");
}



game::Vehicle::Vehicle(World& world, std::string model_name) :
    Entity(world, net::ET_VEHICLE),
    model_name_(model_name),
    model_(LoadVehicleModelByName(model_name)),
    motion_(root_.local)
{
    // setup chassis rigidbody
    float mass = 300.0f;
    static btBoxShape shape(btVector3(1, 1, 1));
    
    btVector3 local_inertia(0, 0, 0);
    shape.calculateLocalInertia(mass, local_inertia);

    btRigidBody::btRigidBodyConstructionInfo rb_info(mass, &motion_, &shape, local_inertia);
    body_ = std::make_unique<btRigidBody>(rb_info);

    // setup vehicle
    btRaycastVehicle::btVehicleTuning tuning;
    vehicle_ = std::make_unique<btRaycastVehicle>(tuning, body_.get(), &world_.GetVehicleRaycaster());
    vehicle_->setCoordinateSystem(0, 2, 1);
    
}

