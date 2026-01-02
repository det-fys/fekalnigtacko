#pragma once

#include "world.hpp"

namespace game
{

class OpenWorld : public World
{
public:
    OpenWorld();

    virtual void PlayerJoined(Player& player) override;
    virtual void PlayerLeft(Player& player) override;

private:
    std::map<Player*, net::EntNum> player_vehicles_;

};

}