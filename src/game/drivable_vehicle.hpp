#pragma once

#include "vehicle.hpp"
#include "usable.hpp"
#include "controllable_character.hpp"

namespace game
{

struct VehicleSeat
{
    glm::vec3 position;
    ControllableCharacter* occupant;
};

class DrivableVehicle : public Vehicle, public Usable
{
public:
    using Super = Vehicle;

    DrivableVehicle(World& world, const VehicleTuning& tuning);

    virtual void OnPhysicsChanged() override;
    
    virtual bool QueryUseTarget(PlayerCharacter& character, uint32_t target_id, UseTargetQueryResult& res) override;
    virtual void Use(PlayerCharacter& character, uint32_t target_id) override;

    bool SetPassenger(uint32_t seat_idx, ControllableCharacter* character);

    size_t GetNumSeats() const { return seats_.size(); }
    ControllableCharacter* GetPassenger(size_t idx) const { return seats_[idx].occupant; }

    ~DrivableVehicle() override;

private:
    void InitSeats();

private:
    std::vector<VehicleSeat> seats_;
};

}