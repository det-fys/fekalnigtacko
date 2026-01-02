#include "entity.hpp"

#include "world.hpp"

game::Entity::Entity(World& world, net::EntType viewtype) : world_(world), entnum_(world.GetNewEntnum()), viewtype_(viewtype) {}
