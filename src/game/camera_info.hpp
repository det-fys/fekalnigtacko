#pragma once

#include "net/defs.hpp"

namespace game
{

using CameraFlags = uint8_t;
enum CameraFlag : CameraFlags
{
    CAM_AIMING = 1,
};

struct CameraInfo
{
    net::EntNum character_entnum = 0;
    net::EntNum rideable_entnum = 0;
    CameraFlags flags = 0;
};

}