#include "controllable_character.hpp"
#include "drivable_vehicle.hpp"

game::ControllableCharacter::ControllableCharacter(World& world, const CharacterTuning& tuning) : Character(world, tuning) {}

void game::ControllableCharacter::SetVehicle(DrivableVehicle* vehicle, uint32_t seat)
{
    if ((vehicle && vehicle_) || (!vehicle && !vehicle_))
        return;

    if (vehicle)
    {
        if (!vehicle->SetPassenger(seat, this))
            return;

        auto seat_loc = vehicle->GetModel()->GetLocation("seat" + std::to_string(seat));
        if (seat_loc)
            SetPosition(seat_loc->position);

        EnablePhysics(false);
        Attach(vehicle->GetEntNum());
        SetMainAnim(seat == 0 ? "vehicle_drive" : "vehicle_passenger");
        SetYaw(0.5f * glm::pi<float>());
    }
    else
    {
        vehicle_->SetPassenger(seat_idx_, nullptr);

        EnablePhysics(true);

        glm::vec3 seat_loc = vehicle_->GetModel()->GetLocation("seat" + std::to_string(seat_idx_))->position;
        seat_loc.x += glm::sign(seat_loc.x) * 0.5f; // to the side
        glm::vec3 pos = vehicle_->GetRoot().matrix * glm::vec4(seat_loc, 1.0f);
        pos.z += 0.5f;
        SetPosition(pos);
        
        Attach(0);
        SetMainAnim("idle");
        // SetYaw(0.0f);
    }

    vehicle_ = vehicle;
    seat_idx_ = seat;
    is_driver_ = vehicle && seat == 0;
    VehicleChanged();
}

game::ControllableCharacter::~ControllableCharacter()
{
    if (vehicle_)
    {
        vehicle_->SetPassenger(seat_idx_, nullptr);
    }
}
