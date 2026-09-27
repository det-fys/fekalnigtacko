#include "samplers.hpp"

#include "utils/math.hpp"

namespace
{

float SampleNoiseNormalized(const FastNoiseLite& noise, const glm::vec2& pos)
{
    return noise.GetNoise(pos.x, pos.y) * 0.5f + 0.5f;
}

} // namespace

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
    float height = SampleNoiseNormalized(noise_, pos) * 1500.0f;
    // height *= glm::mix(0.1f, 1.0f, GetHeightFalloff(pos.x, pos.y, ctx.map_cfg->chunks * ctx.map_cfg->chunk_size_m));
    height -= 500.0f;
    height += SampleNoiseNormalized(noise2_, pos) * 25.0f; // add finer detail
    height -= 250.0f;
    return height;
}

mg::ForestSampler::ForestSampler(uint32_t seed)
{
    intensity_noise_.SetSeed(seed);
    intensity_noise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    intensity_noise_.SetFrequency(0.001f);
    intensity_noise_.SetFractalType(FastNoiseLite::FractalType_FBm);
    intensity_noise_.SetFractalOctaves(5);
    intensity_noise_.SetFractalLacunarity(2.0f);

    density_noise_.SetSeed(seed ^ 0xAAAAAAAA);
    density_noise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    density_noise_.SetFrequency(0.005f);
    density_noise_.SetFractalType(FastNoiseLite::FractalType_FBm);
    density_noise_.SetFractalOctaves(5);
    density_noise_.SetFractalLacunarity(2.0f);

    type_noise_.SetSeed(seed ^ 0x55555555);
    type_noise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    type_noise_.SetFrequency(0.02f);
    type_noise_.SetFractalType(FastNoiseLite::FractalType_FBm);
    type_noise_.SetFractalOctaves(5);
    type_noise_.SetFractalLacunarity(2.0f);
}

mg::ForestSample mg::ForestSampler::Get(const glm::vec2& pos) const
{
    ForestSample forest{};
    forest.intensity = GetIntensity(pos);
    forest.density = GetDensity(pos);
    forest.type = GetType(pos);
    return forest;
}

float mg::ForestSampler::GetIntensity(const glm::vec2& pos) const
{
    auto intensity = SampleNoiseNormalized(intensity_noise_, pos);
    //intensity = glm::pow(intensity, 1.5f); // make low values lower

    return glm::clamp(UnMix(0.4f, 0.55f, intensity), 0.0f, 1.0f); 
}

float mg::ForestSampler::GetDensity(const glm::vec2& pos) const
{
    return SampleNoiseNormalized(density_noise_, pos);
}

float mg::ForestSampler::GetType(const glm::vec2& pos) const
{
    return SampleNoiseNormalized(type_noise_, pos);
}

mg::TerrainColorSampler::TerrainColorSampler(uint32_t seed)
{
    main_noise_.SetSeed(seed);
    main_noise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    main_noise_.SetFrequency(0.004f);
    main_noise_.SetFractalType(FastNoiseLite::FractalType_FBm);
    main_noise_.SetFractalLacunarity(4.0f);
    main_noise_.SetFractalOctaves(5);

    detail_noise_.SetSeed(seed ^ 0x55555555);
    detail_noise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    detail_noise_.SetFrequency(0.02f);
    detail_noise_.SetFractalType(FastNoiseLite::FractalType_FBm);
    detail_noise_.SetFractalLacunarity(4.0f);
    detail_noise_.SetFractalOctaves(5);
}

glm::vec3 mg::TerrainColorSampler::Get(const glm::vec2& pos, const ForestSample& forest) const
{
    if (forest.intensity > 0.3f)
    {
        return GetForestGroundColor(pos, forest);
    }

    return GetGrassColor(pos);
}

glm::vec3 mg::TerrainColorSampler::GetGrassColor(const glm::vec2& pos) const
{
    constexpr glm::vec3 col0 = glm::vec3(0.4f, 0.6f, 0.3f);
    constexpr glm::vec3 col1 = glm::vec3(0.8f, 0.8f, 0.4f);

    auto t = SampleNoiseNormalized(main_noise_, pos);
    auto tint = glm::mix(col0, col1, t);

    auto t_detail = SampleNoiseNormalized(detail_noise_, pos);
    auto mult = glm::mix(0.9f, 1.1f, t_detail);

    return tint * mult;
}

glm::vec3 mg::TerrainColorSampler::GetForestGroundColor(const glm::vec2& pos, const ForestSample& forest) const
{
    constexpr glm::vec3 veg_col = glm::vec3(71, 145, 73) / 255.0f;
    constexpr glm::vec3 needle_col = glm::vec3(181, 119, 72) / 255.0f;

    auto col = forest.intensity > 0.7f ? needle_col : veg_col;

    auto t_detail = SampleNoiseNormalized(detail_noise_, pos + 1000.0f);
    auto mult = glm::mix(0.9f, 1.1f, t_detail);

    return col * mult;

    //auto t = (forest.intensity - 0.3f) / 0.7f;
    //return glm::mix(veg_col, needle_col, t);
}
