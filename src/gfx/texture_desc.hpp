#pragma once

#include "id.hpp"

namespace gfx
{

using TextureID = ID;

enum TextureFilterType : uint8_t
{
    TEXTURE_FILTER_NEAREST,
    TEXTURE_FILTER_LINEAR,
};

enum TextureMipmapsType : uint8_t
{
    TEXTURE_MIPMAP_TYPE_NONE, 
    TEXTURE_MIPMAP_TYPE_LINEAR,
};

struct TextureDescriptor
{
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFilterType filter = TEXTURE_FILTER_NEAREST;
    TextureMipmapsType mipmaps = TEXTURE_MIPMAP_TYPE_NONE;
    uint32_t max_mipmap_level = 0xFFFFFFFF;
    bool linear_rgb = false;
};

}