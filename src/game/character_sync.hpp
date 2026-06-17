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
    net::AnimPhaseQ loco_phase;

    // action anim
    assets::AnimIdx action_anim = assets::NO_ANIM;
    net::AnimTimeQ action_time;

    // aim
    net::AnimAimAngleQ aim_yaw;
    net::AnimAimAngleQ aim_pitch;

    //assets::AnimIdx strafe_left_anim = assets::NO_ANIM;
    //assets::AnimIdx strafe_right_anim = assets::NO_ANIM;
};

using CharacterSyncFieldFlags = uint8_t;

enum CharacterSyncFieldFlag
{
    CSF_TRANSFORM = 1,
    CSF_IDLE_ANIM = 2,
    CSF_LOCO_ANIMS = 4,
    CSF_LOCO_VALS = 8,
    CSF_ACTION_ANIM = 16,
    CSF_ACTION_TIME = 32,
    CSF_AIM = 64,
};

} // namespace game