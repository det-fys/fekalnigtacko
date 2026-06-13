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

void game::HumanCharacter::Update()
{
    UpdateState();
    UpdateActionState();
    Super::Update();
}

void game::HumanCharacter::SetRideable(Rideable* rideable, size_t seat_idx)
{
    if (rideable == rideable_ && seat_idx == seat_idx_)
        return;
    
    if (rideable_)
    {
        glm::vec3 seat_loc = rideable_->GetSeatOffset(seat_idx_);
        seat_loc.x += glm::sign(seat_loc.x) * 0.5f; // to the side

        glm::vec3 pos = rideable_->GetEntity().GetRoot().matrix * glm::vec4(seat_loc, 1.0f);
        pos.z += 0.5f;

        rideable_exit_pos_ = pos;
    }

    rideable_ = rideable;
    vehicle_ = dynamic_cast<DrivableVehicle*>(rideable);
    seat_idx_ = seat_idx;
    is_driver_ = rideable && seat_idx_ == 0;

    SetSignal(HSS_RIDEABLE_CHANGED);
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

void game::HumanCharacter::SetAiming(bool aiming)
{
    if (aiming == GetAiming())
        return;

    Super::SetAiming(aiming);
    OnAimingChanged();
}

void game::HumanCharacter::Fire()
{
    PlaySound("airrifle_fire");

    game::BulletInfo bullet{};
    bullet.start = GetEyePosition();
    bullet.end = bullet.start + GetAimDirection() * 1000.0f;
    bullet.damage = 1.0f;
    bullet.shooter = this;
    GetWorld().FireBullet(bullet);
}

void game::HumanCharacter::UpdateState()
{
    struct HumanCharacterStateTableEntry
    {
        void (HumanCharacter::*enter)();
        HumanCharacterState (HumanCharacter::*update)();
        void (HumanCharacter::*exit)();
    };

    static const HumanCharacterStateTableEntry state_table[] =
    {
        // HS_INIT
        {
            nullptr,
            &HumanCharacter::StateInitUpdate,
            nullptr,
        },

        // HS_ON_FOOT
        {
            &HumanCharacter::StateOnFootEnter,
            &HumanCharacter::StateOnFootUpdate,
            nullptr,
        },

        // HS_RIDING
        {
            &HumanCharacter::StateRidingEnter,
            &HumanCharacter::StateRidingUpdate,
            &HumanCharacter::StateRidingExit,
        },

        // HS_KNOCKED_DOWN
        {
            nullptr,
            &HumanCharacter::StateKnockedDownUpdate,
            nullptr,
        },
    };

	while (true)
	{
		auto new_state = (this->*state_table[state_].update)();

        if (new_state == state_)
            break;

        if (auto exit_fun = state_table[state_].exit)
            (this->*exit_fun)();

        state_ = new_state;

        if (auto enter_fun = state_table[state_].enter)
            (this->*enter_fun)();
	}

    signals_ = 0;
}

void game::HumanCharacter::SetSignal(HumanCharacterStateSignal signal)
{
    signals_ |= signal;
}

bool game::HumanCharacter::PopSignal(HumanCharacterStateSignal signal)
{
    if (signals_ & signal)
    {
        signals_ &= ~signal;
        return true;
    }

    return false;
}

game::HumanCharacterState game::HumanCharacter::StateInitUpdate()
{
    if (GetRideable())
        return HS_RIDING;

    return HS_ON_FOOT;
}

void game::HumanCharacter::StateOnFootEnter()
{
    SetIdleAnim("idle");
    SetWalkAnim("walk");
    SetMovementType(CMT_TURN);
    EnablePhysics(true);

    EnterActionState(ACTION_IDLE);
}

game::HumanCharacterState game::HumanCharacter::StateOnFootUpdate()
{
    if (PopSignal(HSS_RIDEABLE_CHANGED))
        return HS_INIT;

    if (PopSignal(HSS_KNOCK_DOWN))
        return HS_KNOCKED_DOWN;

    SetMovementType(aimheld_ ? CMT_DIRECTIONAL : CMT_TURN);

    return HS_ON_FOOT;
}

void game::HumanCharacter::StateRidingEnter()
{
    auto rideable = GetRideable();
    SetPosition(rideable->GetSeatOffset(seat_idx_));
    EnablePhysics(false);

    Attach(rideable->GetEntity().GetEntNum());
    SetIdleAnim((rideable->GetRideableType() == RIDEABLE_VEHICLE && seat_idx_ == 0) ? "vehicle_drive" : "vehicle_passenger");
    SetYaw(0.0f);
    SetMovementType(CMT_DISABLED);

    EnterActionState(ACTION_IDLE);
}

game::HumanCharacterState game::HumanCharacter::StateRidingUpdate()
{
    if (!GetRideable())
        return HS_INIT;

    if (PopSignal(HSS_RIDEABLE_CHANGED))
        return HS_INIT;

    return HS_RIDING;
}

void game::HumanCharacter::StateRidingExit()
{
    EnablePhysics(true);
    SetPosition(rideable_exit_pos_);
    Attach(0);
}

game::HumanCharacterState game::HumanCharacter::StateKnockedDownUpdate()
{
    return HS_INIT;
}

void game::HumanCharacter::UpdateActionState()
{
    while (true)
    {
        auto new_state = CheckActionStateTransition();

        if (new_state == actionstate_)
            break;

        EnterActionState(new_state);
    }
}

void game::HumanCharacter::EnterActionState(ActionState state)
{
    actionstate_ = state;

    switch (state)
    {
    case ACTION_IDLE:
        if (state_ == HS_ON_FOOT)
            SetIdleAnim("idle_relaxed");
        SetAiming(false);
        PlayActionAnim("rifle_idle");
        break;

    case ACTION_AIM:
        SetViewItem("airsniper");
        SetAiming(true);
        PlayActionAnim("rifle_aim", 3.0f);
        break;

    case ACTION_AIMING:
        SetAiming(true);
        PlayActionAnim("rifle_aiming");
        break;

    case ACTION_FIRE:
        SetAiming(true);
        PlayActionAnim("rifle_fire");
        Fire();
        break;

    case ACTION_UNAIM:
        SetAiming(false);
        PlayActionAnim("rifle_aim", -3.0f);
        break;

    default:
        break;
    }
}

game::ActionState game::HumanCharacter::CheckActionStateTransition()
{
    switch (actionstate_)
    {
    case ACTION_IDLE:
        if (aimheld_) // want aim
            return ACTION_AIM;

        return ACTION_IDLE;

    case ACTION_AIM:
        if (IsActionAnimDone())
            return ACTION_AIMING;

        if (!aimheld_) // stop aiming immediately
            return ACTION_UNAIM;

        return ACTION_AIM;

    case ACTION_AIMING:
        if (!aimheld_)
            return ACTION_UNAIM; // wants aim no more

        if (fireheld_)
            return ACTION_FIRE;

        return ACTION_AIMING;

    case ACTION_FIRE:
        if (IsActionAnimDone())
            return ACTION_AIMING;

        return ACTION_FIRE;

    case ACTION_UNAIM:
        if (IsActionAnimDone())
            return ACTION_IDLE;

        if (aimheld_) // start aiming again
            return ACTION_AIM;

        return ACTION_UNAIM;

    default:
        return actionstate_;
    }
}
