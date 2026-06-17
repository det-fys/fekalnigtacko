#pragma once

#include "item_instance.hpp"

namespace game
{

struct Inventory
{
    std::shared_ptr<ItemInstance> slots[10];
    size_t active_slot = 0;

    std::map<std::string, size_t> ammo;
};

}
