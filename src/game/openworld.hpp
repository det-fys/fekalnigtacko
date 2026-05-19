#pragma once

#include "enterable_world.hpp"
#include "npc_character.hpp"

namespace game
{

class OpenWorld : public EnterableWorld
{
public:
    using Super = EnterableWorld;

    OpenWorld();

    virtual void Update(int64_t delta_time) override;

private: 
    game::DrivableVehicle& SpawnRandomVehicle();
    void SpawnBot();

private:
    std::vector<NpcCharacter*> npcs_;
    float daytime_offset_ = 0.0f;
};

}