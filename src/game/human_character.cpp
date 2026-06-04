#include "human_character.hpp"
#include "drivable_vehicle.hpp"

game::HumanCharacter::HumanCharacter(World& world, const CharacterTuning& tuning) : Character(world, tuning) {}

void game::HumanCharacter::SetRideable(Rideable* rideable, size_t seat_idx)
{
    if (rideable == rideable_ && seat_idx == seat_idx_)
        return;
        
    if (rideable)
    {
        SetPosition(rideable->GetSeatOffset(seat_idx));
        EnablePhysics(false);

        Attach(rideable->GetEntity().GetEntNum());
        SetMainAnim(seat_idx == 0 ? "vehicle_drive" : "vehicle_passenger");
        SetYaw(0.5f * glm::pi<float>());
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
        SetMainAnim("idle");
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
