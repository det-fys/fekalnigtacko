#pragma once

#include <glm/glm.hpp>

namespace gfx
{

struct Environment
{
    glm::vec3 clear_color{0.0f};
    glm::vec3 ambient_light{0.0f};
    glm::vec3 sun_color{0.0f};
    glm::vec3 sun_direction{0.0f, 0.0f, 1.0f};
    glm::vec4 fog{0.0f}; // alpha = distance
};

} // namespace gfx
