#pragma once

#include "world.hpp"

namespace game
{

class Player;
class PlayerCharacter;
class HumanCharacterTuning;
class DrivableVehicle;

class EnterableWorld : public World
{
public:
    EnterableWorld(std::string mapname);

    // events
    virtual PlayerCharacter& InsertPlayer(Player& player, const HumanCharacterTuning& tuning, const glm::vec3& pos, float yaw);
    virtual void PlayerInput(Player& player, PlayerInputType type, bool enabled);
    virtual void PlayerViewAnglesChanged(Player& player, float yaw, float pitch);
    virtual void RemovePlayer(Player& player);

    virtual void OnVehicleJoined(DrivableVehicle& vehicle) {}

    virtual void OnPlayerLeaving(Player& player) {}

    PlayerCharacter* GetPlayerCharacter(Player& player);

private:
    PlayerCharacter& CreatePlayerCharacter(Player& player, const HumanCharacterTuning& tuning, const glm::vec3& position, float yaw);
    void RemovePlayerCharacter(Player& player);

private:
    std::map<Player*, PlayerCharacter*> player_characters_;


};


}