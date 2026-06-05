#include "human_character.hpp"
#include "drivable_vehicle.hpp"

static game::CharacterTuning GetCharacterTuning(const game::HumanCharacterTuning& tuning)
{
    game::CharacterTuning ct{};
    ct.shape = game::CharacterShape(0.3f, 0.75f);
    ct.model_name = "human";
    ct.clothes = tuning.clothes;

    return ct;
}

game::HumanCharacter::HumanCharacter(World& world, const HumanCharacterTuning& tuning) : Character(world, GetCharacterTuning(tuning)), human_tuning_(tuning)
{
    SetIdleAnim("idle");
    SetWalkAnim("walk");
}

void game::HumanCharacter::SetRideable(Rideable* rideable, size_t seat_idx)
{
    if (rideable == rideable_ && seat_idx == seat_idx_)
        return;
        
    if (rideable)
    {
        SetPosition(rideable->GetSeatOffset(seat_idx));
        EnablePhysics(false);

        Attach(rideable->GetEntity().GetEntNum());
        SetIdleAnim((rideable->GetRideableType() == RIDEABLE_VEHICLE && seat_idx == 0) ? "vehicle_drive" : "vehicle_passenger");
        SetYaw(rideable->GetRideableType() == RIDEABLE_VEHICLE ? 0.5f * glm::pi<float>() : 1.0f * glm::pi<float>());
    }
    else
    {
        EnablePhysics(true);

        glm::vec3 seat_loc = rideable_->GetSeatOffset(seat_idx_);
        seat_loc.x += glm::sign(seat_loc.x) * 0.5f; // to the side

        glm::vec3 pos = rideable_->GetEntity().GetRoot().matrix * glm::vec4(seat_loc, 1.0f);
        pos.z += 0.5f;
        SetPosition(pos);

        Attach(0);
        SetIdleAnim("idle");
    }

    rideable_ = rideable;
    vehicle_ = dynamic_cast<DrivableVehicle*>(rideable);
    seat_idx_ = seat_idx;
    is_driver_ = rideable && seat_idx_ == 0;

    OnRideableChanged();

}

void game::HumanCharacter::Ride(Rideable* rideable, size_t seat_idx)
{
    if (rideable_)
    {
        rideable_->SetPassenger(seat_idx_, 0);
    }

    if (rideable)
    {
        rideable->SetPassenger(seat_idx, this);
    }
}

game::HumanCharacter::~HumanCharacter()
{
    Ride(nullptr, 0); // exit rideable
}
