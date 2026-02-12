#include "dynamicsworld.hpp"

#include <stdexcept>

collision::DynamicsWorld::DynamicsWorld()
    : bt_dispatcher_(&bt_cfg_),
      bt_world_(&bt_dispatcher_, &bt_broadphase_, &bt_solver_, &bt_cfg_), bt_veh_raycaster_(&bt_world_)
{
    bt_world_.setGravity(btVector3(0, 0, -9.81f));

    bt_broadphase_.getOverlappingPairCache()->setInternalGhostPairCallback(&bt_ghost_pair_cb_);
}

void collision::DynamicsWorld::AddMapCollision(std::shared_ptr<const assets::Map> map)
{
    if (!map)
        return;

    map_ = std::move(map);

    // add basemodel
    const auto& basemodel = map_->GetBaseModel();
    if (basemodel)
    {
        Transform identity;
        AddModelInstance(*basemodel, identity);
    }

    // add static objects
    for (const auto& chunks = map_->GetChunks(); const auto& chunk : chunks)
    {
        for (const auto& obj : chunk.objs)
        {
            AddModelInstance(*obj.model, obj.node.local);
        }
    }
}

void collision::DynamicsWorld::AddModelInstance(const assets::Model& model, const Transform& trans)
{
    if (auto cmesh = model.GetColMesh(); cmesh)
    {
        // create trimesh object
        btRigidBody::btRigidBodyConstructionInfo rbInfo(0.0f, nullptr, cmesh->GetShape(), btVector3(0,0,0));
        auto obj = std::make_unique<btRigidBody>(rbInfo);

        // set transform
        obj->setWorldTransform(trans.ToBtTransform());

        // add to world
        bt_world_.addRigidBody(obj.get());
        static_objs_.emplace_back(std::move(obj));
    }

    // TODO: add shape
}
