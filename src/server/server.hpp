#pragma once

#include <cstdint>
#include <memory>
#include <set>

#include "net/server.hpp"
#include "client.hpp"

#include "game/game.hpp"

namespace sv
{

class Server
{
public:
    Server(std::unique_ptr<net::ServerInterface> iface);

    void Run();

    void Send(Client& client, std::string msg);

    void Disconnect(Client& client);

    game::Game& GetGame() { return game_; }

    int64_t GetTime() const { return time_; }

private:
    void PollWSEvents();
    void HandleWSConnect(net::ConnId conn);
    void HandleWSMessage(net::ConnId conn, const std::string& data);
    void HandleWSDisconnect(net::ConnId conn);

    void Update();

private:
    std::unique_ptr<net::ServerInterface> interface_;
    bool exit_ = false;

    game::Game game_;
    std::unordered_map<net::ConnId, std::unique_ptr<Client>> clients_;

    int64_t time_ = 0;
    
};

}