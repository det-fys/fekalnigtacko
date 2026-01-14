#pragma once

#include <glm/glm.hpp>

namespace gfx
{

struct HudPosition
{
    glm::vec2 anchor = glm::vec2(0.0f);   // <0;1>
    glm::vec2 pos = glm::vec2(0.0f);      // px or px multiplies (scale != 1)
    glm::vec2 scale = glm::vec2(1.0f);
};

}