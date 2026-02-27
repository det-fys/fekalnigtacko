#include "mapinstance.hpp"

#include <stdexcept>

#include "assets/cache.hpp"
#include "collision/object_type.hpp"

game::MapInstance::MapInstance(collision::DynamicsWorld& world, std::string mapname)
    : world_(world), mapname_(std::move(mapname))
{
    map_ = assets::CacheManager::GetMap("data/" + mapname_ + ".map");

    // add basemodel col
    const auto& basemodel = map_->GetBaseModel();
    if (basemodel)
    {
        Transform identity;
        basemodel_col_ = std::make_unique<MapObjectCollision>(world_, basemodel, 0, identity, 0);
    }

    // add static objects
    const auto& objs = map_->GetStaticObjects();
    obj_cols_.resize(objs.size());
    for (size_t i = 0; i < objs.size(); ++i)
    {
        SpawnObj(static_cast<net::ObjNum>(i));
    }
}

void game::MapInstance::SpawnObj(net::ObjNum objnum)
{
    if (objnum >= obj_cols_.size())
        throw std::runtime_error("Invalid object number");

    size_t i = static_cast<size_t>(objnum);

    if (obj_cols_[i])
        return; // already spawned

    const auto& objs = map_->GetStaticObjects();
    obj_cols_[i] = std::make_unique<MapObjectCollision>(world_, objs[i].model, static_cast<net::ObjNum>(i), objs[i].node.local, MAPOBJ_DESTRUCTIBLE);
}

std::unique_ptr<game::MapObjectCollision> game::MapInstance::DestroyObj(net::ObjNum objnum)
{
    size_t i = static_cast<size_t>(objnum);
    if (i >= obj_cols_.size())
        throw std::runtime_error("Invalid object number");

    auto obj = std::move(obj_cols_[i]);

    if (obj)
    {
        obj->Break();
    }

    return obj;
}

game::MapObjectCollision::MapObjectCollision(collision::DynamicsWorld& world,
                                             std::shared_ptr<const assets::Model> model, net::ObjNum num,
                                             const Transform& trans, MapObjectCollisionFlags flags)
    : world_(world), model_(std::move(model)), num_(num)
{
    auto cshape = model_->GetColShape();
    auto cmesh = model_->GetColMesh();

    const bool destructible = (flags & MAPOBJ_DESTRUCTIBLE) != 0 && cshape != nullptr;

    collision::ObjectType obj_type = collision::OT_MAP_STATIC;

    btVector3 local_inertia(0, 0, 0);
    float mass = 0.0f;
    
    if (destructible)
    {
        obj_type = collision::OT_MAP_DESTRUCTIBLE;
    }

    // prefer simple cshape which allow destruction
    if (cshape)
    {
        body_ = std::make_unique<btRigidBody>(
            btRigidBody::btRigidBodyConstructionInfo(mass, nullptr, cshape, local_inertia));
    }
    else if (cmesh)
    {
        body_ = std::make_unique<btRigidBody>(
            btRigidBody::btRigidBodyConstructionInfo(mass, nullptr, cmesh->GetShape(), local_inertia));
    }

    body_->setWorldTransform(trans.ToBtTransform());
    body_->setUserIndex(static_cast<int>(obj_type));
    body_->setUserPointer(this);

    // world_.GetBtWorld().addRigidBody(body_.get(), btBroadphaseProxy::StaticFilter, btBroadphaseProxy::AllFilter);
    world_.GetBtWorld().addRigidBody(body_.get());
}

void game::MapObjectCollision::Break()
{
    if (!body_)
        return;

    // body_->setCollisionFlags(body_->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);

    float mass = 10.0f;
    btVector3 local_inertia(0, 0, 0);
    body_->getCollisionShape()->calculateLocalInertia(mass, local_inertia);

    body_->setMassProps(mass, local_inertia);
    body_->forceActivationState(ACTIVE_TAG);

    // set to undefined to avoid breaking again
    body_->setUserIndex(static_cast<int>(collision::OT_UNDEFINED));
}

game::MapObjectCollision::~MapObjectCollision()
{
    if (body_)
        world_.GetBtWorld().removeRigidBody(body_.get());
}
