#pragma once

#include <glm/glm.hpp>

namespace gfx
{

struct CameraParams
{
    glm::vec3 eye{0.0f};
    glm::vec3 dir{0.0f, 1.0f, 0.0f};
    float fov{90.0f};
};

} // namespace gfx