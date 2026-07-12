#pragma once

#include <cstdint>
#include <cstddef>
#include <glm/glm.hpp>

namespace gfx
{

struct LightData
{
    glm::vec3 position;
    float radius;
    glm::vec3 color;
    float cos_inner;
    glm::vec3 dir;
    float cos_outer;
};

template <size_t Size>
struct LightArray
{
    size_t num_lights = 0;
    LightData lights[Size];
};

}