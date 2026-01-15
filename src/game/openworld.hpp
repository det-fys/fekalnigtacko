#pragma once

#include "world.hpp"
#include "vehicle.hpp"

namespace game
{

class OpenWorld : public World
{
public:
    OpenWorld();

    virtual void PlayerJoined(Player& player) override;
    virtual void PlayerInput(Player& player, PlayerInputType type, bool enabled) override;
    virtual void PlayerLeft(Player& player) override;

private:
    void SpawnVehicle(Player& player);
    void RemoveVehicle(Player& player);

private:
    std::map<Player*, Vehicle*> player_vehicles_;

};

}