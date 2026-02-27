#include "dynamicsworld.hpp"

#include <stdexcept>

collision::DynamicsWorld::DynamicsWorld()
    : bt_dispatcher_(&bt_cfg_),
      bt_world_(&bt_dispatcher_, &bt_broadphase_, &bt_solver_, &bt_cfg_), bt_veh_raycaster_(&bt_world_)
{
    bt_world_.setGravity(btVector3(0, 0, -9.81f));

    bt_broadphase_.getOverlappingPairCache()->setInternalGhostPairCallback(&bt_ghost_pair_cb_);
}
