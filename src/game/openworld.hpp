#pragma once

#include "enterable_world.hpp"
#include "npc_character.hpp"

namespace game
{

class Game;

class OpenWorld : public EnterableWorld
{
public:
    using Super = EnterableWorld;

    OpenWorld(Game& game);

    virtual void Update(int64_t delta_time) override;
    virtual void PlayerInput(Player& player, PlayerInputType type, bool enabled) override;

private: 
    game::DrivableVehicle& SpawnRandomVehicle();
    game::NpcCharacter& SpawnRandomNpc();
    void SpawnBot();

    void CreateTuningGarage(const glm::vec3& position, float yaw);
    void CreateItemPickups(const std::string& loc_name, const std::string& item_name);
    void CreateItemPickup(const glm::vec3& position, const std::string& item);

    void RecoverPlayer(Player& player);
    bool GetRecoveryPosition(const glm::vec3& current, glm::vec3& recovery);

private:
    Game& game_;
    std::vector<NpcCharacter*> npcs_;
    float daytime_offset_ = 0.0f;
};

}