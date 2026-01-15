#pragma once

#include <set>

#include "net/defs.hpp"
#include "net/inmessage.hpp"
#include "net/msg_producer.hpp"

#include "player_input.hpp"
#include "game.hpp"

namespace game
{

class World;
class Entity;
class Vehicle;

class Player : public net::MsgProducer
{
public:
    Player(Game& game, std::string name);
    DELETE_COPY_MOVE(Player)

    bool ProcessMsg(net::MessageType type, net::InMessage& msg);
    void Update();

    void SetWorld(std::shared_ptr<World> world);

    void SetCamera(net::EntNum entnum);
    void SendChat(const std::string text);

    PlayerInputFlags GetInput() const { return in_; }

    ~Player();

private:
    void SendWorldMsg();

    // entities sync
    void SyncEntities();
    bool ShouldSeeEntity(const Entity& entity) const;
    void SendInitEntity(const Entity& entity);
    void SendUpdateEntity(const Entity& entity);
    void SendDestroyEntity(net::EntNum entnum);

    // msg handlers
    bool ProcessInputMsg(net::InMessage& msg);

    // events
    void Input(PlayerInputType type, bool enabled);

private:
    Game& game_;
    std::string name_;

    std::shared_ptr<World> world_ = nullptr;
    World* known_world_ = nullptr;
    std::set<net::EntNum> known_ents_;

    PlayerInputFlags in_ = 0;
};

}