#include "skeletoninstance.hpp"

game::SkeletonInstance::SkeletonInstance(std::shared_ptr<const assets::Skeleton> skeleton,
                                         const TransformNode* root_node)
    : skeleton_(std::move(skeleton)), root_node_(root_node)
{
    SetupBoneNodes();
}

void game::SkeletonInstance::ApplySkelAnim(const assets::Animation& anim, float time, float weight)
{
    float anim_frame = time * anim.GetTPS();

    if (anim_frame < 0.0f)
        anim_frame = 0.0f;

    size_t num_anim_frames = anim.GetNumFrames();

    size_t frame_i = static_cast<size_t>(anim_frame);
    size_t frame0 = frame_i % num_anim_frames;
    size_t frame1 = (frame_i + 1) % num_anim_frames;
    float t = anim_frame - static_cast<float>(frame_i);

    for (size_t i = 0; i < anim.GetNumChannels(); ++i)
    {
        const assets::AnimationChannel& channel = anim.GetChannel(i);
        TransformNode& node = bone_nodes_[channel.bone_index];

        const Transform* t0 = channel.frames[frame0];
        const Transform* t1 = channel.frames[frame1];

        Transform anim_transform;

        if (t0 != t1)
        {
            anim_transform = Transform::Lerp(*t0, *t1, t); // Frames are different, interpolate
        }
        else
        {
            anim_transform = *t0; // Frames are the same, no interpolation needed
        }

        if (weight < 1.0f)
        {
            // blend with existing transform
            node.local = Transform::Lerp(node.local, anim_transform, weight);
        }
        else
        {
            node.local = anim_transform;
        }
    }
}

void game::SkeletonInstance::UpdateBoneMatrices()
{
    for (TransformNode& node : bone_nodes_)
    {
        node.UpdateMatrix();
    }
}

void game::SkeletonInstance::SetupBoneNodes()
{
    size_t num_bones = skeleton_->GetNumBones();
    bone_nodes_.resize(num_bones);

    for (size_t i = 0; i < num_bones; ++i)
    {
        const auto& bone = skeleton_->GetBone(i);
        TransformNode& node = bone_nodes_[i];

        node.local = bone.bind_transform;

        if (bone.parent_idx >= 0)
        {
            node.parent = &bone_nodes_[bone.parent_idx];
        }
        else
        {
            node.parent = root_node_;
        }
    }

    UpdateBoneMatrices();
}
