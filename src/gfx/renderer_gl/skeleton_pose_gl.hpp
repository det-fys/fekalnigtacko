#pragma once

#include <span>

#include <glm/glm.hpp>

#include "buffer_object.hpp"

namespace gfx
{

class SkeletonPoseGL
{
public:
    SkeletonPoseGL(size_t num_bones);

    void SetData(std::span<const glm::mat4> data);

    GLuint GetUboId() const { return ubo_.GetId(); }

private:
    size_t num_bones_;
    BufferObject ubo_;
};

} // namespace gfx
