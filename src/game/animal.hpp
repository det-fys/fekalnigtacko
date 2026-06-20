#pragma once

#include "character.hpp"
#include "usable.hpp"
#include "rideable.hpp"

namespace game
{

enum AnimalThinkState
{
    ANIMAL_THINKSTATE_IDLE,
    ANIMAL_THINKSTATE_ROAM,
    ANIMAL_THINKSTATE_MOUNTED,
    ANIMAL_THINKSTATE_HURT,
    ANIMAL_THINKSTATE_RUN_AWAY,
};

class Animal : public Character, public Usable, public Rideable
{
public:
    using Super = Character;

    Animal(World& world, const CharacterTuning& tuning, const glm::vec3& position, float yaw);

    virtual void Update() override;

    virtual void ReceiveDamage(const DamageInfo& damage) override;

    virtual bool QueryUseTarget(PlayerCharacter& character, uint32_t target_id, UseTargetQueryResult& res) override;
    virtual void Use(PlayerCharacter& character, uint32_t target_id) override;

    virtual void SetRideableInput(PlayerInputFlags in) override;
    virtual void SetRideableViewAngles(float yaw, float pitch) override;

protected:
    virtual void OnPassengerChanged(size_t seat_idx, HumanCharacter* passenger) override;
    void SetUseMessage(const std::string& message);
    void AddAnimalSeat(const glm::vec3& offset);

    virtual void MakeSound() {}
    virtual void MakeHurtSound() {}

private:
    bool IsMounted() const;
    void ChangeDirection();
    void TryMakeSound();

    void Think();
    void EnterThinkState(AnimalThinkState state);
    AnimalThinkState CheckThinkStateTransition();
    int64_t GetCurrentThinkStateDuration() const;

private:
    std::string use_message_;

    AnimalThinkState think_state_ = ANIMAL_THINKSTATE_IDLE;
    int64_t think_state_start_ = 0;
    int64_t last_sound_time_ = 0;

    bool just_hit_ = false;
    // net::EntNum attacker_ = 0;
    glm::vec3 hit_from_{};

};

}