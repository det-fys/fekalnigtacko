#pragma once

#include <glm/glm.hpp>

inline void MoveToward(float& val, float target, float max_delta)
{
    if (val == target)
        return;

    if (val < target)
    {
        val += max_delta;
        if (val > target)
            val = target;
    }
    else
    {
        val -= max_delta;
        if (val < target)
            val = target;
    }
}

inline float UnMix(float a, float b, float x)
{
    return (x - a) / (b - a);
}

inline glm::mat3 BasisFromDir(const glm::vec3& dir)
{
    glm::vec3 world_up(0.0f, 0.0f, 1.0f);

    auto forward = glm::normalize(dir);

    // if forward is too close to world_up (parallel), choose a different up to avoid degenerate cross
    const float parallel_threshold = 0.999f;
    if (glm::abs(glm::dot(forward, world_up)) > parallel_threshold)
    {
        // pick an arbitrary orthogonal world_up candidate
        world_up = glm::vec3(1.0f, 0.0f, 0.0f);

    }

    auto right = glm::normalize(glm::cross(forward, world_up));
    auto up = glm::normalize(glm::cross(right, forward));
    return glm::mat3(right, forward, up);
}

inline glm::quat RotationTowards(const glm::vec3& dir)
{
    return glm::quat(BasisFromDir(dir));
}

