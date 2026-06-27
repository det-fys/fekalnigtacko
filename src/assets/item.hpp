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
    AIMTYPE_CROSSHAIR,
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
    std::string displayname;

    size_t slot = 0;

    std::string model_name;
    std::shared_ptr<const assets::Model> model;
    
    std::string bone;
    Transform bone_offset;

    bool twohanded = false;

    float walk_speed_mult = 1.0f;

    std::string legs_anim;
    std::string raise_anim;
    std::string idle_anim;
    std::string use_anim; // use or fire
    
    ItemAimType aim_type = AIMTYPE_NONE;

    // consumable
    std::string action;

    // weapon
    WeaponType weapon_type = WEAPON_MANUAL;
    WeaponFireType fire_type = FIRETYPE_MELEE;
    std::string ammo_type;
    size_t clip_size = 0;
    size_t fire_delay = 0;
    float dispersion_min = 0.0f;
    float dispersion_max = 0.0f;
    float dispersion_shot = 0.0f;
    float dispersion_decay = 1.0f;
    float damage = 1.0f;

    std::string projectile_model_name;
    float projectile_speed = 100.0f;
    float projectile_gravity = 0.0f;
    float projectile_lifetime = 5.0f;
    std::string projectile_fx;
    std::string projectile_sound;
    float projectile_damage = 1.0f;
    float projectile_radius = 1.0f;
    float projectile_impulse = 1.0f;

    std::string aim_anim;
    std::string aiming_anim;
    std::string reload_anim;

    std::string fire_snd;
    std::string fire_fx;
    std::string fire_fx_loc;

    static std::shared_ptr<Item> LoadFromFile(const std::string& path);
};



}