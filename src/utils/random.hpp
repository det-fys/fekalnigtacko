#pragma once

#include <cstdlib>

inline float RandomFloat(float min, float max)
{
    return min + (max - min) * static_cast<float>(rand() % 100) * 0.01f;
}
