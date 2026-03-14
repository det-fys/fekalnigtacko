#pragma once

#include <map>
#include <set>

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
    void BroadcastChat(const std::string& text);

private:
    std::shared_ptr<World> default_world_;
    std::set<Player*> players_;

};

}