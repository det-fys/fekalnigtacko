#pragma once

#include <cstdint>
#include <map>
#include <webgpu/webgpu_cpp.h>

namespace gfx
{

using SurfacePipelineFlags = uint32_t;
enum SurfacePipelineFlag : SurfacePipelineFlags
{
    // pipeline, inner order independent
    SPF_BLEND_ADDITIVE = 1,
    SPF_2SIDED = 2,
    SPF_CULL_ALPHA = 4,
    SPF_OBJECT_COLOR = 8,
    SPF_OBJECT_COLOR_BACKGROUND = 16,
    SPF_MULTICOLOR = 32,
    SPF_LIT = 64,
    SPF_TRANSLUCENT = 128,
    SPF_FOG = 256,
    SPF_DEFORM = 512,
    SPF_SKELETAL = 1024,
    SPF_TEXTURE = 2048,

    // order dependent
    SPF_DECAL = 4096,
    SPF_BLEND = 8192,

    // whole pass
    SPF_DEPTH_ONLY = 16384,
    SPF_SHADOW_MAP = 32768,
};

}