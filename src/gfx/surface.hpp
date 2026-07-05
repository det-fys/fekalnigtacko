#pragma once

#include "texture.hpp"
#include "vertex_array.hpp"
#include "deform_texture.hpp"

namespace gfx
{

using MeshFlags = uint8_t;

enum MeshFlag : MeshFlags
{
    MF_NONE = 0x00,
    MF_LIGHTMAP_UV = 0x01, // unused
    MF_SKELETAL = 0x02,
};

using SurfaceFlags = uint16_t;

enum SurfaceFlag : SurfaceFlags
{
    SF_NONE = 0x00,
    SF_2SIDED = 1,              // disable backface culling
    SF_BLEND = 2,               // enable blending, disable depth write
    SF_BLEND_ADDITIVE = 4,      // use additive blending instead of opacity
    SF_OBJECT_COLOR = 8,        // use object color for background instead of alpha culling
    SF_DEFORM_GRID = 16,        // use deform grid
    SF_UNLIT = 32,              // dont apply lighting
    SF_VERTEX_LIT = 64,         // force vertex lighting (only if not SF_UNLIT)
    SF_OBJECT_COLOR_MULT = 128, // object color multiplies instead of acting as background
    SF_MULTICOLOR = 256,        // multiple color slots encoded in alpha
};

struct Surface
{
    std::shared_ptr<const Texture> texture;
    std::shared_ptr<const VertexArray> va;
    size_t first = 0; // first triangle VA EBO
    size_t count = 0; // number of triangles
    MeshFlags mflags = MF_NONE;
    SurfaceFlags sflags = SF_NONE;
    std::shared_ptr<const DeformTexture> deform_tex;
};

} // namespace gfx