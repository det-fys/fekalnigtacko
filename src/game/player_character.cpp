#include "player_character.hpp"
#include "world.hpp"

game::PlayerCharacter::PlayerCharacter(World& world, Player& player, const CharacterTuning& tuning) : Super(world, tuning), player_(&player)
{
    EnablePhysics(true);
    VehicleChanged();

    SetNametag(player.GetName());

    SendUseTargetInfo();
}

void game::PlayerCharacter::Update()
{
    UpdateUseTarget();
    Super::Update();
}

void game::PlayerCharacter::VehicleChanged()
{
    if (!player_)
        return;

    if (vehicle_)
    {
        player_->SetCamera(vehicle_->GetEntNum());
    }
    else
    {
        player_->SetCamera(GetEntNum());
    }

    UpdateInputs();
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

void game::PlayerCharacter::UpdateInputs()
{
    auto in = player_ ? player_->GetInput() : 0;
    CharacterInputFlags c_in = 0;
    VehicleInputFlags v_in = 0;

    if (in & (1 << IN_FORWARD))
    {
        c_in |= 1 << CIN_FORWARD;
        v_in |= 1 << VIN_FORWARD;
    }

    if (in & (1 << IN_BACKWARD))
    {
        c_in |= 1 << CIN_BACKWARD;
        v_in |= 1 << VIN_BACKWARD;
    }

    if (in & (1 << IN_LEFT))
    {
        c_in |= 1 << CIN_LEFT;
        v_in |= 1 << VIN_LEFT;
    }

    if (in & (1 << IN_RIGHT))
    {
        c_in |= 1 << CIN_RIGHT;
        v_in |= 1 << VIN_RIGHT;
    }

    if (in & (1 << IN_JUMP))
    {
        c_in |= 1 << CIN_JUMP;
    }
    
    if (in & (1 << IN_SPRINT))
    {
        c_in |= 1 << CIN_SPRINT;
    }

    if (vehicle_)
    {
        SetInputs(0);

        if (is_driver_)
        {
            vehicle_->SetInputs(v_in);
        }
    }
    else
    {
        SetInputs(c_in);
    }
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

    if (use_target_ && using_)
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
        if (vehicle_ && enabled)
            SetVehicle(nullptr, 0);// no use target and in vehicle -> exit

        return;
    }

    use_progress_ = 0.0f;
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
        player_->SendChat("už nejde nic použít");
        return;
    }

    std::string msg = "[E] " + use_target_->desc;

    if (using_)
    {
        msg += " (zbývá " + std::to_string(use_delay_ - use_progress_) + ")";
    }

    player_->SendChat(msg);
}
