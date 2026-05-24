#include "drivable_vehicle.hpp"
#include "player_character.hpp"
#include "utils/random.hpp"

game::DrivableVehicle::DrivableVehicle(World& world, const VehicleTuning& tuning) : Vehicle(world, tuning), Usable(GetRoot().matrix)
{
    InitSeats();
    OnPhysicsChanged();
}

void game::DrivableVehicle::SetTuning(const VehicleTuning& tuning)
{
    Super::SetTuning(tuning);
    UpdateUseTargetNames(); // to update vehicle color in usetarget names
}

void game::DrivableVehicle::OnPhysicsChanged()
{
    // make body usable
    collision::AddObjectFlags(&GetPhysics()->GetBtBody(), collision::OF_USABLE);
}

bool game::DrivableVehicle::QueryUseTarget(PlayerCharacter& character, uint32_t target_id, UseTargetQueryResult& res)
{
    if (character.GetVehicle())
        return false; // already in vehicle

    res.enabled = true;
    res.error_text = nullptr;
    
    bool seat_occupied = seats_[target_id].occupant != nullptr;
    res.delay = seat_occupied ? 2.0f : 0.25f;

    return true;
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

    if (seat_idx == 0)
    {
        if (character)
        {
            SetLightsOn(true);
        }
        else
        {
            SetLightsOn(false);

            // clear inputs
            SetInputs(0);
            SetSteering(false, 0.0f);
        }

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

static char HexChar(uint32_t val)
{
    if (val < 10)
        return '0' + val;
    
    return 'a' + (val - 10);
}

static std::string GetColorTextPrefix(uint32_t color)
{
    std::string res = "^000";
    res[1] = HexChar((color >> 4) & 0xF);
    res[2] = HexChar((color >> 12) & 0xF);
    res[3] = HexChar((color >> 20) & 0xF);
    return res;
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
        seat.position.z += 1.0f; // the original pos is for animated character which is under vehicle
        seats_.emplace_back(seat);

        uint32_t id = seats_.size() - 1;
        use_targets_.emplace_back(this, id, seat.position, std::string());
    }

    UpdateUseTargetNames();
}

void game::DrivableVehicle::UpdateUseTargetNames()
{
    uint32_t color = GetTuningResult().colors[0];
    std::string prefix = "vlízt do " + GetColorTextPrefix(color) + GetModelName() + "^r";

    for (auto& target : use_targets_)
    {
        target.desc = prefix + " (místo " + std::to_string(target.id + 1) + ")";
    }
}
