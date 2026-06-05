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

class HumanCharacter : public Character
{
public:
    using Super = Character;
    
    HumanCharacter(World& world, const HumanCharacterTuning& tuning);
    
    const HumanCharacterTuning& GetHumanTuning() const { return human_tuning_; }

    void SetRideable(Rideable* rideable, size_t seat_idx); // called by Rideable!!
    void Ride(Rideable* rideable, size_t seat_idx);
    
    Rideable* GetRideable() const { return rideable_; }
    DrivableVehicle* GetVehicle() const { return vehicle_; }

    size_t GeatSeatIdx() const { return seat_idx_; }
    bool IsDriver() const { return is_driver_; }
    
    virtual ~HumanCharacter() override;

protected:
    virtual void OnRideableChanged() {}

private:
    HumanCharacterTuning human_tuning_;

    Rideable* rideable_ = nullptr;
    DrivableVehicle* vehicle_ = nullptr;
    size_t seat_idx_ = 0;
    bool is_driver_ = false;
};
}

