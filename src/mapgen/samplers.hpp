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

class TerrainColorSampler
{
public:
    TerrainColorSampler(uint32_t seed);

    glm::vec3 Get(const glm::vec2& pos) const;

private:
    FastNoiseLite main_noise_;
    FastNoiseLite detail_noise_;
};

} // namespace mg
