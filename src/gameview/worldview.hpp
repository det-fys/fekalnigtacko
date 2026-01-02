#pragma once

#include "assets/map.hpp"
#include "gfx/draw_list.hpp"
#include "net/defs.hpp"
#include "net/inmessage.hpp"

#include "entityview.hpp"

namespace game::view
{

class WorldView
{
public:
    WorldView();

    bool ProcessMsg(net::MessageType type, net::InMessage& msg);

    void Draw(gfx::DrawList& dlist) const;

private:
    // msg handlers
    bool ProcessEntSpawnMsg(net::InMessage& msg);
    bool ProcessEntMsgMsg(net::InMessage& msg);
    bool ProcessEntDestroyMsg(net::InMessage& msg);

private:
    std::shared_ptr<const assets::Map> map_;

    std::map<net::EntNum, std::unique_ptr<EntityView>> ents_;
    
};

} // namespace game::view