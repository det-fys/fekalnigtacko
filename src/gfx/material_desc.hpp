#pragma once

#include "id.hpp"
#include "texture_desc.hpp"

namespace gfx
{

using MaterialID = ID;

enum MaterialBlendType : uint8_t
{
    MATERIAL_BLEND_TYPE_NONE,
    MATERIAL_BLEND_TYPE_OPACITY,
    MATERIAL_BLEND_TYPE_ADDITIVE,
};

enum MaterialObjectColorType : uint8_t
{
    MATERIAL_OBJECT_COLOR_TYPE_NONE,
    MATERIAL_OBJECT_COLOR_TYPE_MULTIPLY,
    MATERIAL_OBJECT_COLOR_TYPE_BACKGROUND,
    MATERIAL_OBJECT_COLOR_TYPE_MULTICOLOR,
};

enum MaterialLightingType : uint8_t
{
    MATERIAL_LIGHTING_TYPE_NORMAL,
    MATERIAL_LIGHTING_TYPE_VERTEX,
    MATERIAL_LIGHTING_TYPE_UNLIT,
};

struct MaterialProperties
{
    bool twosided = false;
    MaterialBlendType blend = MATERIAL_BLEND_TYPE_NONE;
    MaterialObjectColorType color = MATERIAL_OBJECT_COLOR_TYPE_NONE;
    MaterialLightingType lighting = MATERIAL_LIGHTING_TYPE_NORMAL;
    bool translucent = false;
    bool decal = false;
};

struct MaterialDescriptor
{
    TextureID texture = 0;
    MaterialProperties properties{};
};

}