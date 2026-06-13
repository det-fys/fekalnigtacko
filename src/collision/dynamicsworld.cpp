#include "dynamicsworld.hpp"

#include <stdexcept>

collision::DynamicsWorld::DynamicsWorld()
    : bt_dispatcher_(&bt_cfg_),
      bt_world_(&bt_dispatcher_, &bt_broadphase_, &bt_solver_, &bt_cfg_), bt_veh_raycaster_(&bt_world_)
{
    bt_world_.setGravity(btVector3(0, 0, -9.81f));

    bt_broadphase_.getOverlappingPairCache()->setInternalGhostPairCallback(&bt_ghost_pair_cb_);
}

glm::vec3 collision::DynamicsWorld::CameraSweep(const glm::vec3& start, const glm::vec3& end)
{
    const auto& bt_world = GetBtWorld();

    static const btSphereShape shape(0.1f);

    btVector3 bt_start(start.x, start.y, start.z);
    btVector3 bt_end(end.x, end.y, end.z);

    btTransform from, to;
    from.setIdentity();
    from.setOrigin(bt_start);
    to.setIdentity();
    to.setOrigin(bt_end);

    btCollisionWorld::ClosestConvexResultCallback cb(bt_start, bt_end);
    cb.m_collisionFilterGroup = btBroadphaseProxy::DefaultFilter;
    cb.m_collisionFilterMask = btBroadphaseProxy::StaticFilter;

    bt_world.convexSweepTest(&shape, from, to, cb);

    if (!cb.hasHit())
        return end;

    return glm::mix(start, end, cb.m_closestHitFraction);
}
