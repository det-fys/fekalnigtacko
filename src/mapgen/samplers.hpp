#pragma once

#include <cstdint>
#include <array>

#include "FastNoiseLite.h"
#include <glm/glm.hpp>

#include "map_config.hpp"

namespace mg
{

class TerrainHeightSampler
{
public:
    TerrainHeightSampler(const MapConfig& cfg);

    float Get(const glm::vec2& pos) const;

private:
    const float world_size_m_;

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

    glm::vec3 Get(const glm::vec2& pos, const ForestSample& forest, float sea_level) const;
    glm::vec3 GetGrassColor(const glm::vec2& pos) const;
    glm::vec3 GetForestGroundColor(const glm::vec2& pos, const ForestSample& forest) const;
    glm::vec3 GetSandColor(const glm::vec2& pos, float sea_level) const;

private:
    FastNoiseLite main_noise_;
    FastNoiseLite detail_noise_;
};

class GrassSampler
{
public:
    GrassSampler(uint32_t seed);
    
    float GetDensity(const glm::vec2& pos) const;
    glm::vec2 GetOffset(const glm::vec2& pos) const;
    float GetBrightness(const glm::vec2& pos) const;

private:
    FastNoiseLite density_noise_;

    std::array<FastNoiseLite, 2> offset_noises_;

};

} // namespace mg
