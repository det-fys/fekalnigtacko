#pragma once

#include <memory>
#include "net/server.hpp"
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
    Client(Server& server, net::ConnId id);

    bool ProcessMessage(net::InMessage& msg);
    bool ProcessSingleMessage(net::MessageType type, net::InMessage& msg);

    void Update();

    void SendChat(const std::string& text);

    void Disconnect();

    net::ConnId GetConnId() const { return id_; }
    ClientState GetState() const { return state_; }

    game::Player* GetPlayer() { return player_.get(); }
    const game::Player* GetPlayer() const { return player_.get(); }

    ~Client();

private:
    void Send(std::string msg);

    bool ProcessLoginMsg(net::InMessage& msg);

private:
    Server& server_;
    net::ConnId id_ = 0;
    ClientState state_ = CS_INIT;

    std::unique_ptr<game::Player> player_;
    db::PlayerId player_id_ = 0;
};

}
