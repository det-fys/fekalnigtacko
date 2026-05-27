#pragma once

#include <memory>
#include "shader.hpp"
#include "surface_render_flags.hpp"

namespace gfx
{


using SurfaceShaderInputFlags = uint8_t;

enum SurfaceShaderInputFlag : SurfaceShaderInputFlags
{
    SIF_COLOR_TEXTURE = 1,
    SIF_OBJECT_COLOR = 2,
    SIF_LIGHTING_DATA = 4,
    SIF_SKELETAL_DATA = 8,
    SIF_DEFORM_DATA = 16,
    SIF_MULTICOLOR_DATA = 32,
    SIF_FOG_DATA = 64,
};

std::unique_ptr<Shader> CreateSurfaceShader(SurfaceRenderFlags flags, SurfaceShaderInputFlags& input_flags);

}