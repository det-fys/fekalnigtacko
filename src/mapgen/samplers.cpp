#include "samplers.hpp"

#include "utils/math.hpp"

namespace
{

float SampleNoiseNormalized(const FastNoiseLite& noise, const glm::vec2& pos)
{
    return noise.GetNoise(pos.x, pos.y) * 0.5f + 0.5f;
}

float MySmoothstep(float x)
{
    const float x2 = x * x;
    return x2 / (x2 + (x - 1.0f) * (x - 1.0f));
}

float MySmoothstep(float edge0, float edge1, float x)
{
    if (x <= edge0)
        return 0.0f;
    if (x >= edge1)
        return 1.0f;
    return MySmoothstep((x - edge0) / (edge1 - edge0));
    // return (x - edge0) / (edge1 - edge0);
}

float GetHeightFalloff(float x, float y, float world_size)
{
    float half_world_size = world_size * 0.5f;
    float dist = glm::length(glm::vec2(x, y));

    const float shore_d = half_world_size * 0.5f;
    const float border_d = -half_world_size * 0.3f;
    return 1.0f - MySmoothstep(half_world_size - shore_d, half_world_size - border_d, dist);
}

} // namespace

mg::TerrainHeightSampler::TerrainHeightSampler(const MapConfig& cfg)
    : world_size_m_(cfg.chunks * cfg.chunk_size_m)
{
    auto seed = cfg.seed ^ 0x431647;

    noise_.SetSeed(seed);
    noise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise_.SetFrequency(0.00001f);
    noise_.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise_.SetFractalOctaves(5);

    noise2_.SetSeed(seed ^ 0xAAAAAAAA);
    noise2_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise2_.SetFrequency(0.001f);
    noise2_.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise2_.SetFractalLacunarity(2.0f);
    noise2_.SetFractalOctaves(5);
}

float mg::TerrainHeightSampler::Get(const glm::vec2& pos) const
{
    auto height = SampleNoiseNormalized(noise_, pos) * 500.0f;
    height *= glm::mix(0.1f, 1.0f, GetHeightFalloff(pos.x, pos.y, world_size_m_));
    height += SampleNoiseNormalized(noise2_, pos) * 35.0f; // add finer detail
    height -= 100.0f;
    //height -= 250.0f;
    return height;
}

mg::ForestSampler::ForestSampler(uint32_t seed)
{
    seed = seed ^ 0x402357;

    intensity_noise_.SetSeed(seed);
    intensity_noise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    intensity_noise_.SetFrequency(0.001f);
    intensity_noise_.SetFractalType(FastNoiseLite::FractalType_FBm);
    intensity_noise_.SetFractalOctaves(5);
    intensity_noise_.SetFractalLacunarity(2.0f);
    intensity_noise_.SetFractalGain(0.7f);
    intensity_noise_.SetFractalWeightedStrength(-0.2f);

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

    return glm::clamp(UnMix(0.45f, 0.6f, intensity), 0.0f, 1.0f); 
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
    seed = seed ^ 0x7322419;

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

glm::vec3 mg::TerrainColorSampler::Get(const glm::vec2& pos, const ForestSample& forest, float sea_level) const
{
    if (sea_level < 10.0f)
    {
        return GetSandColor(pos, sea_level);
    }

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
    constexpr glm::vec3 needle_col = glm::vec3(180, 119, 76) / 255.0f;
    constexpr glm::vec3 needle_col2 = glm::vec3(140, 129, 86) / 255.0f;

    auto col = forest.intensity > 0.7f ? needle_col : veg_col;
    col = glm::mix(col, needle_col2, SampleNoiseNormalized(main_noise_, pos * 2.0f + 1000.0f));

    auto t_detail = SampleNoiseNormalized(detail_noise_, pos * 10.0f);
    auto mult = glm::mix(0.6f, 1.2f, t_detail);

    return col * mult;

    //auto t = (forest.intensity - 0.3f) / 0.7f;
    //return glm::mix(veg_col, needle_col, t);
}

glm::vec3 mg::TerrainColorSampler::GetSandColor(const glm::vec2& pos, float sea_level) const
{
    constexpr glm::vec3 sand_col = glm::vec3(226, 195, 133) / 255.0f;
    // TODO: noise + deep darker
    return sand_col;
}

mg::GrassSampler::GrassSampler(uint32_t seed)
{
    seed = seed ^ 0x62455000;

    density_noise_.SetSeed(seed);
    density_noise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    density_noise_.SetFrequency(0.01f);
    density_noise_.SetFractalType(FastNoiseLite::FractalType_FBm);
    density_noise_.SetFractalLacunarity(2.0f);
    density_noise_.SetFractalOctaves(5);

    for (int i = 0; i < 2; ++i)
    {
        offset_noises_[i].SetSeed(seed ^ (0x55555555 * (i + 1)));
        offset_noises_[i].SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        offset_noises_[i].SetFrequency(1.0f);
        offset_noises_[i].SetFractalType(FastNoiseLite::FractalType_FBm);
        offset_noises_[i].SetFractalLacunarity(2.0f);
        offset_noises_[i].SetFractalGain(0.7f);
        offset_noises_[i].SetFractalOctaves(3);
    }
}

float mg::GrassSampler::GetDensity(const glm::vec2& pos) const
{
    return glm::clamp(UnMix(0.0f, 0.5f, SampleNoiseNormalized(density_noise_, pos)), 0.0f, 1.0f);
}

glm::vec2 mg::GrassSampler::GetOffset(const glm::vec2& pos) const
{
    auto offset =
        glm::vec2(SampleNoiseNormalized(offset_noises_[0], pos), SampleNoiseNormalized(offset_noises_[1], pos));

    return offset * 2.0f - 1.0f;
}

float mg::GrassSampler::GetBrightness(const glm::vec2& pos) const
{
    return glm::mix(0.7f, 1.4f, SampleNoiseNormalized(offset_noises_[0], pos * 1.5f));
}
