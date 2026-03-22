#include "world.hpp"

#include <iostream>
#include <stdexcept>

#include "assets/cache.hpp"
#include "collision/object_info.hpp"
#include "destroyed_object.hpp"
#include "utils/allocnum.hpp"
#include "player_character.hpp"

game::World::World(std::string mapname) : Scheduler(time_ms_), map_(*this, std::move(mapname)) {}

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

void game::World::DestructibleDestroyed(net::ObjNum num, std::unique_ptr<MapObjectCollision> col)
{
    auto& destroyed_obj = Spawn<DestroyedObject>(std::move(col));

    Schedule(120000, [this, num] { RespawnObj(num); });
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

struct UseTargetAabbCallback : public btBroadphaseAabbCallback
{
    game::PlayerCharacter& character;
    glm::vec3 pos;
    const game::UseTarget* best_target = nullptr;
    float best_dist = std::numeric_limits<float>::max();
    game::UseTargetQueryResult& best_res;

    UseTargetAabbCallback(game::PlayerCharacter& character, game::UseTargetQueryResult& res) : character(character), pos(character.GetRoot().GetGlobalPosition()), best_res(res) {}

    virtual bool process(const btBroadphaseProxy* proxy)
    {
        auto obj = reinterpret_cast<const btCollisionObject*>(proxy->m_clientObject);

        collision::ObjectType type;
        collision::ObjectFlags flags;
        collision::ObjectCallback* obj_cb;
        collision::GetObjectInfo(obj, type, flags, obj_cb);

        if ((flags & collision::OF_USABLE) == 0 || !obj_cb)
            return true;
        
        auto usable = dynamic_cast<game::Usable*>(obj_cb);
        if (!usable)
            return true;

        auto& matrix = usable->GetWSTransformMatrix();

        for (const auto& target : usable->GetUseTargets())
        {
            glm::vec3 pos_world = matrix * glm::vec4(target.position, 1.0f);

            float dist = glm::distance(pos, pos_world);
            if (dist < 3.0f && dist < best_dist)
            {
                if (!usable->QueryUseTarget(character, target.id, best_res))
                    continue;

                best_dist = dist;
                best_target = &target;
            }
        }

        return true;
    }
};

const game::UseTarget* game::World::GetBestUseTarget(game::PlayerCharacter& character, game::UseTargetQueryResult& res)
{
    const float radius = 5.0f;
    
    UseTargetAabbCallback cb(character, res);
    btVector3 min(cb.pos.x - radius, cb.pos.y - radius, cb.pos.z - radius);
    btVector3 max(cb.pos.x + radius, cb.pos.y + radius, cb.pos.z + radius);

    GetBtBroadphase().aabbTest(min, max, cb);
    return cb.best_target;
}

void game::World::HandleContacts()
{
    auto& bt_world = GetBtWorld();
    int numManifolds = bt_world.getDispatcher()->getNumManifolds();

    // destructibles
    static std::vector<net::ObjNum> to_destroy;
    to_destroy.clear();

    auto ProcessContact = [&](btRigidBody* body, btRigidBody* other_body, const btVector3& pos, const btVector3& normal,
                              float impulse) {
        collision::ObjectType type;
        collision::ObjectFlags flags;
        collision::ObjectCallback* cb;
        collision::GetObjectInfo(body, type, flags, cb);

        if (cb && (flags & collision::OF_NOTIFY_CONTACT))
        {
            collision::ContactInfo info;
            info.pos = glm::vec3(pos.x(), pos.y(), pos.z());
            info.normal = glm::vec3(normal.x(), normal.y(), normal.z());
            info.impulse = impulse;
            cb->OnContact(info);
        }

        if (type == collision::OT_MAP_OBJECT && (flags & collision::OF_DESTRUCTIBLE))
        {
            auto col = dynamic_cast<MapObjectCollision*>(cb);
            if (!col)
                return;

            if (impulse > col->GetDestroyThreshold())
            {
                to_destroy.push_back(col->GetNum());
                other_body->applyCentralImpulse(-normal * impulse * 0.5f);
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
            ProcessContact(body0, body1, pt.m_localPointA, -pt.m_normalWorldOnB, pt.getAppliedImpulse());
            ProcessContact(body1, body0, pt.m_localPointB, pt.m_normalWorldOnB, pt.getAppliedImpulse());
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
