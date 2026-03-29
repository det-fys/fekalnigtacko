#pragma once

#include <glm/glm.hpp>

inline glm::u8vec3 ColorU32ToU8Vec3(uint32_t color)
{
    return glm::u8vec3(
        color & 0xFF,
        (color >> 8) & 0xFF,
        (color >> 16) & 0xFF
    );
}

inline uint32_t ColorU8Vec3ToU32(const glm::u8vec3& color)
{
    return 0xFF000000 | (color.b << 16) | (color.g << 8) | color.r;
}