#include "worldview.hpp"

#include "assets/cache.hpp"

#include "characterview.hpp"
#include "vehicleview.hpp"
#include "client_session.hpp"

game::view::WorldView::WorldView(ClientSession& session) : 
    session_(session),
    audiomaster_(session_.GetAudioMaster())
{
    map_ = assets::CacheManager::GetMap("data/openworld.map");
}

bool game::view::WorldView::ProcessMsg(net::MessageType type, net::InMessage& msg)
{
    switch (type)
    {
    case net::MSG_ENTSPAWN:
        return ProcessEntSpawnMsg(msg);

    case net::MSG_ENTMSG:
        return ProcessEntMsgMsg(msg);

    case net::MSG_ENTDESTROY:
        return ProcessEntDestroyMsg(msg);

    default:
        return false;
    }
}

void game::view::WorldView::Update(const UpdateInfo& info)
{
    time_ = info.time;

    for (const auto& [entnum, ent] : ents_)
    {
        ent->Update(info);
    }
}

void game::view::WorldView::Draw(const DrawArgs& args) const
{
    if (map_)
        map_->Draw(args);

    for (const auto& [entnum, ent] : ents_)
    {
        if (args.frustum.IsSphereVisible(ent->GetBoundingSphere()))
            ent->Draw(args);
    }
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
        case net::ET_CHARACTER:
            entslot = std::make_unique<CharacterView>(*this, msg);
            break;
    
        case net::ET_VEHICLE:
            entslot = std::make_unique<VehicleView>(*this, msg);
            break;

        default:
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

bool game::view::WorldView::ProcessEntDestroyMsg(net::InMessage& msg)
{
    net::EntNum entnum;
    if (!msg.Read(entnum))
        return false;

    ents_.erase(entnum);
    return true;
}
