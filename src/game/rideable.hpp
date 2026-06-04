#pragma once

#include "human_character.hpp"
#include "player_input.hpp"

namespace game
{

enum RideableType
{
    RIDEABLE_NONE,
    RIDEABLE_VEHICLE,
    RIDEABLE_ANIMAL,
};

struct RideableSeat
{
    glm::vec3 offset;
    HumanCharacter* passenger;
};

class Rideable
{
public:
    Rideable(Entity& entity, RideableType type);

    void SetPassenger(size_t seat_idx, HumanCharacter* passenger);
    HumanCharacter* GetPassenger(size_t seat_idx);
    const glm::vec3& GetSeatOffset(size_t seat_idx);
    size_t GetNumSeats() const { return seats_.size(); }
    void KickAll();

    virtual void SetRideableInput(PlayerInputFlags in) {}
    virtual void SetRideableYaw(float yaw) {}
    
    RideableType GetRideableType() const { return type_; }
    
    Entity& GetEntity() { return entity_; }
    const Entity& GetEntity() const { return entity_; }

    virtual ~Rideable();

protected:
    size_t AddSeat(const glm::vec3& offset);
    virtual void OnPassengerChanged(size_t seat_idx, HumanCharacter* passenger) {}

private:
    Entity& entity_;
    RideableType type_;
    std::vector<RideableSeat> seats_;
};


}