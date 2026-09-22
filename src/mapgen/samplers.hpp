#pragma once

#include <cstdint>

#include "FastNoiseLite.h"
#include <glm/glm.hpp>

namespace mg
{

class TerrainHeightSampler
{
public:
    TerrainHeightSampler(uint32_t seed);

    float Get(const glm::vec2& pos) const;

private:
    FastNoiseLite noise_;
    FastNoiseLite noise2_;
};

} // namespace mg
