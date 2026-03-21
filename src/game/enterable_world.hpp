#pragma once

#include "world.hpp"
#include "drivable_vehicle.hpp"

namespace game
{

class Player;
class PlayerCharacter;

class EnterableWorld : public World
{
public:
    EnterableWorld(std::string mapname);

    // events
    virtual void InsertPlayer(Player& player, const glm::vec3& pos, float yaw);
    virtual void PlayerInput(Player& player, PlayerInputType type, bool enabled);
    virtual void PlayerViewAnglesChanged(Player& player, float yaw, float pitch);
    virtual void RemovePlayer(Player& player);

private:
    PlayerCharacter& CreatePlayerCharacter(Player& player, const glm::vec3& position, float yaw);
    void RemovePlayerCharacter(Player& player);

private:
    std::map<Player*, PlayerCharacter*> player_characters_;


};


}