#include "mapinstance.hpp"

#include <stdexcept>

#include "assets/cache.hpp"

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

std::unique_ptr<game::MapObjectCollision> game::MapInstance::DestroyObj(net::ObjNum objnum, const MapObjectBreakInfo& info)
{
    size_t i = static_cast<size_t>(objnum);
    if (i >= obj_cols_.size())
        throw std::runtime_error("Invalid object number");

    auto obj = std::move(obj_cols_[i]);

    if (obj)
    {
        obj->Break(info);
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

    btVector3 local_inertia(0, 0, 0);
    float mass = 0.0f;
    collision::ObjectFlags oflags = 0;

    if (destructible)
    {
        oflags |= collision::OF_DESTRUCTIBLE;
        model_->GetParamFloat("destr_th", destr_th_);
    }

    int col_mask = ~collision::OG_STATIC; 

    // prefer simple cshape which allow destruction
    if (cshape)
    {
        body_ = std::make_unique<btRigidBody>(
            btRigidBody::btRigidBodyConstructionInfo(mass, nullptr, cshape, local_inertia));

        if (!model_->IsColShapeBulletTarget())
        {
            col_mask &= ~collision::OG_PROJECTILE;
            no_projectile_collision_ = true;
        }
    }
    else if (cmesh)
    {
        body_ = std::make_unique<btRigidBody>(
            btRigidBody::btRigidBodyConstructionInfo(mass, nullptr, cmesh->GetShape(), local_inertia));
    }

    auto offset_trans = trans;
    offset_trans.position += trans.rotation * model_->GetColOffset();

    body_->setWorldTransform(offset_trans.ToBtTransform());

    collision::SetObjectInfo(body_.get(), collision::OT_MAP_OBJECT, oflags, this);

    // world_.GetBtWorld().addRigidBody(body_.get(), btBroadphaseProxy::StaticFilter, btBroadphaseProxy::AllFilter);
    world_.GetBtWorld().addRigidBody(body_.get(), collision::OG_STATIC, col_mask);
}

void game::MapObjectCollision::Break(const MapObjectBreakInfo& info)
{
    if (!body_)
        return;

    btCollisionShape* shape = body_->getCollisionShape();
    float mass = 10.0f;
    model_->GetParamFloat("destr_mass", mass); // dont care if not present
    btVector3 local_inertia(0, 0, 0);
    shape->calculateLocalInertia(mass, local_inertia);

    btTransform trans = body_->getWorldTransform();

    // remove old
    world_.GetBtWorld().removeRigidBody(body_.get());
    body_.reset();
    
    // make new
    body_ = std::make_unique<btRigidBody>(btRigidBody::btRigidBodyConstructionInfo(mass, nullptr, shape, local_inertia)); 
    body_->setWorldTransform(trans);
    
    collision::SetObjectInfo(body_.get(), collision::OT_UNDEFINED, 0, this);
    
    int col_mask = collision::OG_ALL;
    if (no_projectile_collision_)
        col_mask &= ~collision::OG_PROJECTILE;

    world_.GetBtWorld().addRigidBody(body_.get(), collision::OG_DEFAULT, col_mask);

    if (info.impulse > 0.0f)
    {
        auto normal = glm::normalize(info.hit_pos - info.from_pos);
        auto impulse = normal * info.impulse * body_->getMass() * 0.003f;
        body_->applyImpulse(btVector3(impulse.x, impulse.y, impulse.z),
                            btVector3(info.hit_pos.x, info.hit_pos.y, info.hit_pos.z) -
                                body_->getCenterOfMassPosition());
    }

}

void game::MapObjectCollision::GetModelTransform(Transform& trans) const
{
    trans.SetBtTransform(body_->getWorldTransform());
    trans.position -= trans.rotation * model_->GetColOffset(); // unapply offset
}

game::MapObjectCollision::~MapObjectCollision()
{
    if (body_)
        world_.GetBtWorld().removeRigidBody(body_.get());
}
