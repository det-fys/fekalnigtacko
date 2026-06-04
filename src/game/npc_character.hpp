#pragma once

#include "assets/map.hpp"
#include "human_character.hpp"

namespace game
{

class OpenWorld;

enum NpcVehicleThinkState
{
    NVT_NORMAL,
    NVT_REVERSING,
};

class NpcCharacter : public HumanCharacter
{
public:
    using Super = HumanCharacter;

    NpcCharacter(World& world, const CharacterTuning& tuning);

    virtual void Update() override;

protected:
    virtual void OnRideableChanged() override;

private:
    void UpdateVehicleState();
    void SelectNextNode();
    void VehicleThink();

private:

    // driver
    NpcVehicleThinkState vehicle_state_ = NVT_NORMAL;
    const assets::MapGraph* roads_;
    glm::vec3 seg_start_;
    std::deque<size_t> path_;
    bool gas_ = false;
    size_t stuck_counter_ = 0;
    size_t reversing_frames_ = 0;
    glm::vec3 last_pos_ = glm::vec3(0.0f);
    float speed_limit_ = 0.0f;
};

}