#pragma once

#include "surface_pipeline_wgpu.hpp"

namespace gfx
{

// pipeline flags that affect shader sources
constexpr SurfacePipelineFlags PIPELINE_FLAGS_SHADER = SPF_CULL_ALPHA | SPF_OBJECT_COLOR | SPF_OBJECT_COLOR_BACKGROUND |
                                                       SPF_MULTICOLOR | SPF_LIT | SPF_TRANSLUCENT | SPF_FOG |
                                                       SPF_DEFORM | SPF_SKELETAL | SPF_TEXTURE | SPF_DEPTH_ONLY;

wgpu::ShaderModule CreateSurfaceShaderWGPU(const wgpu::Device& device, SurfacePipelineFlags flags);


}
