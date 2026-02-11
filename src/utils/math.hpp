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
