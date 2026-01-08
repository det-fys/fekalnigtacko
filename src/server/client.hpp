#pragma once

#include <memory>
#include "wsserver.hpp"
#include "net/defs.hpp"
#include "net/inmessage.hpp"
#include "game/player.hpp"

namespace sv
{

class Server;

enum ClientState
{
    CS_INIT,
    CS_PLAYER,
    CS_CLOSED,
};

class Client
{
public:
    Client(Server& server, WSConnId id);

    bool ProcessMessage(net::InMessage& msg);
    bool ProcessSingleMessage(net::MessageType type, net::InMessage& msg);

    void Update();

    // void Disconnect(const std::string& reason);

    WSConnId GetConnId() const { return id_; }
    ClientState GetState() const { return state_; }

    game::Player* GetPlayer() { return player_.get(); }
    const game::Player* GetPlayer() const { return player_.get(); }

private:
    void Send(std::string msg);

private:
    Server& server_;
    WSConnId id_ = 0;
    ClientState state_ = CS_INIT;

    std::unique_ptr<game::Player> player_;
};

}
