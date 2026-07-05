#pragma once

#include "net/defs.hpp"

namespace game
{

using PlayerHudFields = uint8_t;
enum PlayerHudField : PlayerHudFields
{
    PHUD_BALANCE = 1,

    PHUD_HEALTH = 2,
    PHUD_WEAPON_SLOTS = 4,
    PHUD_ITEM = 8,
    PHUD_AMMO_LOADED = 16,
    PHUD_AMMO_TOTAL = 32,
    PHUD_DEATH = 64,
};

using PlayerBalance = int64_t;

struct PlayerCharacterHudData
{
    // general
    uint8_t health = 0;
    // TODO: stamina ?
    uint8_t weapon_slots = 0;
    
    // held item
    std::string held_item;
    uint8_t ammo_loaded = 0;
    uint32_t ammo_total = 0;
    
    // death
    uint8_t dead = 0;
    
    // TODO: use target
};

struct PlayerHudData
{
    PlayerBalance balance = 0;
    PlayerCharacterHudData character;
};

enum HudEventType
{
    HUD_EVENT_DAMAGE, // DAMAGE <DamageEventType>
    HUD_EVENT_MONEY, // MONEY <PlayerBalance delta>
};

enum DamageEventType : uint8_t
{
    DAMAGE_EVENT_RECEIVED,
    DAMAGE_EVENT_DEALT,
    DAMAGE_EVENT_DEALT_KILL,
};



}