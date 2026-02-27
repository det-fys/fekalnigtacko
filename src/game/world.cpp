#include "world.hpp"

#include <stdexcept>
#include <iostream>

#include "assets/cache.hpp"
#include "utils/allocnum.hpp"
#include "collision/object_type.hpp"

game::World::World(std::string mapname) : map_(*this, std::move(mapname))
{
}

void game::World::SendInitData(Player& player, net::OutMessage& msg)
{
    msg.Write(net::MapName(map_.GetName()));

    msg.Write<net::ObjCount>(destroyed_objs_.size());
    for (auto objnum : destroyed_objs_)
    {
        msg.Write(objnum);
    }
}

net::EntNum game::World::GetNewEntnum()
{
    auto entnum = utils::AllocNum(ents_, last_entnum_);

    if (!entnum)
        throw std::runtime_error("Max entities reached");

    return entnum;
}

void game::World::RegisterEntity(std::unique_ptr<Entity> ent)
{
    auto& entslot = ents_[ent->GetEntNum()];

    if (entslot)
        throw std::runtime_error("Attempted to register entity with an occupied entnum");

    entslot = std::move(ent);
}

void game::World::Update(int64_t delta_time)
{
    time_ms_ += delta_time; 
    float delta_s = static_cast<float>(delta_time) * 0.001f;
    // GetBtWorld().stepSimulation(delta_s, 1, delta_s);
    GetBtWorld().stepSimulation(delta_s, 2, delta_s * 0.5f);

    DetectDestructibleCollisions();

    // update entities
    for (auto it = ents_.begin(); it != ents_.end();)
    {
        it->second->TryUpdate();

        if (it->second->IsRemoved())
            it = ents_.erase(it);
        else
            ++it;
    }
}

void game::World::FinishFrame()
{
    // reset ent msgs
    for (auto& [entnum, ent] : ents_)
    {
        ent->ResetMsg();
    }

}

game::Entity* game::World::GetEntity(net::EntNum entnum)
{
    auto it = ents_.find(entnum);
    if (it == ents_.end())
        return nullptr;

    return it->second.get();
}

void game::World::DetectDestructibleCollisions()
{
    auto& bt_world = GetBtWorld();
    int numManifolds = bt_world.getDispatcher()->getNumManifolds();

    static std::vector<net::ObjNum> to_destroy;
    to_destroy.clear();

    // std::cout << "Checking " << numManifolds << " manifolds for destructible collisions..." << std::endl;
    for (int i = 0; i < numManifolds; i++)
    {
        btPersistentManifold* contactManifold = bt_world.getDispatcher()->getManifoldByIndexInternal(i);

        const btRigidBody* bodyA = static_cast<const btRigidBody*>(contactManifold->getBody0());
        const btRigidBody* bodyB = static_cast<const btRigidBody*>(contactManifold->getBody1());

        const btRigidBody* destructibleBody = nullptr;

        if (bodyA->getUserIndex() == collision::OT_MAP_DESTRUCTIBLE)
            destructibleBody = bodyA;
        else if (bodyB->getUserIndex() == collision::OT_MAP_DESTRUCTIBLE)
            destructibleBody = bodyB;

        if (!destructibleBody)
            continue;

        for (int j = 0; j < contactManifold->getNumContacts(); j++)
        {
            const float break_threshold = 3000.0f; // TODO: per-object threshold

            btManifoldPoint& pt = contactManifold->getContactPoint(j);

            if (pt.getAppliedImpulse() > break_threshold)
            {
                std::cout << "Destructible collision detected: impulse = " << pt.getAppliedImpulse() << std::endl;

                MapObjectCollision* obj_col = static_cast<MapObjectCollision*>(destructibleBody->getUserPointer());
                to_destroy.push_back(obj_col->GetNum());

                const btRigidBody* otherBody = (destructibleBody == bodyA) ? bodyB : bodyA;
                btRigidBody* otherBodyNonConst = const_cast<btRigidBody*>(otherBody);
                otherBodyNonConst->applyCentralImpulse(pt.m_normalWorldOnB * pt.getAppliedImpulse() * 0.5f);
            }
        }
    }

    // destroy objs outside the loop to avoid corruption of the manifold list
    for (auto objnum : to_destroy)
    {
        DestroyObject(objnum);
    }
}

void game::World::DestroyObject(net::ObjNum objnum)
{
    SendObjDestroyedMsg(objnum);
    destroyed_objs_.insert(objnum);

    auto col = map_.DestroyObj(objnum);
    if (col)
    {
        DestructibleDestroyed(objnum, std::move(col));
    }
}

void game::World::SendObjDestroyedMsg(net::ObjNum objnum)
{
    auto msg = BeginMsg(net::MSG_OBJDESTROY);
    msg.Write(objnum);
}

void game::World::SendObjRespawnedMsg(net::ObjNum objnum)
{
    auto msg = BeginMsg(net::MSG_OBJRESPAWN);
    msg.Write(objnum);
}
