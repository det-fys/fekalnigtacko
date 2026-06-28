#pragma once

#include <map>

#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#include "assets/map.hpp"

namespace collision
{

struct DynamicsWorldInfo
{
    std::string broadphase{"Dbvt"};
    glm::vec3 bounds_min{};
    glm::vec3 bounds_max{};
};

class DynamicsWorld
{
public:
    DynamicsWorld();
    DynamicsWorld(const DynamicsWorldInfo& info);
    
    glm::vec3 CameraSweep(const glm::vec3& start, const glm::vec3& end);

    btDynamicsWorld& GetBtWorld() { return bt_world_; }
    const btDynamicsWorld& GetBtWorld() const { return bt_world_; }
    btBroadphaseInterface& GetBtBroadphase() { return *bt_broadphase_; }
    btVehicleRaycaster& GetVehicleRaycaster() { return bt_veh_raycaster_; }

private:
    btDefaultCollisionConfiguration bt_cfg_;
    btCollisionDispatcher bt_dispatcher_;
    btGhostPairCallback bt_ghost_pair_cb_;
    std::unique_ptr<btBroadphaseInterface> bt_broadphase_;
    btSequentialImpulseConstraintSolver bt_solver_;
    btDiscreteDynamicsWorld bt_world_;
    btDefaultVehicleRaycaster bt_veh_raycaster_;

};

} // namespace collision