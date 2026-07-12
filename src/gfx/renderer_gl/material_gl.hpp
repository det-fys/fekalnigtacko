#pragma once

#include "../material_desc.hpp"

namespace gfx
{

struct GLMaterial
{
public:
    GLMaterial(const MaterialDescriptor& desc) : desc_(desc) {}

    const MaterialProperties& GetProperties() const { return desc_.properties; }
    TextureID GetTexture() const { return desc_.texture; }

private:
    MaterialDescriptor desc_;
};

} // namespace gfx
