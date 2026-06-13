#pragma once

#include <map>

#include "model.hpp"

namespace assets
{

enum ItemType
{
    ITEM_NONE,
    ITEM_WEAPON,
    ITEM_CONSUMABLE,
};

enum ItemAimType
{
    AIMTYPE_NONE,
    AIMTYPE_AIM,
    AIMTYPE_SCOPE,

};

enum WeaponType
{
    WEAPON_MANUAL,
    WEAPON_SEMIAUTO,
    WEAPON_AUTO,
};

enum WeaponFireType
{
    FIRETYPE_MELEE,
    FIRETYPE_BULLET,
    FIRETYPE_PROJECTILE,
};

struct Item
{
    ItemType type = ITEM_NONE;
    std::string name;

    std::string idle_anim;
    std::string use_anim; // use or fire
    
    std::shared_ptr<const assets::Model> model;
    
    std::string bone;
    Transform bone_offset;

    // consumable
    std::string action;

    // weapon
    WeaponType weapon_type = WEAPON_MANUAL;
    WeaponFireType fire_type = FIRETYPE_MELEE;
    std::string ammo_type;
    size_t clip_size = 0;
    size_t fire_delay = 0;

    std::string aim_anim;
    std::string aiming_anim;

    static std::shared_ptr<Item> LoadFromFile(const std::string& path);
};



}