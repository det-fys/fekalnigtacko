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

struct ForestSample
{
    float intensity = 0.0f;
    float density = 0.0f;
    float type = 0.0f;
};

class ForestSampler
{
public:
    ForestSampler(uint32_t seed);

    ForestSample Get(const glm::vec2& pos) const;
    float GetIntensity(const glm::vec2& pos) const;
    float GetDensity(const glm::vec2& pos) const;
    float GetType(const glm::vec2& pos) const;

private:
    FastNoiseLite intensity_noise_;
    FastNoiseLite density_noise_;
    FastNoiseLite type_noise_;
};

class TerrainColorSampler
{
public:
    TerrainColorSampler(uint32_t seed);

    glm::vec3 Get(const glm::vec2& pos, const ForestSample& forest) const;

private:
    glm::vec3 GetGrassColor(const glm::vec2& pos) const;
    glm::vec3 GetForestGroundColor(const glm::vec2& pos, const ForestSample& forest) const;

private:
    FastNoiseLite main_noise_;
    FastNoiseLite detail_noise_;
};

} // namespace mg
