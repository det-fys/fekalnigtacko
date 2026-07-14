#include "skeleton_pose_gl.hpp"
#include "shader_defs.hpp"

#include <cassert>
#include <array>

gfx::SkeletonPoseGL::SkeletonPoseGL(size_t num_bones) : num_bones_(num_bones), ubo_(GL_UNIFORM_BUFFER, GL_DYNAMIC_DRAW)
{
    std::array<glm::mat4, SD_MAX_BONES> buf;
    ubo_.SetData(buf.data(), buf.size() * sizeof(buf[0]));
}

void gfx::SkeletonPoseGL::SetData(std::span<const glm::mat4> data)
{
    assert(data.size() == num_bones_);

    ubo_.SetData(data.data(), data.size_bytes());
}
