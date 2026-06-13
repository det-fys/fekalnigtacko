#include "player_character.hpp"
#include "world.hpp"
#include "input_mapping.hpp"

game::PlayerCharacter::PlayerCharacter(World& world, Player& player, const HumanCharacterTuning& tuning) : Super(world, tuning), player_(&player)
{
    EnablePhysics(true);
    UpdatePlayerCamera();
    SetNametag(player.GetName());
    SendUseTargetInfo();
}

void game::PlayerCharacter::Update()
{
    UpdateUseTarget();
    UpdateAimTarget();
    Super::Update();

    if (GetRideable() && IsDriver())
    {
        GetRideable()->SetRideableViewAngles(GetViewYaw(), GetViewPitch());
    }
}

void game::PlayerCharacter::ProcessInput(PlayerInputType type, bool enabled)
{
    switch (type)
    {
    case IN_USE:
        UseChanged(enabled);
        break;

    default:
        UpdateInputs();
        break;
    }
}

void game::PlayerCharacter::DetachFromPlayer()
{
    player_ = nullptr;
}

void game::PlayerCharacter::OnRideableChanged()
{
    UpdatePlayerCamera();
    UpdateInputs();
}

void game::PlayerCharacter::OnAimingChanged()
{
    UpdatePlayerCamera();
}

void game::PlayerCharacter::UpdatePlayerCamera()
{
    if (!player_)
        return;

    CameraInfo camera_info{};
    camera_info.character_entnum = GetEntNum();
    camera_info.rideable_entnum = GetRideable() ? GetRideable()->GetEntity().GetEntNum() : 0;

    if (GetAiming())
        camera_info.flags |= CAM_AIMING;

    player_->SetCamera(camera_info);
}

void game::PlayerCharacter::UpdateInputs()
{
    auto in = player_ ? player_->GetInput() : 0;

    if (auto rideable = GetRideable(); rideable)
    {
        SetInputs(0);

        if (IsDriver())
        {
            rideable->SetRideableInput(in);
        }
    }
    else
    {
        SetInputs(MapPlayerInputToCharacterInput(in));
    }

    SetAimHeld(in & (1 << IN_ATTACK_SECONDARY));
    SetFireHeld(in & (1 << IN_ATTACK_PRIMARY));
}

void game::PlayerCharacter::UpdateAimTarget()
{
    if (!player_)
        return;

    glm::vec3 eye, forward;
    if (!player_->GetView(eye, forward))
        return;

    auto target = eye + forward * 1000.0f;

    if (GetAiming()) // save perf if not aiming
    {
        GetWorld().TraceBullet(eye, target, this, target); // update target if hit
    }

    SetAimTarget(target);

    // GetWorld().Beam(eye, target, 0xFFFF00, 1.0 / 25.0f);
    GetWorld().BeamBox(target - 0.05f, target + 0.05f, 0xFFFF00, 1.5f / 25.0f);
}

void game::PlayerCharacter::UpdateUseTarget()
{
    UseTargetQueryResult res{};
    auto new_use_target = world_.GetBestUseTarget(*this, res);

    if (new_use_target != use_target_ || res.enabled != use_enabled_ || res.error_text != use_error_ || res.delay != use_delay_)
    {
        use_target_ = new_use_target;
        use_enabled_ = res.enabled;
        use_delay_ = res.delay;
        use_error_ = res.error_text;
        use_progress_ = 0.0f;
        using_ = false;
        
        SendUseTargetInfo();
    }

    if (use_target_ && use_enabled_ && using_)
    {
        use_progress_ += 0.04f;
        if (use_progress_ >= use_delay_)
        {
            using_ = false;
            use_progress_ = 0.0f;
            use_target_->usable->Use(*this, use_target_->id);
        }
    }

}

void game::PlayerCharacter::UseChanged(bool enabled)
{
    if (!use_target_)
    {
        // exit rideable if not target
        if (enabled && GetRideable())
            Ride(nullptr, 0);

        return;
    }

    use_progress_ = 0.0f;

    if (!use_enabled_)
        return;

    bool change = using_ != enabled;
    using_ = enabled;

    if (change)
        SendUseTargetInfo();

}

void game::PlayerCharacter::SendUseTargetInfo()
{
    if (!player_)
        return;

    if (!use_target_)
    {
        player_->SetUseTarget(std::string(), std::string(), 0.0f);
        return;
    }

    std::string error_text;
    if (use_error_)
        error_text = use_error_;

    player_->SetUseTarget(use_target_->desc, error_text, using_ ? use_delay_ - use_progress_ : 0.0f);
}
