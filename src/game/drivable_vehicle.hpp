#pragma once

#include "vehicle.hpp"
#include "usable.hpp"
#include "rideable.hpp"
#include "human_character.hpp"

namespace game
{

class DrivableVehicle : public Vehicle, public Usable, public Rideable
{
public:
    using Super = Vehicle;

    DrivableVehicle(World& world, const VehicleSpawnInfo& info);

    virtual void Update() override;

    virtual HumanCharacter* GetResponsibleCharacter() override;

    virtual void SetTuning(const VehicleTuning& tuning) override;

    virtual void OnPhysicsChanged() override;
    
    virtual bool QueryUseTarget(PlayerCharacter& character, uint32_t target_id, UseTargetQueryResult& res) override;
    virtual void Use(PlayerCharacter& character, uint32_t target_id) override;
    
    virtual void OnContact(const collision::ContactInfo& info) override;
    virtual void ReceiveDamage(const DamageInfo& damage) override;

    virtual void SetRideableInput(PlayerInputFlags in) override;

protected:
    virtual void OnPassengerChanged(size_t seat_idx, HumanCharacter* passenger) override;

private:
    void InitSeats();
    void UpdateUseTargetNames();

};

}