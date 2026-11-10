#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstddef>
#include <type_traits>

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

template <typename T>
inline constexpr T AlignUp(T value, T alignment)
{
    static_assert(std::is_integral_v<T>, "T must be an integer type");

    return (value + alignment - 1) / alignment * alignment;
}

inline glm::vec3 Slerp(const glm::vec3& a, const glm::vec3& b, float t)
{
    float dot = glm::clamp(glm::dot(a, b), -1.0f, 1.0f);
    float theta = glm::acos(dot);

    if (theta < 0.001f)
        return glm::normalize(glm::mix(a, b, t));

    float sinTheta = glm::sin(theta);

    return (glm::sin((1.0f - t) * theta) * a + glm::sin(t * theta) * b) / sinTheta;
}

inline float SignedAngle(float a, float b)
{
    float diff = glm::mod(b - a + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>();
    return diff;
}

inline bool Turn(float& angle, float target, float step)
{
    angle = glm::mod(angle, glm::two_pi<float>());
    target = glm::mod(target, glm::two_pi<float>());

    auto diff = SignedAngle(angle, target);

    if (glm::abs(diff) <= step)
    {
        angle = target;
        return true;
    }

    angle = glm::mod(angle + glm::sign(diff) * step, glm::two_pi<float>());
    return false;
}