#pragma once

#include "character.hpp"

namespace game
{

class DrivableVehicle;

class ControllableCharacter : public Character
{
public:
    using Super = Character;
    
    ControllableCharacter(World& world, const CharacterTuning& tuning);
    
    void SetVehicle(DrivableVehicle* vehicle, uint32_t seat);
    
    DrivableVehicle* GetVehicle() const { return vehicle_; }
    bool IsDriver() const { return is_driver_; }
    
    ~ControllableCharacter() override;

protected:
    virtual void VehicleChanged() = 0;

    size_t seat_idx_ = 0;
    bool is_driver_ = false;
    DrivableVehicle* vehicle_ = nullptr;
};
}

