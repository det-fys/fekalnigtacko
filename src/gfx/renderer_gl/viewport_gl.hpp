#pragma once

#include <cstdint>

#include "gl.hpp"
#include "utils/defs.hpp"

namespace gfx
{

class ViewportGL
{
public:
    ViewportGL();
    DELETE_COPY_MOVE(ViewportGL);

    void Setup(uint32_t width, uint32_t height);

    GLuint GetFramebufferID() const { return fb_id_; }
    GLuint GetColorTextureID() const { return color_tex_id_; }
    GLuint GetDepthTextureID() const { return depth_tex_id_; }

    ~ViewportGL();

private:
    void CreateResources();
    void ReleaseResources();

private:
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    GLuint fb_id_ = 0;
    GLuint color_tex_id_ = 0;
    GLuint depth_tex_id_ = 0;
};



}