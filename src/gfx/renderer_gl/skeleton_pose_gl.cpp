#include "skeleton_pose_gl.hpp"

#include <cassert>

gfx::GLSkeletonPose::GLSkeletonPose(size_t num_bones) : num_bones_(num_bones), ubo_(GL_UNIFORM_BUFFER, GL_DYNAMIC_DRAW) {}

void gfx::GLSkeletonPose::SetData(std::span<const glm::mat4> data)
{
    assert(data.size() == num_bones_);

    ubo_.SetData(data.data(), data.size_bytes());
}
