#pragma once

#include <string>

namespace game
{

struct VehicleTuning
{
    std::string model;
    
    uint32_t primary_color = 0xFFFFFFFF;
    
    size_t wheels_idx = 0;
    uint32_t wheel_color = 0xFFFFFFFF;
};


}