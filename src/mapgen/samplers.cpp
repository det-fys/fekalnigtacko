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
