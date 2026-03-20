#pragma once

#include "world.hpp"
#include "vehicle.hpp"
#include "character.hpp"
#include "usable.hpp"
#include "drivable_vehicle.hpp"

#include <optional>

namespace game
{

class PlayerCharacter;
class NpcCharacter;

class OpenWorld : public World
{
public:
    OpenWorld();

    virtual void Update(int64_t delta_time) override;

    virtual void PlayerJoined(Player& player) override;
    virtual void PlayerInput(Player& player, PlayerInputType type, bool enabled) override;
    virtual void PlayerViewAnglesChanged(Player& player, float yaw, float pitch) override;
    virtual void PlayerLeft(Player& player) override;

    virtual void DestructibleDestroyed(net::ObjNum num, std::unique_ptr<MapObjectCollision> col) override;

private:
    void CreatePlayerCharacter(Player& player);
    void RemovePlayerCharacter(Player& player);
    
    game::DrivableVehicle& SpawnRandomVehicle();
    void SpawnBot();

private:
    std::map<Player*, PlayerCharacter*> player_characters_;
    std::vector<NpcCharacter*> npcs_;
};

}