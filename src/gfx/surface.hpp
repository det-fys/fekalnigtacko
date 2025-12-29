#pragma once

#include "texture.hpp"
#include "vertex_array.hpp"

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
    SF_DOUBLE_SIDED = 0x01,
    SF_TRANSPARENT = 0x02,
    SF_OBJECT_COLOR = 0x08, // use object level tint
};

struct Surface
{
    std::shared_ptr<const gfx::Texture> texture;
    std::shared_ptr<const gfx::VertexArray> va;
    size_t first = 0; // first triangle VA EBO
    size_t count = 0; // number of triangles
    MeshFlags mflags = MF_NONE;
    SurfaceFlags sflags = SF_NONE;
};

} // namespace gfx