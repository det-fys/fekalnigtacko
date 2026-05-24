#pragma once

#include <cstdint>
#include <cstddef>
#include <glm/glm.hpp>
#include "shader_defs.hpp"

namespace gfx
{

template <size_t Size>
struct LightArray
{
    size_t num_lights = 0;
    glm::vec3 positions[Size];
    glm::vec4 colors_rs[Size];
};

struct LightCache : public LightArray<SD_MAX_LIGHTS>
{
    glm::vec3 center = glm::vec3(0.0f);
    size_t frame = 0;
};


}