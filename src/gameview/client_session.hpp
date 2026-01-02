#pragma once

#include <memory>

#include "worldview.hpp"

#include "gfx/draw_list.hpp"
#include "net/defs.hpp"
#include "net/inmessage.hpp"

class App;

namespace game::view
{

class ClientSession
{
public:
    ClientSession(App& app);

    bool ProcessMessage(net::InMessage& msg);
    bool ProcessSingleMessage(net::MessageType type, net::InMessage& msg);

    const WorldView* GetWorld() const { return world_.get(); } 

private:
    // msg handlers
    bool ProcessWorldMsg(net::InMessage& msg);

private:
    App& app_;
    
    std::unique_ptr<WorldView> world_;

};

} // namespace game::view