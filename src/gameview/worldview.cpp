#include "worldview.hpp"

#include "assets/cache.hpp"

game::view::WorldView::WorldView()
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

void game::view::WorldView::Draw(gfx::DrawList& dlist) const
{
    if (map_)
        map_->Draw(dlist);
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

    switch (type)
    {

    default:
        return false;
    }

    return true;
}

bool game::view::WorldView::ProcessEntMsgMsg(net::InMessage& msg)
{
    return false;
}

bool game::view::WorldView::ProcessEntDestroyMsg(net::InMessage& msg)
{
    net::EntNum entnum;
    if (!msg.Read(entnum))
        return false;

    ents_.erase(entnum);
    return true;
}
