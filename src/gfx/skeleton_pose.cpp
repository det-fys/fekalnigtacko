#include "skeleton_pose.hpp"

#include "renderer.hpp"

gfx::SkeletonPose::SkeletonPose(const SkeletonPoseDescriptor& desc)
{
    id_ = Renderer::GetInstance().CreateSkeletonPose(desc);
}

void gfx::SkeletonPose::SetTransforms(std::span<const glm::mat4> data)
{
    Renderer::GetInstance().SetSkeletonPoseTransforms(id_, data);
}

gfx::SkeletonPose::~SkeletonPose()
{
    Renderer::GetInstance().ReleaseSkeletonPose(id_);
}
