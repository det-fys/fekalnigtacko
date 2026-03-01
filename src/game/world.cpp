#include "world.hpp"

#include <stdexcept>
#include <iostream>

#include "assets/cache.hpp"
#include "utils/allocnum.hpp"
#include "collision/object_info.hpp"

game::World::World(std::string mapname) : Scheduler(time_ms_), map_(*this, std::move(mapname))
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

    HandleContacts();

    RunTasks();

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
    ResetMsg();

    // reset ent msgs
    for (auto& [entnum, ent] : ents_)
    {
        ent->FinalizeFrame();
    }

}

game::Entity* game::World::GetEntity(net::EntNum entnum)
{
    auto it = ents_.find(entnum);
    if (it == ents_.end())
        return nullptr;

    return it->second.get();
}

void game::World::RespawnObj(net::ObjNum objnum)
{
    if (destroyed_objs_.erase(objnum) > 0)
    {
        map_.SpawnObj(objnum);
        SendObjRespawnedMsg(objnum);
    }
}

void game::World::HandleContacts()
{
    auto& bt_world = GetBtWorld();
    int numManifolds = bt_world.getDispatcher()->getNumManifolds();

    // destructibles
    static std::vector<net::ObjNum> to_destroy;
    to_destroy.clear();

    auto ProcessContact = [&](btRigidBody* body, btRigidBody* other_body, btManifoldPoint& pt) {
        collision::ObjectType type;
        collision::ObjectFlags flags;
        collision::ObjectCallback* cb;
        collision::GetObjectInfo(body, type, flags, cb);

        if (cb && (flags & collision::OF_NOTIFY_CONTACT))
        {
            cb->OnContact(pt.getAppliedImpulse());
        }

        if (type == collision::OT_MAP_OBJECT && (flags & collision::OF_DESTRUCTIBLE))
        {
            auto col = dynamic_cast<MapObjectCollision*>(cb);
            if (!col)
                return;
                           
            if (pt.getAppliedImpulse() > col->GetDestroyThreshold())
            {
                to_destroy.push_back(col->GetNum());
                other_body->applyCentralImpulse(pt.m_normalWorldOnB * pt.getAppliedImpulse() * 0.5f);        
            }

            return;
        }
    };

    // std::cout << "Checking " << numManifolds << " manifolds for destructible collisions..." << std::endl;
    for (int i = 0; i < numManifolds; i++)
    {
        btPersistentManifold* contactManifold = bt_world.getDispatcher()->getManifoldByIndexInternal(i);

        btRigidBody* body0 = const_cast<btRigidBody*>(static_cast<const btRigidBody*>(contactManifold->getBody0()));
        btRigidBody* body1 = const_cast<btRigidBody*>(static_cast<const btRigidBody*>(contactManifold->getBody1()));

        for (int j = 0; j < contactManifold->getNumContacts(); j++)
        {
            btManifoldPoint& pt = contactManifold->getContactPoint(j);
            ProcessContact(body0, body1, pt);
            ProcessContact(body1, body0, pt);
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
