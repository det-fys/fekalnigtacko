#pragma once

#include <cstdint>

#include "net/defs.hpp"
#include "assets/skeleton.hpp"

namespace game
{

struct CharacterSyncState
{
    // transform
    net::PositionQ pos;
    net::PositiveAngleQ yaw;

    // idle
    assets::AnimIdx idle_anim = assets::NO_ANIM;

    // loco anim
    assets::AnimIdx walk_anim = assets::NO_ANIM;
    assets::AnimIdx run_anim = assets::NO_ANIM;
    net::AnimBlendQ loco_blend;
    net::AnimTimeQ loco_phase;
    //assets::AnimIdx strafe_left_anim = assets::NO_ANIM;
    //assets::AnimIdx strafe_right_anim = assets::NO_ANIM;

    // TODO: action
};

using CharacterSyncFieldFlags = uint8_t;

enum CharacterSyncFieldFlag
{
    CSF_TRANSFORM = 0x01,
    CSF_IDLE_ANIM = 0x02,
    CSF_LOCO_ANIMS = 0x04,
    CSF_LOCO_VALS = 0x08,
};

} // namespace game