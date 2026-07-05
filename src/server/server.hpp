#pragma once

#include <cstdint>
#include <memory>
#include <set>

#include "net/server.hpp"
#include "client.hpp"

#include "game/game.hpp"

namespace sv
{

struct ServerInfo
{
    std::unique_ptr<net::ServerInterface> iface;
    std::unique_ptr<game::Game> game;
};

class Server
{
public:
    Server(ServerInfo info);
    DELETE_COPY_MOVE(Server);

    void Run();

    void Send(Client& client, std::string msg);

    void Disconnect(Client& client);

    game::Game& GetGame() { return *game_; }

    int64_t GetTime() const { return time_; }

    void SetPlayerConnected(db::PlayerId player_id, bool connected);
    bool IsPlayerConnected(db::PlayerId player_id) const;

private:
    void PollWSEvents();
    void HandleWSConnect(net::ConnId conn);
    void HandleWSMessage(net::ConnId conn, const std::string& data);
    void HandleWSDisconnect(net::ConnId conn);

    void Update();

private:
    std::unique_ptr<net::ServerInterface> interface_;
    bool exit_ = false;

    std::unique_ptr<game::Game> game_;
    std::unordered_map<net::ConnId, std::unique_ptr<Client>> clients_;

    int64_t time_ = 0;

    std::set<db::PlayerId> connected_players_;
    
};

}