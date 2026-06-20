#include "rideable.hpp"

#include <stdexcept>

#include "world.hpp"

game::Rideable::Rideable(Entity& entity, RideableType type) : entity_(entity), type_(type)
{
    last_passenger_leave_time_ = entity_.GetWorld().GetTime();
}

void game::Rideable::SetPassenger(size_t seat_idx, HumanCharacter* passenger) 
{
    if (seat_idx >= seats_.size())
        throw std::runtime_error("Invalid seat index");

    auto& seat = seats_[seat_idx];

    if (seat.passenger == passenger)
        return; // already sitting here

    if (seat.passenger)
    {
        seat.passenger->SetRideable(nullptr, 0); // remove current passenger
        seat.passenger = nullptr;
        OnPassengerChanged(seat_idx, nullptr);
    }

    seat.passenger = passenger;

    if (passenger)
    {
        passenger->SetRideable(this, seat_idx);
    }

    OnPassengerChanged(seat_idx, passenger);
    UpdateLeaveTime();
}

game::HumanCharacter* game::Rideable::GetPassenger(size_t seat_idx) const
{
    if (seat_idx >= seats_.size())
        return nullptr;

    return seats_[seat_idx].passenger;
}

const glm::vec3& game::Rideable::GetSeatOffset(size_t seat_idx) const
{
    if (seat_idx >= seats_.size())
        throw std::runtime_error("Invalid seat index");

    return seats_[seat_idx].offset;
}

void game::Rideable::KickAll()
{
    for (size_t i = 0; i < seats_.size(); ++i)
    {
        if (seats_[i].passenger)
        SetPassenger(i, nullptr);
    }
}

void game::Rideable::OnRideableDamaged(const DamageInfo& damage) const
{
    for (const auto& seat : seats_)
    {
        if (seat.passenger)
        {
            seat.passenger->OnRideableDamaged(damage);
        }
    }
}

bool game::Rideable::IsAbandoned(int64_t time) const
{
    // still someone in
    if (last_passenger_leave_time_ < 0)
    {
        return false;
    }

    return entity_.GetWorld().GetTime() - last_passenger_leave_time_ >= time;
}

game::Rideable::~Rideable()
{
    // kick passengers
    KickAll();
}

size_t game::Rideable::AddSeat(const glm::vec3& offset) 
{
    RideableSeat seat{};
    seat.offset = offset;
    seats_.emplace_back(seat);
    return seats_.size() - 1;
}

void game::Rideable::UpdateLeaveTime()
{
    size_t num_passengers = 0;
    for (const auto& seat : seats_)
    {
        if (seat.passenger)
        {
            ++num_passengers;
        }
    }

    if (num_passengers > 0)
    {
        last_passenger_leave_time_ = -1;
    }
    else
    {
        last_passenger_leave_time_ = entity_.GetWorld().GetTime();
    }

}
