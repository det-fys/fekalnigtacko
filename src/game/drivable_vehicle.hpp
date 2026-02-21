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
    DrivableVehicle(World& world, std::string model_name, const glm::vec3& color);

    virtual void Use(PlayerCharacter& character, uint32_t target_id) override;

    bool SetPassenger(uint32_t seat_idx, ControllableCharacter* character);

    ~DrivableVehicle() override;

private:
    void InitSeats();

private:
    std::vector<VehicleSeat> seats_;
};

}