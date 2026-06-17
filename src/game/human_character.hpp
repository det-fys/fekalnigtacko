#pragma once

#include "character.hpp"
#include "item_instance.hpp"

namespace game
{

struct HumanCharacterTuning
{
    std::vector<CharacterConfigClothes> clothes;
};

class Rideable;
class DrivableVehicle;

using HumanCharacterStateSignals = uint32_t;

enum HumanCharacterStateSignal : HumanCharacterStateSignals
{
    HSS_KNOCK_DOWN = 1,
    HSS_RIDEABLE_CHANGED = 2,
};

enum HumanCharacterState
{
    HS_INIT,
    HS_ON_FOOT,
    HS_RIDING,
    HS_KNOCKED_DOWN,
};

enum ActionState
{
    ACTION_IDLE,
    ACTION_RAISE,
    ACTION_AIM,
    ACTION_AIMING,
    ACTION_FIRE,
    ACTION_FIRE_REPEAT,
    ACTION_RELOAD,
    ACTION_UNAIM,
    ACTION_PUTAWAY,
};

class HumanCharacter : public Character
{
public:
    using Super = Character;
    
    HumanCharacter(World& world, const HumanCharacterTuning& tuning);
    
    virtual void Update() override;

    const HumanCharacterTuning& GetHumanTuning() const { return human_tuning_; }

    void SetRideable(Rideable* rideable, size_t seat_idx); // called by Rideable!!
    void Ride(Rideable* rideable, size_t seat_idx);
    
    Rideable* GetRideable() const { return rideable_; }
    DrivableVehicle* GetVehicle() const { return vehicle_; }

    size_t GeatSeatIdx() const { return seat_idx_; }
    bool IsDriver() const { return is_driver_; }
    
    void SetAimHeld(bool aimheld) { aimheld_ = aimheld; }
    void SetFireHeld(bool fireheld) { fireheld_ = fireheld; }
    void SetReloadHeld(bool reloadheld) { reloadheld_ = reloadheld; }

    void Equip(std::shared_ptr<ItemInstance> item);
    const std::shared_ptr<ItemInstance>& GetHeldItem() const { return item_; }

    virtual ~HumanCharacter() override;

protected:
    virtual void OnRideableChanged() {}
    virtual void OnAimingChanged() {}
    virtual void OnHeldItemChanged() {}
    virtual bool HaveAmmo(const std::string& ammo_name);
    virtual size_t GetAmmo(size_t required, const std::string& ammo_name);

private:
    int64_t GetTime() const;
    bool CanAim();
    void SetAiming(bool aiming);
    bool CanFire();
    void Fire();
    bool NeedReload();
    bool CanReload();
    void Reload();
    bool PendingItemSwitch();
    void SwitchItem();
    void UpdateItemStuff();
    void PlayItemActionAnim(const std::string assets::Item::*anim, float speed = 1.0f);

    void UpdateState();
    void SetSignal(HumanCharacterStateSignal signal);
    bool PopSignal(HumanCharacterStateSignal signal);

    //void StateInitEnter();
    HumanCharacterState StateInitUpdate();
    //void StateInitExit(); 

    void StateOnFootEnter();
    HumanCharacterState StateOnFootUpdate();
    //void StateOnFootExit();

    void StateRidingEnter();
    HumanCharacterState StateRidingUpdate();
    void StateRidingExit();

    // void StateKnockedDownEnter();
    HumanCharacterState StateKnockedDownUpdate();
    // void StateKnockedDownExit();

    void ResetActionState();
    void UpdateActionState();
    void EnterActionState(ActionState state);
    int64_t GetActionStateTime() const;
    ActionState CheckActionStateTransition();

    void UpdateDispersion();

private:
    HumanCharacterTuning human_tuning_;

    Rideable* rideable_ = nullptr;
    DrivableVehicle* vehicle_ = nullptr;
    size_t seat_idx_ = 0;
    bool is_driver_ = false;

    HumanCharacterState state_ = HS_INIT;
    HumanCharacterStateSignals signals_ = 0;

    glm::vec3 rideable_exit_pos_ = glm::vec3(0.0f);

    bool aimheld_ = false;
    bool fireheld_ = false;
    bool reloadheld_ = false;

    ActionState actionstate_ = ACTION_IDLE;
    int64_t actionstate_start_ = 0;

    std::shared_ptr<ItemInstance> item_;
    std::shared_ptr<ItemInstance> pending_item_;
    
    int64_t last_fire_time_ = 0;
    float dispersion_ = 0.0f;
};
}

