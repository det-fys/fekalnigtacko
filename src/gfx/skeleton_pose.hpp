#pragma once

#include <span>

#include <glm/glm.hpp>

#include "skeleton_pose_desc.hpp"
#include "utils/defs.hpp"

namespace gfx
{

class SkeletonPose
{
public:
    SkeletonPose(const SkeletonPoseDescriptor& desc);
    DELETE_COPY_MOVE(SkeletonPose);

    void SetTransforms(std::span<const glm::mat4> data);

    SkeletonPoseID GetID() const { return id_; }

    ~SkeletonPose();

private:
    SkeletonPoseID id_;
};

} // namespace gfx