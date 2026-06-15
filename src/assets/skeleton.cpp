#include "skeleton.hpp"

#include "cmdfile.hpp"
#include "collision/shape_info.hpp"

#include <stdexcept>

std::shared_ptr<const assets::Skeleton> assets::Skeleton::LoadFromFile(const std::string& filename)
{
    auto skeleton = std::make_shared<Skeleton>();

    LoadCMDFile(filename, [&](const std::string& command, std::istringstream& iss) {
        if (command == "b")
        {
            Transform t;
            std::string bone_name, parent_name;

            iss >> bone_name >> parent_name;
            ParseTransform(iss, t);

            if (iss.fail())
            {
                throw std::runtime_error("Failed to parse bone definition in file: " + filename);
            }

            skeleton->AddBone(bone_name, parent_name, t);
        }
        else if (command == "anim")
        {
            std::string anim_name, anim_filename;
            iss >> anim_name >> anim_filename;

            if (iss.fail())
            {
                throw std::runtime_error("Failed to parse animation definition in file: " + filename);
            }

            std::shared_ptr<const Animation> anim =
                Animation::LoadFromFile("data/" + anim_filename + ".anim", skeleton.get());
            skeleton->AddAnimation(anim_name, anim);
        }
        else if (command == "hitbone")
        {
            auto& hitbone = skeleton->hit_bones_.emplace_back();

            std::string shape_name, bone_name;
            float sy, sz;
            iss >> hitbone.name >> bone_name >> shape_name;
            ParseTransform(iss, hitbone.offset);
            iss >> sy >> sz;

            int bone_idx = skeleton->GetBoneIndex(bone_name);
            hitbone.bone_idx = bone_idx >= 0 ? bone_idx : 0; 

            glm::vec3 shape_size(hitbone.offset.scale, sy, sz);
            hitbone.offset.scale = 1.0f;

            if (shape_name == "capsule")
            {
                hitbone.col_shape = std::make_unique<btCapsuleShapeZ>(shape_size.x, shape_size.z); // TODO: check dimenmsions
            }
            else
            {
                throw std::runtime_error("Unknown hitbone shape: " + shape_name);
            }

            collision::SetShapeMaterial(*hitbone.col_shape, collision::PM_FLESH);
        }
        else if (command == "loc")
        {
            std::string loc_name, bone_name;
            iss >> loc_name >> bone_name;

            auto& loc = skeleton->locations_[loc_name];
            ParseTransform(iss, loc.offset);

            loc.bone_name = bone_name;
            int bone_idx = skeleton->GetBoneIndex(bone_name);
            loc.bone_idx = bone_idx >= 0 ? bone_idx : 0; 
        }
    });

    skeleton->AddAimBones();

    return skeleton;
}

int assets::Skeleton::GetBoneIndex(const std::string& name) const
{
    auto it = bone_map_.find(name);
    if (it != bone_map_.end())
    {
        return it->second;
    }

    return -1;
}

assets::AnimIdx assets::Skeleton::GetAnimationIdx(const std::string& name) const
{
    auto it = anim_idxs_.find(name);
    if (it != anim_idxs_.end())
    {
        return it->second;
    }
    return NO_ANIM;
}

const assets::Animation* assets::Skeleton::GetAnimation(AnimIdx idx) const
{
    if (idx >= anims_.size())
        return nullptr;

    return anims_[idx].get();
}

const assets::Animation* assets::Skeleton::GetAnimation(const std::string& name) const
{
    return GetAnimation(GetAnimationIdx(name));
}

const assets::SkeletonLocation* assets::Skeleton::GetLocation(const std::string& name) const
{
    auto it = locations_.find(name);
    if (it == locations_.end())
        return nullptr;

    return &it->second;
}

void assets::Skeleton::AddBone(const std::string& name, const std::string& parent_name, const Transform& transform)
{
    int index = static_cast<int>(bones_.size());

    Bone& bone = bones_.emplace_back();
    bone.name = name;
    bone.parent_idx = GetBoneIndex(parent_name);
    bone.bind_transform = transform;
    bone.inv_bind_matrix = glm::inverse(transform.ToMatrix());

    bone_map_[bone.name] = index;
}

void assets::Skeleton::AddAnimation(const std::string& name, const std::shared_ptr<const Animation>& anim)
{
    anim_idxs_[name] = anims_.size();
    anims_.push_back(anim);
}

void assets::Skeleton::AddAimBones()
{
    AddAimBone("DEF-spine.002", 1.0f, glm::vec3(0.0f, 1.0f, 0.0f), 0.0f, glm::vec3(0.0f));
    AddAimBone("spine_fk.002", 1.0f, glm::vec3(0.0f, 0.0f, 1.0f), 0.0f, glm::vec3(0.0f));

    AddAimBone("DEF-spine.002", 0.0f, glm::vec3(0.0f), 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
    AddAimBone("MCH-spine.002", 0.0f, glm::vec3(0.0f), 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
    AddAimBone("DEF-spine.003", 0.0f, glm::vec3(0.0f), 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
    AddAimBone("MCH-spine.003", 0.0f, glm::vec3(0.0f), 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
}

void assets::Skeleton::AddAimBone(const std::string& name, float yaw_weight, const glm::vec3& yaw_axis, float pitch_weight, const glm::vec3& pitch_axis)
{
    auto idx = GetBoneIndex(name);
    if (idx < 0)
        return;

    AimBone aimbone{};
    aimbone.idx = idx;
    aimbone.yaw_weight = yaw_weight;
    aimbone.yaw_axis = yaw_axis;
    aimbone.pitch_weight = pitch_weight;
    aimbone.pitch_axis = pitch_axis;
    aim_bones_.emplace_back(aimbone);
}
