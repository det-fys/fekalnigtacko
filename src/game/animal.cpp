#include "animal.hpp"

#include "player_character.hpp"
#include "input_mapping.hpp"

game::Animal::Animal(World& world, const CharacterTuning& tuning, const glm::vec3& position, float yaw)
    : Character(world, tuning), Usable(root_.matrix), Rideable(*this, RIDEABLE_ANIMAL)
{
    SetPosition(position);
    SetYaw(yaw);
    EnablePhysics(true);

    collision::AddObjectFlags(&GetController()->GetBtGhost(), collision::OF_USABLE);
}

bool game::Animal::QueryUseTarget(PlayerCharacter& character, uint32_t target_id, UseTargetQueryResult& res)
{
    if (character.GetRideable())
        return false; // already in something

    res.enabled = true;
    res.error_text = nullptr;
    
    bool seat_occupied = GetPassenger(target_id) != nullptr;
    res.delay = seat_occupied ? 2.0f : 0.25f;

    return true;
}

void game::Animal::Use(PlayerCharacter& character, uint32_t target_id)
{
    if (target_id >= GetNumSeats())
        return;

    character.Ride(this, target_id);
}

void game::Animal::SetRideableInput(PlayerInputFlags in)
{
    SetInputs(MapPlayerInputToCharacterInput(in));
}

void game::Animal::SetRideableYaw(float yaw)
{
    SetForwardYaw(yaw);
}

void game::Animal::OnPassengerChanged(size_t seat_idx, HumanCharacter* passenger)
{
    if (seat_idx == 0 && !passenger)
    {
        SetInputs(0);
    }
}

void game::Animal::SetUseMessage(const std::string& message)
{
    use_message_ = message;
}

void game::Animal::AddAnimalSeat(const glm::vec3& offset)
{
    size_t seat_idx = AddSeat(offset);
    use_targets_.emplace_back(this, static_cast<uint32_t>(seat_idx), offset + glm::vec3(0.0f, 0.0f, 1.0f),
                              use_message_ + " (místo " + std::to_string(seat_idx + 1) + ")");
}
