#pragma once

#include <span>

#include "deform_texture_desc.hpp"
#include "utils/defs.hpp"

namespace gfx
{

class DeformTexture
{
public:
    DeformTexture(const DeformTextureDescriptor& desc);
    DELETE_COPY_MOVE(DeformTexture);

    void SetData(std::span<const glm::i8vec3> data);

    ~DeformTexture();

private:
    DeformTextureID id_;
};

} // namespace gfx
