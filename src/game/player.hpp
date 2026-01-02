#pragma once

#include <set>

#include "net/defs.hpp"
#include "net/inmessage.hpp"
#include "net/msg_producer.hpp"

#include "player_input.hpp"
#include "game.hpp"
#include "controllable.hpp"

namespace game
{

class World;
class Entity;
class Vehicle;

enum PlayerState
{
    PS_NONE,
    PS_CHARACTER,
    PS_VEHICLE,
};

class Player : public net::MsgProducer
{
public:
    Player(Game& game, std::string name);
    DELETE_COPY_MOVE(Player)

    bool ProcessMsg(net::MessageType type, net::InMessage& msg);
    void Update();

    void SetWorld(World* world);
    void Control(Controllable* ctl);

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

private:
    Game& game_;
    std::string name_;

    World* world_ = nullptr;
    World* known_world_ = nullptr;
    std::set<net::EntNum> known_ents_;

    PlayerInputFlags in_ = 0;

    PlayerState state_ = PS_NONE;
    Controllable* ctl_ = nullptr;

};

}