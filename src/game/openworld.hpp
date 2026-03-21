#pragma once

#include "enterable_world.hpp"
#include "npc_character.hpp"

namespace game
{

class OpenWorld : public EnterableWorld
{
public:
    OpenWorld();

private: 
    game::DrivableVehicle& SpawnRandomVehicle();
    void SpawnBot();

private:
    std::vector<NpcCharacter*> npcs_;
};

}