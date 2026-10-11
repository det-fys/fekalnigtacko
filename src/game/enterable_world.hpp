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
    EnterableWorld(const collision::DynamicsWorldInfo& col_info, const WorldInfo& world_info);

    virtual void Update(int64_t delta_time) override;

    // events
    virtual PlayerCharacter& InsertPlayer(Player& player, const HumanCharacterTuning& tuning, const glm::dvec3& pos,
                                          float yaw);
    virtual void PlayerInput(Player& player, PlayerInputType type, bool enabled);
    virtual void PlayerViewAnglesChanged(Player& player, float yaw, float pitch);
    virtual void RemovePlayer(Player& player);

    virtual void OnVehicleJoined(DrivableVehicle& vehicle) {}

    virtual void OnPlayerLeaving(Player& player) {}

    PlayerCharacter* GetPlayerCharacter(Player& player);

    void SetSpawnPoint(const glm::dvec3& pos) { spawnpoint_ = pos; }
    const glm::dvec3& GetSpawnPoint() const { return spawnpoint_; }

private:
    PlayerCharacter& CreatePlayerCharacter(Player& player, const HumanCharacterTuning& tuning,
                                           const glm::dvec3& position, float yaw);
    void RemovePlayerCharacter(Player& player);

    void DrawNavMeshBeams();

private:
    std::map<Player*, PlayerCharacter*> player_characters_;
    glm::dvec3 spawnpoint_{};

};


}