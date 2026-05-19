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

using SurfaceFlags = uint8_t;

enum SurfaceFlag : SurfaceFlags
{
    SF_NONE = 0x00,
    SF_2SIDED = 0x01,         // disable backface culling
    SF_BLEND = 0x02,          // enable blending, disable depth write
    SF_BLEND_ADDITIVE = 0x04, // use additive blending instead of opacity
    SF_OBJECT_COLOR = 0x08,   // use object color for background instead of alpha culling
    SF_DEFORM_GRID = 0x10,    // use deform grid
    SF_UNLIT = 0x20,          // dont apply lighting
    SF_OBJECT_COLOR_MULT = 0x40, // object color multiplies instead of acting as background
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