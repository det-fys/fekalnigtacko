#include "samplers.hpp"

#include "utils/math.hpp"

namespace
{

float SampleNoiseNormalized(const FastNoiseLite& noise, const glm::dvec2& pos)
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

float GetHeightFalloff(const glm::dvec2& pos, double world_size)
{
    double half_world_size = world_size * 0.5;
    double dist = glm::length(glm::dvec2(pos.x, pos.y));

    const double shore_d = half_world_size * 0.5;
    const double border_d = -half_world_size * 0.3;
    return 1.0 - MySmoothstep(half_world_size - shore_d, half_world_size - border_d, dist);
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

namespace
{

// https://www.shadertoy.com/view/33cXW8

// Copyright 2025 Rune Skovbo Johansen
// Copyright 2023 Fewes
// Copyright 2020 Clay John

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
// and associated documentation files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or 
// substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

glm::vec2 hash(const glm::vec2& x)
{
    const glm::vec2 k = glm::vec2(0.3183099f, 0.3678794f);
    auto nx = x * k + glm::vec2(k.y, k.x);
    return -1.0f + 2.0f * glm::fract(16.0f * k * glm::fract(nx.x * nx.y * (nx.x + nx.y)));
}

glm::vec3 Gullies(const glm::vec2& p, const glm::vec2& slope)
{
    // Get a vector orthogonal to the slope, with a proportional magnitude. Front-load the
    // multiplication by 2 pi, since every usage of sideDir multiplied with that anyway.
    glm::vec2 sideDir = glm::vec2(-slope.y, slope.x) * 2.0f * glm::pi<float>();

    // Iterate over 4x4 cells, calculating a stripe pattern for each and blending between them.
    // pInt is the integer part of the current coordinate p, pFrac is the remainder.
    //
    // o   o   o   o
    //
    // o   o   o   o
    //       p
    // o   i   o   o
    //
    // o   o   o   o
    //
    // p: current coordinate    i: integer part of p    o: grid points for 4x4 cells
    //
    glm::vec2 pInt = glm::floor(p);
    glm::vec2 pFrac = glm::fract(p);
    glm::vec3 heightAndSlope = glm::vec3(0.0f);
    float weightSum = 0.0f;
    for (int i = -1; i <= 2; i++)
    {
        for (int j = -1; j <= 2; j++)
        {
            glm::vec2 gridOffset = glm::vec2(i, j);

            // Calculate a cell point by starting off with a point in an integer grid.
            glm::vec2 gridPoint = pInt + gridOffset;

            // Calculate a random offset for the cell point between -0.5 and 0.5 on each axis.
            glm::vec2 randomOffset = hash(gridPoint) * 0.5f;

            // The final cell point (we don't store it) is the gridPoint plus the randomOffset.
            // Calculate a vector representing the input point relative to this cell point:
            // p - (gridPoint + randomOffset)
            // = (pFrac + pInt) - ((pInt + gridOffset) + randomOffset)
            // = pFrac + pInt - pInt - gridOffset - randomOffset
            // = pFrac - gridOffset - randomOffset
            glm::vec2 vectorFromCellPoint = pFrac - gridOffset - randomOffset;

            // Bell-shaped weight function which is 1 at dist 0 and 0 at dist 1.5.
            // Due to the random offsets of up to 0.5, the closest a cell point not in the 4x4
            // grid can be to the current point is 1.5 units away.
            // The subtraction of 0.01111 is to make the function actually 0 at distance 1.5,
            // which avoids some (very subltle) grid line artefacts.
            float sqrDist = glm::dot(vectorFromCellPoint, vectorFromCellPoint);
            float weight = glm::max(0.0f, glm::exp(-sqrDist * 2.0f) - 0.01111f);

            // Keep track of total sum of weights.
            weightSum += weight;

            // The waveInput is a gradient which increases in value along sideDir (sideways
            // along the terrain slope). Its rate of change is the slope times 2 pi, due to the
            // 2 pi multiplier pre-applied to sideDir.
            float waveInput = dot(vectorFromCellPoint, sideDir);

            // Add a wave along sideDir. The longer sideDir is, the higher frequency the wave.
            // This means we get more high-frequency creases on steeper slopes.
            // Note that the fact we use cos() for the height (and not sin()) has a significant
            // effect on mountain peaks. Since the slope and hence the frequency is zero there,
            // cos() ensures this evaluates to the maximum value in the wave, meaning peaks
            // "get no erosion".
            heightAndSlope += glm::vec3(glm::cos(waveInput),           // Amplitude of the wave
                                         -glm::sin(waveInput) * sideDir // Analytical derivative of the wave
                                         // Correct derivatives would also involve applying the chain rule here,
                                         // but as it seems to look worse, it's disabled here:
                                         //+ cos(waveInput) * 4.0 * vectorFromCellPoint
                                         ) *
                              weight;
        }
    }
    return heightAndSlope / weightSum;
}

// Apply an erosion filter inspired by Fractal Brownian Motion type noise.
// Over several iterations, gullies are added as sine wave stripes that run parallel to the
// terrain slope. The gullies modify the slope, causing the next iteration of gullies to branch
// out at an angle to the previous ones.
glm::vec3 Erosion(const glm::vec2& p, glm::vec3& heightAndSlope, float scale, float strength, float slopePower,
                   float cellScale, int octaves, float gain, float lacunarity)
{
    glm::vec3 inputHeightAndSlope = heightAndSlope;
    float freq = 1.0f / (scale * cellScale);
    strength *= scale;
    for (int i = 0; i < octaves; i++)
    {
        // Adjust the slope given to the Gullies function, without influencing the final slope.
        // The calculation is equivalent to raising the length of the slope to slopePower.
        auto heightAndSlopeYZ = glm::vec2(heightAndSlope.y, heightAndSlope.z);
        float sqrLen = glm::dot(heightAndSlopeYZ, heightAndSlopeYZ);
        glm::vec2 inputSlope = heightAndSlopeYZ * glm::pow(sqrLen, 0.5f * (slopePower - 1.0f));

        // Calculate and add gullies to the height and slope.
        heightAndSlope += Gullies(p * freq, inputSlope * cellScale) * strength * glm::vec3(1, freq, freq);

        // Prepare the next octave.
        strength *= gain;
        freq *= lacunarity;
    }
    return heightAndSlope - inputHeightAndSlope;
}

// from https://www.shadertoy.com/view/XdXBRH
glm::vec3 noised(const glm::vec2& p)
{
    glm::vec2 i = glm::floor(p);
    glm::vec2 f = glm::fract(p);

    glm::vec2 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
    glm::vec2 du = 30.0f * f * f * (f * (f - 2.0f) + 1.0f);

    glm::vec2 ga = hash(i + glm::vec2(0.0f, 0.0f));
    glm::vec2 gb = hash(i + glm::vec2(1.0f, 0.0f));
    glm::vec2 gc = hash(i + glm::vec2(0.0f, 1.0f));
    glm::vec2 gd = hash(i + glm::vec2(1.0f, 1.0f));

    float va = dot(ga, f - glm::vec2(0.0f, 0.0f));
    float vb = dot(gb, f - glm::vec2(1.0f, 0.0f));
    float vc = dot(gc, f - glm::vec2(0.0f, 1.0f));
    float vd = dot(gd, f - glm::vec2(1.0f, 1.0f));

    return glm::vec3(va + u.x * (vb - va) + u.y * (vc - va) + u.x * u.y * (va - vb - vc + vd),
                     ga + u.x * (gb - ga) + u.y * (gc - ga) + u.x * u.y * (ga - gb - gc + gd) +
                         du * (glm::vec2(u.y, u.x) * (va - vb - vc + vd) + glm::vec2(vb, vc) - va));
}

glm::vec3 FractalNoise(const glm::vec2& p, float freq, int octaves, float lacunarity, float gain) {
    glm::vec3 n = glm::vec3(0.0f);
    float nf = freq;
    float na = 1.0f;
    for (int i = 0; i < octaves; i++) {
        n += noised(p * nf) * na * glm::vec3(1.0f, nf, nf);
        na *= gain;
        nf *= lacunarity;
    }
    return n;
}

// Use the geometric series to calculate the extents of layered noise where the first layer has
// magnitude 1 and each successive layer is multiplied with gain.
float MagnitudeSum(int octaves, float gain)
{
    return (1.0f - glm::pow(gain, static_cast<float>(octaves))) / (1.0f - gain);
}

// The overall erosion scale.
// This is the most important parameter to get right first when applying the erosion
// filter to a height function. Try with the width of a mountain divided by five to ten.
// Value changes can cause abrupt changes in output, especially far away from the origin,
// so this parameter is not well suited for animation or for modulation by other functions.
constexpr static float EROSION_SCALE = 0.08333f;
//#define EROSION_SCALE (0.08*pow(2.0, 2.0*sin(iTime*0.25)))

// The depth of the gullies created.
// The erosion strength is relative to the erosion scale, and values between 0 and 0.5
// usually produce good results.
constexpr static float EROSION_STRENGTH = 0.16f;

// The frequency of gullies is based on the slope length raised to the slope power.
// Note that at sufficiently low gully frequencies, the depth decreases too, since cosine
// wave peaks from different cells blend together. This means gullies effectively disappear
// at flat slopes, such as at mountain peaks. The slope power controls the falloff.
//
// 1.0 : Produces smooth mountain peaks and ridges.
// 0.5 : Produces shaper mountain peaks and ridges.
constexpr static float EROSION_SLOPE_POWER = 0.6f;
//#define EROSION_SLOPE_POWER (0.5 + 0.5 * sin(iTime*0.5))

// Gullies are based on stripes within Voronoi-like cells. The cell scale is relative to
// the overall erosion scale, and values close to 1 usually produce good results. Smaller
// values produce more grainy gullies while larger values produce longer unbroken gullies.
// Tiny values make erosion disappear while huge values produce chaotic curved gullies.
// Value changes can cause abrupt changes in output, especially far away from the origin,
// so this parameter is not well suited for animation or for modulation by other functions.
constexpr static float EROSION_CELL_SCALE = 1.0f;
//#define EROSION_CELL_SCALE pow(2.0, 1.0-2.0*sin(iTime*0.25))

// A value between -1.0 and 1.0 which controls the degree to which the erosion raises or
// lowers the terrain height.
//
// -1.0 : Only lowers the terrain.
// -0.5 : Mostly lowers the terrain, but peaks may be slightly raised.
//  0.0 : In theory neutral, but in practise it raises more than lowers.
//  1.0 : Only raises the terrain.
constexpr static float EROSION_HEIGHT_OFFSET = -0.5f;

constexpr static int EROSION_OCTAVES = 5;

constexpr static float EROSION_GAIN = 0.5f;
//#define EROSION_GAIN (0.3 + 0.3 * sin(iTime*0.5))

constexpr static float EROSION_LACUNARITY = 2.0f;
//#define EROSION_LACUNARITY (2.0 + 1.0 * sin(iTime*0.5))


constexpr static float HEIGHT_TILES = 3.0f;
constexpr static int HEIGHT_OCTAVES = 3;
constexpr static float HEIGHT_AMP = 0.25f;
constexpr static float HEIGHT_GAIN = 0.1f;
constexpr static float HEIGHT_LACUNARITY = 2.0f;

constexpr static float WATER_HEIGHT = 0.465f;

glm::vec2 Heightmap(const glm::vec2 p) {
    // FBM terrain
    auto n = FractalNoise(p, HEIGHT_TILES, HEIGHT_OCTAVES, HEIGHT_LACUNARITY, HEIGHT_GAIN)
		* HEIGHT_AMP;
    
    // [-1, 1] -> [0, 1]
    n = n * 0.5f + glm::vec3(0.5f, 0.0f, 0.0f);

    float strength = EROSION_STRENGTH;

    // Smooth valleys
    //strength *= smoothstep(0.0, 1.0, n.x);

    strength *= glm::smoothstep(WATER_HEIGHT - 0.1f, WATER_HEIGHT + 0.1f, n.x);
    
    int octaves = EROSION_OCTAVES;

    // Store erosion in h:
    auto h = Erosion(p, n, EROSION_SCALE, strength, EROSION_SLOPE_POWER, EROSION_CELL_SCALE, octaves, EROSION_GAIN,
                     EROSION_LACUNARITY);

    // Calculate the offset which satisfies the EROSION_HEIGHT_OFFSET parameter.
    float erosionMagnitude = EROSION_SCALE * strength
        * MagnitudeSum(octaves, EROSION_GAIN);
    float offset = erosionMagnitude * EROSION_HEIGHT_OFFSET;

    return glm::vec2(n.x + h.x + offset, h.x / erosionMagnitude);
}

}

float mg::TerrainHeightSampler::GetBase(const glm::dvec2& pos) const
{   
    auto height = Heightmap(pos * 0.00015).x * 500.0f;
    // auto height = SampleNoiseNormalized(noise_, pos) * 500.0f;
    height -= 100.0f;
    height *= glm::mix(0.1f, 1.0f, GetHeightFalloff(pos, world_size_m_));
    height -= 100.0f;
    return height;
}

float mg::TerrainHeightSampler::GetDetail(const glm::dvec2& pos) const
{
    return SampleNoiseNormalized(noise2_, pos) * 20.0f; // add finer detail
    // return 0.0f;
}

float mg::TerrainHeightSampler::Get(const glm::dvec2& pos) const
{
    return GetBase(pos) + GetDetail(pos);
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

mg::ForestSample mg::ForestSampler::Get(const glm::dvec2& pos) const
{
    ForestSample forest{};
    forest.intensity = GetIntensity(pos);
    forest.density = GetDensity(pos);
    forest.type = GetType(pos);
    return forest;
}

float mg::ForestSampler::GetIntensity(const glm::dvec2& pos) const
{
    auto intensity = SampleNoiseNormalized(intensity_noise_, pos);
    //intensity = glm::pow(intensity, 1.5f); // make low values lower

    return glm::clamp(UnMix(0.45f, 0.6f, intensity), 0.0f, 1.0f); 
}

float mg::ForestSampler::GetDensity(const glm::dvec2& pos) const
{
    return SampleNoiseNormalized(density_noise_, pos);
}

float mg::ForestSampler::GetType(const glm::dvec2& pos) const
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

glm::vec3 mg::TerrainColorSampler::Get(const glm::dvec2& pos, const ForestSample& forest, float sea_level) const
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

glm::vec3 mg::TerrainColorSampler::GetGrassColor(const glm::dvec2& pos) const
{
    constexpr glm::vec3 col0 = glm::vec3(0.4f, 0.6f, 0.3f);
    constexpr glm::vec3 col1 = glm::vec3(0.8f, 0.8f, 0.4f);

    auto t = SampleNoiseNormalized(main_noise_, pos);
    auto tint = glm::mix(col0, col1, t);

    auto t_detail = SampleNoiseNormalized(detail_noise_, pos);
    auto mult = glm::mix(0.9f, 1.1f, t_detail);

    return tint * mult;
}

glm::vec3 mg::TerrainColorSampler::GetForestGroundColor(const glm::dvec2& pos, const ForestSample& forest) const
{
    constexpr glm::vec3 veg_col = glm::vec3(71, 145, 73) / 255.0f;
    constexpr glm::vec3 needle_col = glm::vec3(180, 119, 76) / 255.0f;
    constexpr glm::vec3 needle_col2 = glm::vec3(140, 129, 86) / 255.0f;

    auto col = forest.intensity > 0.7f ? needle_col : veg_col;
    col = glm::mix(col, needle_col2, SampleNoiseNormalized(main_noise_, pos * 2.0 + 1000.0));

    auto t_detail = SampleNoiseNormalized(detail_noise_, pos * 10.0);
    auto mult = glm::mix(0.6f, 1.2f, t_detail);

    return col * mult;

    //auto t = (forest.intensity - 0.3f) / 0.7f;
    //return glm::mix(veg_col, needle_col, t);
}

glm::vec3 mg::TerrainColorSampler::GetSandColor(const glm::dvec2& pos, float sea_level) const
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
        offset_noises_[i].SetSeed(seed ^ (0x5555 * (i + 1)));
        offset_noises_[i].SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        offset_noises_[i].SetFrequency(1.0f);
        offset_noises_[i].SetFractalType(FastNoiseLite::FractalType_FBm);
        offset_noises_[i].SetFractalLacunarity(2.0f);
        offset_noises_[i].SetFractalGain(0.7f);
        offset_noises_[i].SetFractalOctaves(3);
    }
}

float mg::GrassSampler::GetDensity(const glm::dvec2& pos) const
{
    return glm::clamp(UnMix(0.0f, 0.5f, SampleNoiseNormalized(density_noise_, pos)), 0.0f, 1.0f);
}

glm::vec2 mg::GrassSampler::GetOffset(const glm::dvec2& pos) const
{
    auto offset =
        glm::vec2(SampleNoiseNormalized(offset_noises_[0], pos), SampleNoiseNormalized(offset_noises_[1], pos));

    return offset * 2.0f - 1.0f;
}

float mg::GrassSampler::GetBrightness(const glm::dvec2& pos) const
{
    return glm::mix(0.7f, 1.4f, SampleNoiseNormalized(offset_noises_[0], pos * 1.5));
}
