#include "skeleton_pose_gl.hpp"

#include <cassert>

gfx::SkeletonPoseGL::SkeletonPoseGL(size_t num_bones) : num_bones_(num_bones), ubo_(GL_UNIFORM_BUFFER, GL_DYNAMIC_DRAW) {}

void gfx::SkeletonPoseGL::SetData(std::span<const glm::mat4> data)
{
    assert(data.size() == num_bones_);

    ubo_.SetData(data.data(), data.size_bytes());
}
