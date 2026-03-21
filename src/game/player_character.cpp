#include "player_character.hpp"
#include "world.hpp"

game::PlayerCharacter::PlayerCharacter(World& world, Player& player) : Super(world), player_(player)
{
    EnablePhysics(true);
    VehicleChanged();

    SetNametag(player.GetName());
}

void game::PlayerCharacter::Update()
{
    Super::Update();

    UpdateUseTarget();
}

void game::PlayerCharacter::VehicleChanged()
{
    if (vehicle_)
    {
        player_.SetCamera(vehicle_->GetEntNum());
    }
    else
    {
        player_.SetCamera(GetEntNum());
    }

    UpdateInputs();
}

void game::PlayerCharacter::ProcessInput(PlayerInputType type, bool enabled)
{
    switch (type)
    {
    case IN_USE:
        if (enabled)
        {
            if (!vehicle_)
            {
                auto use_target = world_.GetBestUseTarget(GetRootTransform().position);
                if (use_target)
                {
                    use_target->usable->Use(*this, use_target->id);
                }
            }
            else
            {
                SetVehicle(nullptr, 0);
            }

        }
        break;

    default:
        UpdateInputs();
        break;
    }
}

void game::PlayerCharacter::UpdateInputs()
{
    auto in = player_.GetInput();
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

void game::PlayerCharacter::UpdateUseTarget() {}
