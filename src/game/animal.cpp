#include "animal.hpp"

#include "player_character.hpp"
#include "input_mapping.hpp"
#include "utils/random.hpp"

game::Animal::Animal(World& world, const CharacterTuning& tuning, const glm::vec3& position, float yaw)
    : Character(world, tuning), Usable(root_.matrix), Rideable(*this, RIDEABLE_ANIMAL)
{
    SetPosition(position);
    SetYaw(yaw);
    EnablePhysics(true);
    SetMovementType(CMT_TURN);

    collision::AddObjectFlags(&GetController()->GetBtGhost(), collision::OF_USABLE);
}

void game::Animal::Update()
{
    Think();
    just_hit_ = false;
    Super::Update();
}

void game::Animal::ReceiveDamage(const DamageInfo& damage)
{
    Super::ReceiveDamage(damage);
    just_hit_ = true;
    hit_from_ = damage.from_pos;
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
    if (think_state_ == ANIMAL_THINKSTATE_MOUNTED)
    {
        SetInputs(MapPlayerInputToCharacterInput(in));
    }
}

void game::Animal::SetRideableViewAngles(float yaw, float pitch)
{
    if (think_state_ == ANIMAL_THINKSTATE_MOUNTED)
    {
        SetViewAngles(yaw, pitch);
    }
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

bool game::Animal::IsMounted() const
{
    return GetPassenger(0) != nullptr;
}

void game::Animal::ChangeDirection()
{
    auto yaw = GetViewYaw();
    yaw += RandomFloat(-1.0f, 1.0f) * glm::half_pi<float>() * 0.5f;
    yaw = glm::mod(yaw, glm::two_pi<float>());
    SetViewAngles(yaw, 0.0f);
}

void game::Animal::TryMakeSound()
{
    auto time = GetWorld().GetTime();
    if (time - last_sound_time_ < 3000)
        return;

    last_sound_time_ = time;
    MakeSound();
}

void game::Animal::Think()
{
    while (true)
    {
        auto new_state = CheckThinkStateTransition();
        if (new_state == think_state_)
            break;

        EnterThinkState(new_state);
    }
}

static game::CharacterInputFlags GetRandomRoamInput()
{
    game::CharacterInputFlags in = 0;
    auto dir_choice = RandomFloat(0.0f, 1.0f);

    if (dir_choice < 0.4f)
        in |= 1 << game::CIN_FORWARD;
    else if (dir_choice < 0.6f)
        in |= (1 << game::CIN_FORWARD) | (1 << game::CIN_RIGHT);
    else if (dir_choice < 0.8f)
        in |= (1 << game::CIN_FORWARD) | (1 << game::CIN_RIGHT);

    return in;
}

static float GetAwayYaw(const glm::vec3& my_pos, const glm::vec3& enemy)
{
    auto away_dir = glm::normalize(glm::vec2(my_pos - enemy));
    auto yaw = glm::atan(-away_dir.x, away_dir.y);
    return yaw;
}

void game::Animal::EnterThinkState(AnimalThinkState state)
{
    think_state_ = state;
    think_state_start_ = GetWorld().GetTime();

    switch (state)
    {
    case ANIMAL_THINKSTATE_IDLE:
        SetInputs(0);
        break;

    case ANIMAL_THINKSTATE_ROAM:
        SetViewAngles(RandomFloat(0.0f, glm::two_pi<float>()), 0.0f);
        SetInputs(GetRandomRoamInput());
        SetWeightSpeedMult(0.3f);
        break;

    case ANIMAL_THINKSTATE_MOUNTED:
        SetInputs(0);
        SetWeightSpeedMult(1.0f);
        break;
        
    case ANIMAL_THINKSTATE_HURT:
        // SetInput(CIN_JUMP, true);
        SetWeightSpeedMult(1.0f);
        MakeHurtSound();
        break;

    case ANIMAL_THINKSTATE_RUN_AWAY:
        SetWeightSpeedMult(1.0f);
        SetInputs((1 << CIN_FORWARD) | (1 << CIN_SPRINT));
        SetViewAngles(GetAwayYaw(root_.GetGlobalPosition(), hit_from_), 0.0f);
        break;

    default:
        break;
    }
}

game::AnimalThinkState game::Animal::CheckThinkStateTransition()
{
    switch (think_state_)
    {
    case ANIMAL_THINKSTATE_IDLE:
        if (IsMounted())
            return ANIMAL_THINKSTATE_MOUNTED;

        if (just_hit_)
            return ANIMAL_THINKSTATE_HURT;

        if (ChanceAvgTime(5.0f) || GetCurrentThinkStateDuration() > 10000)
            return ANIMAL_THINKSTATE_ROAM;

        if (ChanceAvgTime(15.0f))
            TryMakeSound();

        return ANIMAL_THINKSTATE_IDLE;
    
    case ANIMAL_THINKSTATE_ROAM:
        if (just_hit_)
            return ANIMAL_THINKSTATE_HURT;
    
        if (ChanceAvgTime(5.0f) || GetCurrentThinkStateDuration() > 15000)
            return ANIMAL_THINKSTATE_IDLE;
        
        if (ChanceAvgTime(1.0f))
            ChangeDirection();

        return ANIMAL_THINKSTATE_ROAM;
    
    case ANIMAL_THINKSTATE_MOUNTED:
        if (!IsMounted())
            return ANIMAL_THINKSTATE_IDLE;

        if (just_hit_ && Chance(0.07f))
            return ANIMAL_THINKSTATE_HURT;

        return ANIMAL_THINKSTATE_MOUNTED;

    case ANIMAL_THINKSTATE_HURT:
        if (GetCurrentThinkStateDuration() > 0)
            return ANIMAL_THINKSTATE_RUN_AWAY;
            
        return ANIMAL_THINKSTATE_HURT;

    case ANIMAL_THINKSTATE_RUN_AWAY:
        if (!IsMounted() && GetCurrentThinkStateDuration() > 4000 &&
            (ChanceAvgTime(7.0f) || GetCurrentThinkStateDuration() > 9000))
            return ANIMAL_THINKSTATE_ROAM;

        if (IsMounted() && (ChanceAvgTime(1.0f) || GetCurrentThinkStateDuration() > 2000))
            return ANIMAL_THINKSTATE_IDLE;

        if (ChanceAvgTime(1.0f))
            ChangeDirection();

        return ANIMAL_THINKSTATE_RUN_AWAY;
        
    
    default:
        return ANIMAL_THINKSTATE_IDLE;
    }

}

int64_t game::Animal::GetCurrentThinkStateDuration() const
{
    return GetWorld().GetTime() - think_state_start_;
}
