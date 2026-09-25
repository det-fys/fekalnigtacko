#include "samplers.hpp"

mg::TerrainHeightSampler::TerrainHeightSampler(uint32_t heightmap_seed)
{
    noise_.SetSeed(heightmap_seed);
    noise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise_.SetFrequency(0.00001f);
    noise_.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise_.SetFractalOctaves(5);

    noise2_.SetSeed(heightmap_seed ^ 0xAAAAAAAA);
    noise2_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise2_.SetFrequency(0.001f);
    noise2_.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise2_.SetFractalLacunarity(2.0f);
    noise2_.SetFractalOctaves(5);
}

float mg::TerrainHeightSampler::Get(const glm::vec2& pos) const
{
    float height = (noise_.GetNoise(pos.x, pos.y) * 0.5f + 0.5f) * 1500.0f;
    //height *= glm::mix(0.1f, 1.0f, GetHeightFalloff(pos.x, pos.y, ctx.map_cfg->chunks * ctx.map_cfg->chunk_size_m));
    height -= 500.0f;
    height += (noise2_.GetNoise(pos.x, pos.y) * 0.5f + 0.5f) * 25.0f; // add finer detail
    height -= 250.0f;
    return height;
}

mg::TerrainColorSampler::TerrainColorSampler(uint32_t seed)
{
    noise_.SetSeed(seed);
    noise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise_.SetFrequency(0.004f);
    noise_.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise_.SetFractalLacunarity(4.0f);
    noise_.SetFractalOctaves(5);
}

glm::vec3 mg::TerrainColorSampler::Get(const glm::vec2& pos) const
{
    constexpr glm::vec3 col0 = glm::vec3(0.4f, 0.6f, 0.3f);
    constexpr glm::vec3 col1 = glm::vec3(0.8f, 0.8f, 0.4f);

    auto t = noise_.GetNoise(pos.x, pos.y) * 0.5f + 0.5f;
    return glm::mix(col0, col1, t) * 0.8f;
}
