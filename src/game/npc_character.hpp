#pragma once

#include "assets/map.hpp"
#include "controllable_character.hpp"

namespace game
{

class OpenWorld;

class NpcCharacter : public ControllableCharacter
{
public:
    using Super = ControllableCharacter;

    NpcCharacter(World& world);

    virtual void VehicleChanged() override;


    virtual void Update() override
    {
        Super::Update();

        VehicleThink();
    }

private:
    void SelectNextNode();
    void VehicleThink();

private:

    // driver
    const assets::MapGraph* roads_;
    glm::vec3 seg_start_;
    std::deque<size_t> path_;
    bool gas_ = false;
    size_t stuck_counter_ = 0;
    glm::vec3 last_pos_ = glm::vec3(0.0f);
    float speed_limit_ = 0.0f;
};

}