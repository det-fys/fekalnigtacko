#pragma once

#include "skeletoninstance.hpp"

namespace game
{

struct CharacterAnimState
{
    assets::AnimIdx idle_anim_idx = assets::NO_ANIM;
    assets::AnimIdx walk_anim_idx = assets::NO_ANIM;
    assets::AnimIdx run_anim_idx = assets::NO_ANIM;

    float loco_blend = 0.0f;
    float loco_phase = 0.0f;

    void ApplyToSkeleton(SkeletonInstance& sk) const;


};
} // namespace game
