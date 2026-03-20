#include "drivable_vehicle.hpp"
#include "player_character.hpp"
#include "utils/random.hpp"

game::DrivableVehicle::DrivableVehicle(World& world, const VehicleTuning& tuning) : Vehicle(world, tuning)
{
    InitSeats();
}

void game::DrivableVehicle::Use(PlayerCharacter& character, uint32_t target_id)
{
    if (target_id >= seats_.size())
        return;

    character.SetVehicle(this, target_id); // seat idx is same as target_id
    PlaySound("cardoor", 1.0f, RandomFloat(0.9f, 1.1f));

}

bool game::DrivableVehicle::SetPassenger(uint32_t seat_idx, ControllableCharacter* character)
{
    if (seat_idx >= seats_.size())
        return false;

    auto& seat_info = seats_[seat_idx];

    if (seat_info.occupant == character)
        return true; // already sitting here

    if (seat_info.occupant && character)
    {
        seat_info.occupant->SetVehicle(nullptr, 0); // remove current occupant
    }

    seat_info.occupant = character;

    if (seat_idx == 0 && !character)
    {
        // clear inputs
        SetInputs(0);
        SetSteering(false, 0.0f);
    }

    return true;
}

game::DrivableVehicle::~DrivableVehicle()
{
    // remove occupants
    for (auto& seat : seats_)
    {
        if (seat.occupant)
            seat.occupant->SetVehicle(nullptr, 0);
    }
}

void game::DrivableVehicle::InitSeats()
{
    const auto& veh = *GetModel();
    for (char c = '0'; c <= '9'; ++c)
    {
        auto trans = veh.GetLocation(std::string("seat") + c);
        if (!trans)
            break;

        VehicleSeat seat{};
        seat.position = trans->position;
        seats_.emplace_back(seat);

        uint32_t id = seats_.size() - 1;
        use_targets_.emplace_back(this, id, seat.position, "vlízt do " + GetModelName() + " (místo " + std::to_string(id) + ")");
    }
}
