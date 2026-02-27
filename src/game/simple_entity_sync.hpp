#pragma once

#include "net/defs.hpp"

namespace game
{

struct SimpleEntitySyncState
{
    net::PositionQ pos;
    net::QuatQ rot;
};

using SimpleEntitySyncFieldFlags = uint8_t;

enum SimpleEntitySyncFieldFlag
{
    SESF_POSITION = 0x01,
    SESF_ROTATION = 0x02,
};


}