#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace mg
{

struct MapConfig
{
    uint32_t chunks = 128;
    float chunk_size_m = 256.0f;
    uint32_t chunk_global_tiles = 1;
    uint32_t chunk_tiles = 512;
    uint32_t chunk_border = 32;

    uint32_t seed = 420;

};

}
