#include "drivable_vehicle.hpp"
#include "player_character.hpp"
#include "npc_character.hpp"
#include "utils/random.hpp"
#include "input_mapping.hpp"

game::DrivableVehicle::DrivableVehicle(World& world, const VehicleSpawnInfo& info) : Vehicle(world, info), Usable(GetRoot().matrix), Rideable(*this, RIDEABLE_VEHICLE)
{
    InitSeats();
    OnPhysicsChanged();
}

void game::DrivableVehicle::Update()
{
    float daytime = world_.GetDayTime();
    SetLightsOn(GetPassenger(0) && (daytime < 6.0f || daytime > 18.0f));

    Super::Update();

    contact_ = false;
}

game::HumanCharacter* game::DrivableVehicle::GetResponsibleCharacter()
{
    return GetPassenger(0);
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
    if (character.GetRideable())
        return false; // already in something

    res.enabled = true;
    res.error_text = nullptr;
    
    bool seat_occupied = GetPassenger(target_id) != nullptr;
    res.delay = seat_occupied ? 2.0f : 0.25f;

    return true;
}

void game::DrivableVehicle::Use(PlayerCharacter& character, uint32_t target_id)
{
    if (target_id >= GetNumSeats())
        return;

    character.Ride(this, target_id);
    PlaySound("cardoor", 1.0f, RandomFloat(0.9f, 1.1f));
}

void game::DrivableVehicle::OnContact(const collision::ContactInfo& info)
{
    Super::OnContact(info);

    // TODO: move this logic to NpcCharacter but need to coordinate driver and passenger

    if (contact_)
        return;

    auto other_driver = info.other_cb ? info.other_cb->GetResponsibleCharacter() : nullptr;
    if (!other_driver)
        return;

    contact_ = true;

    auto is_player = dynamic_cast<PlayerCharacter*>(other_driver) != nullptr;
    if (!Chance(is_player ? 0.1f : 0.01f))
        return;

    auto my_driver = dynamic_cast<NpcCharacter*>(GetPassenger(0));
    if (!my_driver || !my_driver->IsArmed())
        return;

    // make passengers angry
    DamageInfo damage{};
    damage.type = DAMAGE_CRASH;
    damage.inflictor = other_driver;
    OnRideableDamaged(damage);
}

void game::DrivableVehicle::ReceiveDamage(const DamageInfo& damage)
{
    Super::ReceiveDamage(damage);
    OnRideableDamaged(damage);
}

void game::DrivableVehicle::SetRideableInput(PlayerInputFlags in)
{
    SetInputs(MapPlayerInputToVehicleInput(in));
}

void game::DrivableVehicle::OnPassengerChanged(size_t seat_idx, HumanCharacter* passenger)
{
    if (seat_idx == 0 && !passenger)
    {
        // driver left
        SetSteering(false, 0.0f);
        SetInputs(0);
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

        auto position = trans->position;
        size_t seat_idx = AddSeat(position);

        position.z += 1.0f; // the original pos is for animated character which is under vehicle
        use_targets_.emplace_back(this, static_cast<uint32_t>(seat_idx), position, std::string());
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
