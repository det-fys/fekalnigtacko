#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "animation.hpp"
#include "utils/transform.hpp"
#include "asset_manager.hpp"

namespace assets
{

struct Bone
{
    int parent_idx;
    std::string name;
    Transform bind_transform;
    glm::mat4 inv_bind_matrix;
};

using AnimIdx = uint8_t;
constexpr AnimIdx NO_ANIM = 255;

struct AimBone
{
    size_t idx;
    float yaw_weight;
    glm::vec3 yaw_axis;
    float pitch_weight;
    glm::vec3 pitch_axis;
};

struct HitBone
{
    size_t bone_idx = 0;
    std::string name;
    Transform offset;
    std::unique_ptr<btCollisionShape> col_shape;
};

struct SkeletonLocation
{
    size_t bone_idx = 0;
    std::string bone_name;
    Transform offset;
};

class Skeleton : public Asset
{
public:
    Skeleton() = default;
    static std::shared_ptr<Skeleton> Load(const std::string& name);
    static std::shared_ptr<Skeleton> LoadFromFile(const std::string& filename);

    int GetBoneIndex(const std::string& name) const;

    size_t GetNumBones() const { return bones_.size(); }
    const Bone& GetBone(size_t idx) const { return bones_[idx]; }

    AnimIdx GetAnimationIdx(const std::string& name) const;
    const Animation* GetAnimation(AnimIdx idx) const;
    const Animation* GetAnimation(const std::string& name) const;

    const std::vector<AimBone>& GetAimBones() const { return aim_bones_; }
    const std::vector<HitBone>& GetHitBones() const { return hit_bones_; }
    const SkeletonLocation* GetLocation(const std::string& name) const;

private:
    void AddBone(const std::string& name, const std::string& parent_name, const Transform& transform);
    void AddAnimation(const std::string& name, const std::shared_ptr<const Animation>& anim);

    void AddAimBones();
    void AddAimBone(const std::string& name, float yaw_weight, const glm::vec3& yaw_axis, float pitch_weight, const glm::vec3& pitch_axis);

private:
    std::string name_;
    std::vector<Bone> bones_;
    std::map<std::string, int> bone_map_;

    std::vector<std::shared_ptr<const Animation>> anims_;
    std::map<std::string, AnimIdx> anim_idxs_;

    std::vector<AimBone> aim_bones_;
    std::vector<HitBone> hit_bones_;
    std::map<std::string, SkeletonLocation> locations_;
};

} // namespace assets