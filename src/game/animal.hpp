#pragma once

#include "character.hpp"
#include "usable.hpp"
#include "rideable.hpp"

namespace game
{

class Animal : public Character, public Usable, public Rideable
{
public:
    Animal(World& world, const CharacterTuning& tuning, const glm::vec3& position, float yaw);

    virtual bool QueryUseTarget(PlayerCharacter& character, uint32_t target_id, UseTargetQueryResult& res) override;
    virtual void Use(PlayerCharacter& character, uint32_t target_id) override;

    virtual void SetRideableInput(PlayerInputFlags in) override;
    virtual void SetRideableViewAngles(float yaw, float pitch) override;

protected:
    virtual void OnPassengerChanged(size_t seat_idx, HumanCharacter* passenger) override;
    void SetUseMessage(const std::string& message);
    void AddAnimalSeat(const glm::vec3& offset);

private:
    std::string use_message_;

};

}