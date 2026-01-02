#pragma once

#include <map>

#include "world.hpp"

namespace game
{

class Player;

class Game
{
public:
    Game();

    void Update();

    void PlayerJoined(Player& player);
    void PlayerLeft(Player& player);

private:
    std::shared_ptr<World> default_world_;

};

}