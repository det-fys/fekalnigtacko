#pragma once

#include "surface_pipeline_wgpu.hpp"

namespace gfx
{

// pipeline flags that affect shader sources
constexpr SurfacePipelineFlags PIPELINE_FLAGS_SHADER = SPF_CULL_ALPHA | SPF_OBJECT_COLOR | SPF_OBJECT_COLOR_BACKGROUND |
                                                       SPF_MULTICOLOR | SPF_LIT | SPF_TRANSLUCENT | SPF_FOG |
                                                       SPF_DEFORM | SPF_SKELETAL | SPF_TEXTURE | SPF_DEPTH_ONLY;

enum ShadowSampleFunction
{
    SHADOW_SAMPLE_DEFAULT,
    SHADOW_SAMPLE_PCF2X2,
    SHADOW_SAMPLE_PCF3X3,
    SHADOW_SAMPLE_IGN,
};

struct ShaderConfig
{
    ShadowSampleFunction shadow_sample_func = SHADOW_SAMPLE_DEFAULT;
};

wgpu::ShaderModule CreateSurfaceShaderWGPU(const wgpu::Device& device, SurfacePipelineFlags flags,
                                           const ShaderConfig& cfg);


}
