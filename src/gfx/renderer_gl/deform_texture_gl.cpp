#include "deform_texture_gl.hpp"

#include <cassert>
#include <stdexcept>

gfx::DeformTextureGL::DeformTextureGL(const DeformGridInfo& info) : info_(info)
{
    glGenTextures(1, &id_);

    if (!id_)
        throw std::runtime_error("Could not create deform texture!");

    glBindTexture(GL_TEXTURE_3D, id_);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_3D, 0);
}

void gfx::DeformTextureGL::SetData(std::span<const glm::i8vec3> data)
{
    assert(data.size() >= info_.res.x * info_.res.y * info_.res.z);

    glBindTexture(GL_TEXTURE_3D, id_);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB8_SNORM, info_.res.x, info_.res.y, info_.res.z, 0, GL_RGB, GL_BYTE, data.data());
}

gfx::DeformTextureGL::~DeformTextureGL()
{
    glDeleteTextures(1, &id_);
}
