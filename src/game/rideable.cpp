#include "rideable.hpp"

#include <stdexcept>

game::Rideable::Rideable(Entity& entity, RideableType type) : entity_(entity), type_(type) {}

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
    }

    seat.passenger = passenger;

    if (passenger)
    {
        passenger->SetRideable(this, seat_idx);
    }

    OnPassengerChanged(seat_idx, passenger);
}

game::HumanCharacter* game::Rideable::GetPassenger(size_t seat_idx)
{
    if (seat_idx >= seats_.size())
        return nullptr;

    return seats_[seat_idx].passenger;
}

const glm::vec3& game::Rideable::GetSeatOffset(size_t seat_idx)
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
