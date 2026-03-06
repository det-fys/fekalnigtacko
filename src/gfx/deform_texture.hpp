#pragma once

#include <span>
#include <glm/glm.hpp>
#include "client/gl.hpp"
#include "utils/defs.hpp"
#include "deform_grid_info.hpp"

namespace gfx
{

class DeformTexture
{
public:
    DeformTexture(const DeformGridInfo& info);
    DELETE_COPY_MOVE(DeformTexture)

    void SetData(std::span<const glm::i8vec3> data);

    const DeformGridInfo& GetInfo() const { return info_; }
    GLuint GetId() const { return id_; }

    ~DeformTexture();

private:
    const DeformGridInfo info_;
    GLuint id_;

};




}