#include "texture_gl.hpp"
#include <stdexcept>
#include <cassert>

static void GetGLFilterModes(bool linear, bool mipmaps, GLenum& filter_min, GLenum& filter_mag) {
	if (linear)
	{
		filter_min = GL_LINEAR;
		filter_mag = GL_LINEAR;
		
		if (mipmaps)
		{
			filter_min = GL_LINEAR_MIPMAP_LINEAR;
		}
	}
	else
	{
		filter_min = GL_NEAREST;
		filter_mag = GL_NEAREST;
		
		if (mipmaps)
		{
			// Mipmaps always linear
			filter_min = GL_LINEAR_MIPMAP_LINEAR;
		}
	}
}

gfx::TextureGL::TextureGL(uint32_t width, uint32_t height, GLint internalformat, GLenum format, GLenum type,
                          bool linear, bool mipmaps)
    : width_(width), height_(height), internalformat_(internalformat), format_(format), type_(type), linear_(linear),
      mipmaps_(mipmaps)
{
    glGenTextures(1, &id_);

    if (!id_)
        throw std::runtime_error("Could not create GL texture!");

    glBindTexture(GL_TEXTURE_2D, id_);

    GLenum filter_min, filter_mag;
    GetGLFilterModes(linear, mipmaps, filter_min, filter_mag);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_min);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_mag);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    if (mipmaps_)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
    }
}

void gfx::TextureGL::SetData(std::span<const uint8_t> data)
{
    assert(data.size() >= width_ * height_ * 4);

    glBindTexture(GL_TEXTURE_2D, id_);

    glTexImage2D(GL_TEXTURE_2D, 0, internalformat_, width_, height_, 0, format_, type_, data.data());
    
    if (mipmaps_)
    {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
}

gfx::TextureGL::~TextureGL()
{
    glDeleteTextures(1, &id_);
}
