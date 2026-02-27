#pragma once

#include <map>

#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#include "assets/map.hpp"

namespace collision
{

class DynamicsWorld
{
public:
    DynamicsWorld();
    
    btDynamicsWorld& GetBtWorld() { return bt_world_; }
    const btDynamicsWorld& GetBtWorld() const { return bt_world_; }
    btVehicleRaycaster& GetVehicleRaycaster() { return bt_veh_raycaster_; }

private:
    btDefaultCollisionConfiguration bt_cfg_;
    btCollisionDispatcher bt_dispatcher_;
    btGhostPairCallback bt_ghost_pair_cb_;
    btDbvtBroadphase bt_broadphase_;
    btSequentialImpulseConstraintSolver bt_solver_;
    btDiscreteDynamicsWorld bt_world_;
    btDefaultVehicleRaycaster bt_veh_raycaster_;

};

} // namespace collision