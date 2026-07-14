#pragma once

#include <glm/glm.hpp>

namespace gfx
{

inline glm::mat3 GetHudMatrix(const glm::u32vec2 viewport_size)
{
    float w = static_cast<float>(viewport_size.x);
    float h = static_cast<float>(viewport_size.y);
    glm::vec2 screen_size_px(w, h);
    glm::vec2 ndc_scale(2.0f / screen_size_px.x, -2.0f / screen_size_px.y);
    constexpr glm::vec2 ndc_offset(-1.0f, 1.0f);

    glm::mat3 matrix(1.0f);
    matrix[0][0] = ndc_scale.x;
    matrix[1][1] = ndc_scale.y;
    matrix[2][0] = ndc_offset.x;
    matrix[2][1] = ndc_offset.y;

    return matrix;
}

}