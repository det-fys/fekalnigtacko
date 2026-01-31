#include "dynamicsworld.hpp"

#include <stdexcept>

collision::DynamicsWorld::DynamicsWorld(std::shared_ptr<const assets::Map> map)
    : map_(std::move(map)), bt_dispatcher_(&bt_cfg_),
      bt_world_(&bt_dispatcher_, &bt_broadphase_, &bt_solver_, &bt_cfg_), bt_veh_raycaster_(&bt_world_)
{
    bt_world_.setGravity(btVector3(0, 0, -9.81f));

    AddMapCollision();

    btTransform t;
    t.setIdentity();
    t.setOrigin(btVector3(0,0,-12));

    // TODO: remove
    static btDefaultMotionState motion(t);
    static btBoxShape box(btVector3(100, 100, 2));
    btRigidBody::btRigidBodyConstructionInfo rbInfo(0.0f, &motion, &box, btVector3(0,0,0));
    static btRigidBody body(rbInfo);
    bt_world_.addRigidBody(&body);
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

    // for (const auto& sobjs = map_->GetStaticObjects(); const auto& sobj : sobjs)
    // {
    //     AddModelInstance(*sobj.model, sobj.node.local);
    // }
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
