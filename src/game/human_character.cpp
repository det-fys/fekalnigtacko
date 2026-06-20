#include "human_character.hpp"
#include "drivable_vehicle.hpp"
#include "utils/random.hpp"

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
    UpdateDispersion();
    Super::Update();
}

void game::HumanCharacter::ReceiveDamage(const DamageInfo& damage)
{
    if (!IsAlive())
        return;

    Super::ReceiveDamage(damage);

    if (damage.inflictor && damage.inflictor != this)
    {
        damage.inflictor->OnDamageDealt(!IsAlive());
    }
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

void game::HumanCharacter::Equip(std::shared_ptr<ItemInstance> item)
{
    if (!IsAlive() && item)
        return;

    pending_item_ = std::move(item);
}

bool game::HumanCharacter::IsDead() const
{
    return actionstate_ == ACTION_DEAD;
}

game::HumanCharacter::~HumanCharacter()
{
    Ride(nullptr, 0); // exit rideable
}

bool game::HumanCharacter::HaveAmmo(const std::string& ammo_name)
{
    return true;
}

size_t game::HumanCharacter::GetAmmo(size_t required, const std::string& ammo_name)
{
    return required; // unlimited by default
}

bool game::HumanCharacter::IsOnFoot() const
{
    return state_ == HS_ON_FOOT;
}

int64_t game::HumanCharacter::GetTime() const
{
    return GetWorld().GetTime();
}

bool game::HumanCharacter::CanAim()
{
    return !NeedReload() && !PendingItemSwitch() && !(GetVehicle() && IsDriver() && !item_);
}

void game::HumanCharacter::SetAiming(bool aiming)
{
    if (aiming == GetAiming())
        return;

    Super::SetAiming(aiming);
    OnAimingChanged();
}

bool game::HumanCharacter::CanFire()
{
    return item_ && item_->ammo > 0 && ((GetTime() - last_fire_time_ + 40) >= item_->def->fire_delay);
}

void game::HumanCharacter::Fire()
{
    if (!item_)
        return; // fire wat?

    // PlaySound("airrifle_fire");
    SendFire();

    float range = 500.0f;
    // float dispersion = 5.0f; // m/100m
    
    game::BulletInfo bullet{};
    bullet.start = GetEyePosition();
    bullet.end = bullet.start + ApplyRandomDispersion(GetAimDirection(), dispersion_) * range;
    bullet.damage = item_->def->damage;
    bullet.shooter = this;
    GetWorld().FireBullet(bullet);

    last_fire_time_ = GetTime();
    dispersion_ = glm::min(dispersion_ + item_->def->dispersion_shot, item_->def->dispersion_max);

    // it should always be >0 but if it has been faked this far
    // nothing else can be done than to pretend that
    // the bullet was there
    if (item_->ammo > 0)
    { 
        --item_->ammo;
    }
}

bool game::HumanCharacter::NeedReload()
{
    return item_ && CanReload() && (item_->ammo == 0 || reloadheld_);
}

bool game::HumanCharacter::CanReload()
{
    return item_ && HaveAmmo(item_->def->ammo_type) && item_->ammo < item_->def->clip_size;
}

void game::HumanCharacter::Reload()
{
    if (!item_)
        return;

    item_->ammo += GetAmmo(item_->def->clip_size - item_->ammo, item_->def->ammo_type);
}

bool game::HumanCharacter::PendingItemSwitch()
{
    return pending_item_ != item_;
}

void game::HumanCharacter::SwitchItem()
{
    if (item_ == pending_item_)
        return;

    item_ = pending_item_;
    UpdateItemStuff();
    OnHeldItemChanged();
}

void game::HumanCharacter::UpdateItemStuff()
{
    // update legs anim
    if (state_ == HS_ON_FOOT)
    {
        if (item_ && !item_->def->legs_anim.empty())
            SetIdleAnim(item_->def->legs_anim);
        else
            SetIdleAnim("idle");

        // SetIdleAnim("idle_relaxed");
    }

    // update view item
    if (item_)
    {
        SetViewItem(item_->def->name);
        SetWeightSpeedMult(item_->def->walk_speed_mult);
    }
    else
    {
        SetViewItem("");
        SetWeightSpeedMult(1.0f);
    }
}

void game::HumanCharacter::ClearItem()
{
    auto item = item_;
    Equip(nullptr);
    SwitchItem();
}

void game::HumanCharacter::PlayItemActionAnim(const std::string assets::Item::*anim, float speed)
{
    if (!item_)
    {
        ClearActionAnim();
        return;
    }

    PlayActionAnim(item_->def.get()->*anim, speed);
}

void game::HumanCharacter::PlayDeathAnim()
{
    if (state_ == HS_ON_FOOT)
    {
        PlayActionAnim("die", 1.5f);
    }
    else if (state_ == HS_RIDING)
    {
        if (GetVehicle())
        {
            PlayActionAnim(IsDriver() ? "vehicle_drive_die" : "vehicle_passenger_die", 1.0f);
        }
        else
        {
            PlayActionAnim("vehicle_passenger_die_animal", 1.0f);
        }
    }
}

void game::HumanCharacter::TrySpawnLoot()
{
    if (loot_spawned_)
        return;
    
    loot_spawned_ = true;
    SpawnLoot();
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

    ResetActionState();
}

game::HumanCharacterState game::HumanCharacter::StateOnFootUpdate()
{
    if (PopSignal(HSS_RIDEABLE_CHANGED))
        return HS_INIT;

    if (PopSignal(HSS_KNOCK_DOWN))
        return HS_KNOCKED_DOWN;

    SetMovementType(IsAlive() ? (aimheld_ ? CMT_DIRECTIONAL : CMT_TURN) : CMT_DISABLED);

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

    ResetActionState();
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

void game::HumanCharacter::ResetActionState()
{
    item_.reset();
    EnterActionState(ACTION_IDLE);
    UpdateItemStuff();
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
    actionstate_start_ = GetWorld().GetTime();

    switch (state)
    {
    case ACTION_IDLE:
        SetAiming(false);
        SetCanSprint(true);
        PlayItemActionAnim(&assets::Item::idle_anim);
        break;

    case ACTION_RAISE:
        SwitchItem();
        SetAiming(false);
        SetCanSprint(true);
        PlayItemActionAnim(&assets::Item::raise_anim, 3.0f);
        break;

    case ACTION_AIM:
        SetAiming(true);
        SetCanSprint(false);
        PlayItemActionAnim(&assets::Item::aim_anim, 3.0f);
        break;

    case ACTION_AIMING:
        SetAiming(true);
        SetCanSprint(false);
        PlayItemActionAnim(&assets::Item::aiming_anim);
        break;

    case ACTION_FIRE:
        SetAiming(true);
        SetCanSprint(false);
        PlayItemActionAnim(&assets::Item::use_anim);
        Fire();
        break;

    case ACTION_FIRE_REPEAT:
        PlayItemActionAnim(&assets::Item::use_anim, -5.0f);
        break;

    case ACTION_RELOAD:
        // SetAiming(true);
        // SetCanSprint(true);
        PlayItemActionAnim(&assets::Item::reload_anim);
        break;

    case ACTION_UNAIM:
        SetAiming(false);
        SetCanSprint(false);
        PlayItemActionAnim(&assets::Item::aim_anim, -3.0f);
        break;

    case ACTION_PUTAWAY:
        SetAiming(false);
        SetCanSprint(true);
        PlayItemActionAnim(&assets::Item::raise_anim, -3.0f);
        break;
    
    case ACTION_DIE:
        SetAiming(false);
        SetCanSprint(false);
        PlayDeathAnim();
        break;

    case ACTION_DEAD:
        EnablePhysics(false);
        ClearItem();
        TrySpawnLoot();
        break;

    default:
        break;
    }
}

int64_t game::HumanCharacter::GetActionStateTime() const
{
    return GetWorld().GetTime() - actionstate_start_;
}

game::ActionState game::HumanCharacter::CheckActionStateTransition()
{
    switch (actionstate_)
    {
    case ACTION_IDLE:
        if (!IsAlive())
            return ACTION_DIE;

        if (PendingItemSwitch())
            return ACTION_PUTAWAY;

        if (NeedReload())
            return ACTION_RELOAD;

        if (aimheld_ && CanAim()) // want aim
            return ACTION_AIM;

        return ACTION_IDLE;

    case ACTION_RAISE:
        if (!IsAlive())
            return ACTION_DIE;

        if (PendingItemSwitch())
            return ACTION_PUTAWAY;

        if (IsActionAnimDone())
            return ACTION_IDLE;

        return ACTION_RAISE;

    case ACTION_AIM:
        if (!IsAlive())
            return ACTION_DIE;

        if (!aimheld_ || !CanAim())
            return ACTION_UNAIM;

        if (IsActionAnimDone())
            return ACTION_AIMING;

        return ACTION_AIM;

    case ACTION_AIMING:
        if (!IsAlive())
            return ACTION_DIE;

        if (!aimheld_ || !CanAim())
            return ACTION_UNAIM;

        if (fireheld_ && CanFire())
            return ACTION_FIRE;

        return ACTION_AIMING;

    case ACTION_FIRE:
        if (!IsAlive())
            return ACTION_DIE;

        if (IsActionAnimDone())
            return ACTION_AIMING;

        if (CanAim() && fireheld_ && CanFire())
            return ACTION_FIRE_REPEAT;

        return ACTION_FIRE;

    case ACTION_FIRE_REPEAT:
        // proxy to enter fire state again and reset anims and stuff
        if (GetActionStateTime() > 0)
            return ACTION_FIRE;

        return ACTION_FIRE_REPEAT; 

    case ACTION_RELOAD:
        if (!IsAlive())
            return ACTION_DIE;

        // SetAiming(aimheld_); // optional here
        if (IsActionAnimDone())
        {
            Reload();
            return ACTION_IDLE;
        }

        return ACTION_RELOAD;

    case ACTION_UNAIM:
        if (!IsAlive())
            return ACTION_DIE;

        if (aimheld_ && CanAim()) // start aiming again
            return ACTION_AIM;
        
        if (IsActionAnimDone())
            return ACTION_IDLE;

        return ACTION_UNAIM;

    case ACTION_PUTAWAY:
        if (!IsAlive())
            return ACTION_DIE;

        if (IsActionAnimDone())
            return ACTION_RAISE;

        if (!PendingItemSwitch()) // possibly player wants that item again
            return ACTION_RAISE;

        return ACTION_PUTAWAY;

    case ACTION_DIE:
        if (IsActionAnimDone())
            return ACTION_DEAD;
        
        return ACTION_DIE;
    
    case ACTION_DEAD: 
        return ACTION_DEAD; // no way back :(

    default:
        return actionstate_;
    }
}

void game::HumanCharacter::UpdateDispersion()
{
    if (!item_)
    {
        dispersion_ = 0.0f;
        return;
    }

    dispersion_ = glm::clamp(dispersion_ - (item_->def->dispersion_decay / 25.0f), item_->def->dispersion_min,
                             item_->def->dispersion_max);
}
