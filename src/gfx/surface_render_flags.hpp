#pragma once

#include <cstdint>

namespace gfx
{

using SurfaceRenderFlags = uint16_t; 

enum SurfaceRenderFlag : SurfaceRenderFlags
{
    // dont require shader switch
    SRF_BLEND_ADDITIVE = 1,
    SRF_2SIDED = 2,

    // require shader switch
    SRF_CULL_ALPHA = 4,
    SRF_OBJECT_COLOR = 8,
    SRF_OBJECT_COLOR_BACKGROUND = 16,
    SRF_MULTICOLOR = 32,
    SRF_LIT = 64,
    SRF_LIT_VERTEX = 128,
    SRF_TRANSLUCENT = 256,
    SRF_FOG = 512,
    SRF_DEFORM = 1024,
    SRF_SKELETAL = 2048,
    SRF_TEXTURE = 4096,

    SRF__SHADER = SRF_CULL_ALPHA | SRF_OBJECT_COLOR | SRF_OBJECT_COLOR_BACKGROUND | SRF_MULTICOLOR | SRF_LIT |
                  SRF_LIT_VERTEX | SRF_TRANSLUCENT | SRF_FOG | SRF_SKELETAL | SRF_DEFORM | SRF_TEXTURE,

    // order affects visual result
    SRF_BLEND = 8192,

};

};