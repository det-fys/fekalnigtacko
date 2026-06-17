#pragma once

#include <cstdlib>

inline float RandomFloat(float min, float max)
{
    return min + (max - min) * static_cast<float>(rand() % 100) * 0.01f;
}

inline int RandomInt(int min, int max)
{
    return min + rand() % (max - min + 1);
}

inline glm::vec3 ApplyRandomDispersion(const glm::vec3& dir, float dispersion)
{
    float rand_rotation = RandomFloat(0.0f, glm::radians(360.0f));
    float rand_dispersion = RandomFloat(0.0f, 0.01f) * dispersion; // dispersion in m/100m

    auto right = glm::normalize(glm::cross(dir, glm::vec3(0.0f, 0.0f, 1.0f)));
    auto up = glm::normalize(glm::cross(right, dir));

    return dir + (right * glm::sin(rand_rotation) + up * glm::cos(rand_rotation)) * rand_dispersion;
}

inline bool Chance(float probability)
{
    return RandomFloat(0.0f, 1.0f) < probability;
}

inline bool ChanceAvgTime(float avg_time)
{
    return Chance((1.0f / 25.0f) / avg_time);
}