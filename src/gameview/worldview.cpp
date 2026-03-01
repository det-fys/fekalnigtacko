#include "worldview.hpp"

#include "assets/cache.hpp"

#include "simple_entity_view.hpp"
#include "characterview.hpp"
#include "vehicleview.hpp"
#include "client_session.hpp"

game::view::WorldView::WorldView(ClientSession& session, net::InMessage& msg) : 
    session_(session), audiomaster_(session_.GetAudioMaster()), map_("openworld")
{
    net::MapName mapname;
    if (!msg.Read(mapname))
        throw EntityInitError();

    // init destroyed objs
    net::ObjCount objcount;
    if (!msg.Read(objcount))
        throw EntityInitError();

    for (net::ObjCount i = 0; i < objcount; ++i)
    {
        net::ObjNum objnum;
        if (!msg.Read(objnum))
            throw EntityInitError();

        map_.EnableObj(objnum, false);
    }

    // cache common snds and stuff
    Cache(assets::CacheManager::GetSound("data/breaksign.snd"));
    Cache(assets::CacheManager::GetSound("data/breakpatnik.snd"));
    Cache(assets::CacheManager::GetSound("data/breakwindow.snd"));
    Cache(assets::CacheManager::GetSound("data/breakwood.snd"));
    Cache(assets::CacheManager::GetSound("data/cardoor.snd"));
    Cache(assets::CacheManager::GetSound("data/crash.snd"));
}

bool game::view::WorldView::ProcessMsg(net::MessageType type, net::InMessage& msg)
{
    switch (type)
    {
    case net::MSG_ENTSPAWN:
        return ProcessEntSpawnMsg(msg);

    case net::MSG_ENTMSG:
        return ProcessEntMsgMsg(msg);
        
    case net::MSG_UPDATEENTS:
        return ProcessUpdateEntsMsg(msg);

    case net::MSG_ENTDESTROY:
        return ProcessEntDestroyMsg(msg);

    case net::MSG_OBJDESTROY:
        return ProcessObjDestroyOrRespawnMsg(msg, false);

    case net::MSG_OBJRESPAWN:
        return ProcessObjDestroyOrRespawnMsg(msg, true);

    default:
        return false;
    }
}

void game::view::WorldView::Update(const UpdateInfo& info)
{
    time_ = info.time;

    for (const auto& [entnum, ent] : ents_)
    {
        ent->TryUpdate(info);
    }
}

void game::view::WorldView::Draw(const DrawArgs& args) const
{
    map_.Draw(args);

    for (const auto& [entnum, ent] : ents_)
    {
        if (args.frustum.IsSphereVisible(ent->GetBoundingSphere()))
            ent->Draw(args);
    }
}

glm::vec3 game::view::WorldView::CameraSweep(const glm::vec3& start, const glm::vec3& end)
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

    bt_world.convexSweepTest(&shape, from, to, cb);

    if (!cb.hasHit())
        return end;

    return glm::mix(start, end, cb.m_closestHitFraction);
}

game::view::EntityView* game::view::WorldView::GetEntity(net::EntNum entnum)
{
    auto it = ents_.find(entnum);
    if (it != ents_.end())
        return it->second.get();

    return nullptr;
}

bool game::view::WorldView::ProcessEntSpawnMsg(net::InMessage& msg)
{
    net::EntNum entnum;
    net::EntType type;

    if (!msg.Read(entnum) || !msg.Read(type))
        return false;

    auto& entslot = ents_[entnum];
    if (entslot)
        entslot.reset();

    try
    {
        switch (type)
        {
        case net::ET_SIMPLE:
            entslot = std::make_unique<SimpleEntityView>(*this, msg);
            break;

        case net::ET_CHARACTER:
            entslot = std::make_unique<CharacterView>(*this, msg);
            break;
    
        case net::ET_VEHICLE:
            entslot = std::make_unique<VehicleView>(*this, msg);
            break;

        default:
            ents_.erase(entnum);
            return false; // unknown type
        }

        return true;

    } catch (const EntityInitError& e) // failed
    {
        ents_.erase(entnum);
        return false;
    }
}

bool game::view::WorldView::ProcessEntMsgMsg(net::InMessage& msg)
{
    net::EntNum entnum;
    net::EntMsgType type;

    if (!msg.Read(entnum) || !msg.Read(type))
        return false;

    auto ent_it = ents_.find(entnum);

    if (ent_it == ents_.end())
        return false;

    return ent_it->second->ProcessMsg(type, msg);
}

bool game::view::WorldView::ProcessUpdateEntsMsg(net::InMessage& msg)
{
    net::EntCount count;
    if (!msg.Read(count))
        return false;

    net::EntNum current = 0;
    auto it = ents_.begin();

    for (net::EntNum i = 0; i < count; ++i)
    {
        int64_t diff;
        if (!msg.ReadVarInt(diff))
            return false;
        
        current += static_cast<net::EntNum>(diff);

        while (it->first < current)
        {
            it->second->ProcessUpdateMsg(nullptr); // blank update
            ++it;
            if (it == ents_.end())
                return false;
        }

        if (it->first != current)
        {
            return false; // exact num wasnt found and it overrun 
        }

        if (!it->second->ProcessUpdateMsg(&msg))
            return false;
    }

    // process remaining
    for (; it != ents_.end(); ++it)
    {
        it->second->ProcessUpdateMsg(nullptr);
    }

    return true;
}

bool game::view::WorldView::ProcessEntDestroyMsg(net::InMessage& msg)
{
    net::EntNum entnum;
    if (!msg.Read(entnum))
        return false;

    ents_.erase(entnum);
    return true;
}

bool game::view::WorldView::ProcessObjDestroyOrRespawnMsg(net::InMessage& msg, bool enable)
{
    net::ObjNum objnum;
    if (!msg.Read(objnum))
        return false;

    map_.EnableObj(objnum, enable);
    return true;
}

void game::view::WorldView::Cache(std::any val)
{
    cache_.emplace_back(std::move(val));
}
