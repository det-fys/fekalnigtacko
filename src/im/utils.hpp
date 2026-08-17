#pragma once

#include <glm/glm.hpp>
#include <imgui.h>

namespace im
{

inline ImVec2 VecToImGui(const glm::vec2& vec)
{
    return ImVec2(vec.x, vec.y);
}

inline glm::vec2 VecFromImGui(const ImVec2& vec)
{
    return glm::vec2(vec.x, vec.y);
}

}
