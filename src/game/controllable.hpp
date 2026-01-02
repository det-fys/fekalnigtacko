#pragma once

#include "utils/defs.hpp"

namespace game
{

class Entity;
class Player;

class Controllable
{
public:
    Controllable() = default;
    DELETE_COPY_MOVE(Controllable)

    virtual Entity& GetEntity() = 0;

    Player* GetController() { return controller_; }
    const Player* GetController() const { return controller_; }

    ~Controllable();

private:
    Player* controller_ = nullptr;
    friend class Player;

};

}