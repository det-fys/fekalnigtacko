#include "controllable.hpp"

#include "player.hpp"

game::Controllable::~Controllable()
{
    if (controller_)
        controller_->Control(nullptr);
}
