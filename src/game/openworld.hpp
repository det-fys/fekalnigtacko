#pragma once

#include "world.hpp"
#include "vehicle.hpp"
#include "character.hpp"

namespace game
{

class OpenWorld : public World
{
public:
    OpenWorld();

    virtual void Update(int64_t delta_time) override;

    virtual void PlayerJoined(Player& player) override;
    virtual void PlayerInput(Player& player, PlayerInputType type, bool enabled) override;
    virtual void PlayerViewAnglesChanged(Player& player, float yaw, float pitch) override;
    virtual void PlayerLeft(Player& player) override;

private:
    void SpawnVehicle(Player& player);
    void RemoveVehicle(Player& player);

    void SpawnCharacter(Player& player);
    void RemoveCharacter(Player& player);

    void SpawnBot();

private:
    std::map<Player*, Vehicle*> player_vehicles_;
    std::map<Player*, Character*> player_characters_;
    std::vector<Vehicle*> bots_;
};

}