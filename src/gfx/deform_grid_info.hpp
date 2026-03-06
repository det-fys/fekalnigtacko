#pragma once

#include <glm/glm.hpp>

namespace gfx
{

struct DeformGridInfo
{
    glm::vec3 min;
    glm::vec3 max;
    glm::ivec3 res;
    float max_offset;
};


}