#pragma once

#include <map>

#include "world.hpp"

namespace game
{

class Game
{
public:
    Game();

private:
    std::shared_ptr<World> default_world_;

};

}