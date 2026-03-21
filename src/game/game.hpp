#pragma once

#include <map>

#include "enterable_world.hpp"
#include "openworld.hpp"

namespace game
{

class Player;

struct PlayerGameInfo
{
    Player& player;
    EnterableWorld* world = nullptr;

    PlayerGameInfo(Player& player) : player(player) {}
};

class Game
{
public:
    Game();

    void Update();
    void FinishFrame();

    void PlayerJoined(Player& player);
    void PlayerViewAnglesChanged(Player& player, float yaw, float pitch);
    void PlayerInput(Player& player, PlayerInputType type, bool enabled);
    void PlayerLeft(Player& player);

private:
    void BroadcastChat(const std::string& text);

    void MovePlayerToWorld(PlayerGameInfo& player_info, EnterableWorld* new_world, const glm::vec3& pos, float yaw);

    EnterableWorld* FindPlayerWorld(Player& player) const;

private:
    std::shared_ptr<OpenWorld> openworld_;
    std::shared_ptr<EnterableWorld> testworld_;

    std::vector<World*> all_worlds_; // for common update etc.
    std::map<Player*, PlayerGameInfo> players_;

};

}