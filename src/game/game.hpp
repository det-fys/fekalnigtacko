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
    void FinishFrame();

    void PlayerJoined(Player& player);
    void PlayerLeft(Player& player);
    bool PlayerInput(Player& player, PlayerInputType type, bool enabled);


private:
    std::shared_ptr<World> default_world_;

};

}