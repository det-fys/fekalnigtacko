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
    HumanCharacter* GetPassenger(size_t seat_idx) const;
    const glm::vec3& GetSeatOffset(size_t seat_idx) const;
    size_t GetNumSeats() const { return seats_.size(); }
    void KickAll();

    virtual void SetRideableInput(PlayerInputFlags in) {}
    virtual void SetRideableViewAngles(float yaw, float pitch) {}
    
    void OnRideableDamaged(const DamageInfo& damage) const;
    
    RideableType GetRideableType() const { return type_; }
    
    bool IsAbandoned(int64_t time) const;

    Entity& GetEntity() { return entity_; }
    const Entity& GetEntity() const { return entity_; }

    virtual ~Rideable();

protected:
    size_t AddSeat(const glm::vec3& offset);
    virtual void OnPassengerChanged(size_t seat_idx, HumanCharacter* passenger) {}

private:
    void UpdateLeaveTime();

private:
    Entity& entity_;
    RideableType type_;
    std::vector<RideableSeat> seats_;

    int64_t last_passenger_leave_time_ = -1;
};


}