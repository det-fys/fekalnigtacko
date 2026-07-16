#pragma once

#include <span>
#include <glm/glm.hpp>
#include "gl.hpp"
#include "utils/defs.hpp"
#include "../deform_grid_info.hpp"

namespace gfx
{

class DeformTextureGL
{
public:
    DeformTextureGL(const DeformGridInfo& info);
    DELETE_COPY_MOVE(DeformTextureGL)

    void SetData(std::span<const glm::i8vec3> data);

    const DeformGridInfo& GetInfo() const { return info_; }
    GLuint GetId() const { return id_; }

    ~DeformTextureGL();

private:
    const DeformGridInfo info_;
    GLuint id_;

};




}