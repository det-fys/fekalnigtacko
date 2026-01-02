#pragma once

#include <map>

#include <btBulletDynamicsCommon.h>

#include "assets/map.hpp"

namespace collision
{

class DynamicsWorld
{
public:
    DynamicsWorld(std::shared_ptr<const assets::Map> map);

    btDynamicsWorld& GetBtWorld() { return bt_world_; }
    const btDynamicsWorld& GetBtWorld() const { return bt_world_; }
    btVehicleRaycaster& GetVehicleRaycaster() { return bt_veh_raycaster_; }

    const std::shared_ptr<const assets::Map>& GetMap() const { return map_; }
    
private:
    void AddMapCollision();
    void AddModelInstance(const assets::Model& model, const Transform& trans);

private:
    // this is BEFORE bt_world_!!!
    std::shared_ptr<const assets::Map> map_;
    std::vector<std::unique_ptr<btCollisionObject>> static_objs_;
    // ^-----

    btDefaultCollisionConfiguration bt_cfg_;
    btCollisionDispatcher bt_dispatcher_;
    btDbvtBroadphase bt_broadphase_;
    btSequentialImpulseConstraintSolver bt_solver_;
    btDiscreteDynamicsWorld bt_world_;
    btDefaultVehicleRaycaster bt_veh_raycaster_;

};

} // namespace collision