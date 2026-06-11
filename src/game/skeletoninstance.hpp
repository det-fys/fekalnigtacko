#pragma once

#include "assets/skeleton.hpp"
#include "transform_node.hpp"
#include "utils/defs.hpp"

namespace game
{

class SkeletonInstance
{
public:
    SkeletonInstance() = default;
    SkeletonInstance(std::shared_ptr<const assets::Skeleton> skeleton, const TransformNode* root_node);

    const TransformNode* GetRootNode() const { return root_node_; }
    const TransformNode& GetBoneNode(size_t index) const { return bone_nodes_[index]; }

    void ApplySkelAnim(const assets::Animation& anim, float time, float weight);
    void ApplyAim(float yaw, float pitch);

    void UpdateBoneMatrices();

    const std::vector<TransformNode> GetBoneNodes() const { return bone_nodes_; }

    const std::shared_ptr<const assets::Skeleton>& GetSkeleton() const { return skeleton_; }

private:
    void SetupBoneNodes();

private:
    std::shared_ptr<const assets::Skeleton> skeleton_;
    const TransformNode* root_node_ = nullptr;
    std::vector<TransformNode> bone_nodes_;
};

} // namespace game