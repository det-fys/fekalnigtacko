#include "character_anim_state.hpp"
#include "utils/math.hpp"

void game::CharacterAnimState::ApplyToSkeleton(SkeletonInstance& sk) const
{
    const auto& skeleton = sk.GetSkeleton();
    const assets::Animation* idle_anim = skeleton->GetAnimation(idle_anim_idx);
    const assets::Animation* walk_anim = skeleton->GetAnimation(walk_anim_idx);
    const assets::Animation* run_anim = skeleton->GetAnimation(run_anim_idx);

    if (!idle_anim)
        return;

    if (!walk_anim)
        walk_anim = idle_anim;

    if (!run_anim)
        run_anim = walk_anim;

    if (loco_blend == 0.0f) // idle
    {
        sk.ApplySkelAnim(*idle_anim, loco_phase, 1.0f);
    }
    else if (loco_blend < 0.5f) // idle-walk
    {
        sk.ApplySkelAnim(*idle_anim, loco_phase, 1.0f);
        sk.ApplySkelAnim(*walk_anim, loco_phase, UnMix(0.0f, 0.5f, loco_blend));
    }

    else if (loco_blend == 0.5f) // walk
    {
        sk.ApplySkelAnim(*walk_anim, loco_phase, 1.0f);
    }
    else if (loco_blend < 1.0f) // walk-run
    {
        sk.ApplySkelAnim(*walk_anim, loco_phase, 1.0f);
        sk.ApplySkelAnim(*run_anim, loco_phase, UnMix(0.5f, 1.0f, loco_blend));
    }
    else // run
    {
        sk.ApplySkelAnim(*run_anim, loco_phase, 1.0f);
    }

}
