#pragma once

#include "character.hpp"

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
    ACTION_AIM,
    ACTION_AIMING,
    ACTION_FIRE,
    ACTION_UNAIM,
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
    
    void SetAiming(bool aiming) { aiming_ = aiming; }

    virtual ~HumanCharacter() override;

protected:
    virtual void OnRideableChanged() {}

private:
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


    void UpdateActionState();

    void EnterActionState();
    ActionState CheckActionStateTransition();
    void ExitActionState();

private:
    HumanCharacterTuning human_tuning_;

    Rideable* rideable_ = nullptr;
    DrivableVehicle* vehicle_ = nullptr;
    size_t seat_idx_ = 0;
    bool is_driver_ = false;

    HumanCharacterState state_ = HS_INIT;
    HumanCharacterStateSignals signals_ = 0;

    glm::vec3 rideable_exit_pos_ = glm::vec3(0.0f);

    bool aiming_ = false;

    ActionState actionstate_ = ACTION_IDLE;

};
}

