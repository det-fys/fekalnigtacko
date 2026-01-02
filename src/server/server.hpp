#pragma once

#include <cstdint>
#include <memory>
#include <set>

#include "wsserver.hpp"
#include "client.hpp"

#include "game/game.hpp"

namespace sv
{

class Server
{
public:
    Server(uint16_t port);

    void Run();

    void Send(Client& client, std::string msg);

    game::Game& GetGame() { return game_; }

private:
    void PollWSEvents();
    void HandleWSConnect(WSConnId conn);
    void HandleWSMessage(WSConnId conn, const std::string& data);
    void HandleWSDisconnect(WSConnId conn);

    void Update();

private:
    WSServer ws_;
    bool exit_ = false;

    game::Game game_;
    std::unordered_map<WSConnId, std::unique_ptr<Client>> clients_;

    
};

}