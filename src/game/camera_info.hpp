#pragma once

#include "net/defs.hpp"

namespace game
{

using CameraFlags = uint8_t;
enum CameraFlag : CameraFlags
{
    CAM_AIMING = 1,
    CAM_AIM_CROSSHAIR = 2,
    CAM_AIM_SCOPE = 4,
    CAM_FIRST_PERSON = 8,
};

struct CameraInfo
{
    net::EntNum character_entnum = 0;
    net::EntNum rideable_entnum = 0;
    CameraFlags flags = 0;
};

}