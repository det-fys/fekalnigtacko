#include "dynamicsworld.hpp"

#include <stdexcept>

collision::DynamicsWorld::DynamicsWorld(std::shared_ptr<const assets::Map> map)
    : map_(std::move(map)), bt_dispatcher_(&bt_cfg_),
      bt_world_(&bt_dispatcher_, &bt_broadphase_, &bt_solver_, &bt_cfg_), bt_veh_raycaster_(&bt_world_)
{
    AddMapCollision();
}

void collision::DynamicsWorld::AddMapCollision()
{
    if (!map_) // is perfectly possible that there is no map in this world
        return;

    // add basemodel
    const auto& basemodel = map_->GetBaseModel();
    if (basemodel)
    {
        Transform identity;
        AddModelInstance(*basemodel, identity);
    }

    // add static objects
    for (const auto& sobjs = map_->GetStaticObjects(); const auto& sobj : sobjs)
    {
        AddModelInstance(*sobj.model, sobj.transform);
    }
}

void collision::DynamicsWorld::AddModelInstance(const assets::Model& model, const Transform& trans)
{
    if (auto cmesh = model.GetColMesh(); cmesh)
    {
        // create trimesh object
        auto obj = std::make_unique<btCollisionObject>();
        obj->setCollisionShape(cmesh->GetShape());

        // set transform
        obj->setWorldTransform(trans.ToBtTransform());

        // add to world
        bt_world_.addCollisionObject(obj.get());
        static_objs_.emplace_back(std::move(obj));
    }

    for (const auto& shapes = model.GetColShapes(); const auto& shape : shapes)
    {
        // TODO: add basic shapes
    }
}
