#include "viewport_gl.hpp"

#include <stdexcept>

gfx::ViewportGL::ViewportGL() {}

void gfx::ViewportGL::Setup(uint32_t width, uint32_t height)
{
    if (width_ == width && height_ == height)
    {
        return;
    }

    ReleaseResources();

    width_ = width;
    height_ = height;
    CreateResources();
}

gfx::ViewportGL::~ViewportGL()
{
    ReleaseResources();
}

void gfx::ViewportGL::CreateResources()
{
    if (width_ == 0 || height_ == 0)
    {
        return;
    }

    glGenFramebuffers(1, &fb_id_);
    glBindFramebuffer(GL_FRAMEBUFFER, fb_id_);

    // Create color texture
    glGenTextures(1, &color_tex_id_);
    glBindTexture(GL_TEXTURE_2D, color_tex_id_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex_id_, 0);
    
    // Create depth texture
    glGenTextures(1, &depth_tex_id_);
    glBindTexture(GL_TEXTURE_2D, depth_tex_id_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width_, height_, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_tex_id_, 0);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        throw std::runtime_error("Failed to create framebuffer for viewport");
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void gfx::ViewportGL::ReleaseResources() 
{
    if (fb_id_ != 0)
    {
        glDeleteFramebuffers(1, &fb_id_);
        fb_id_ = 0;
    }
    if (color_tex_id_ != 0)
    {
        glDeleteTextures(1, &color_tex_id_);
        color_tex_id_ = 0;
    }
    if (depth_tex_id_ != 0)
    {
        glDeleteTextures(1, &depth_tex_id_);
        depth_tex_id_ = 0;
    }
}
