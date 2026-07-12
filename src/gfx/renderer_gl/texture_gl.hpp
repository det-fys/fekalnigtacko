#pragma once

#include <span>

#include "gl.hpp"
#include "utils/defs.hpp"

namespace gfx
{

class GLTexture
{
public:
    GLTexture(uint32_t width, uint32_t height, GLint internalformat, GLenum format, GLenum type, bool linear, bool mipmaps);
    DELETE_COPY_MOVE(GLTexture);

    void SetData(std::span<const uint8_t> data);

    ~GLTexture();

    GLuint GetId() const { return id_; }

private:
    uint32_t width_, height_;
    GLint internalformat_;
    GLenum format_;
    GLenum type_;
    bool linear_;
    bool mipmaps_;

    GLuint id_;
};

} // namespace gfx
