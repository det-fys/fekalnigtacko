#pragma once

#include "assets/item.hpp"

namespace game
{

struct ItemInstance
{
    std::shared_ptr<const assets::Item> def;
    size_t ammo = 0;
    
    ItemInstance(std::shared_ptr<const assets::Item> def);
    ItemInstance(const std::string& name);
};

}