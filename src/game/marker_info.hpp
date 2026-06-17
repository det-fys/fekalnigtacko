#pragma once

#include <cstdint>
#include <string>
#include <glm/glm.hpp>

namespace game
{

enum MarkerType : uint8_t
{
    MARKER_FOOT,
    MARKER_VEHICLE,
    MARKER_PICKUP,
};

struct MarkerInfo
{
    glm::vec3 position;
    MarkerType type;
    uint32_t color;
    std::string model;
};

}