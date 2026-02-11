#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "animation.hpp"
#include "utils/transform.hpp"

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

class Skeleton
{
public:
    Skeleton() = default;
    static std::shared_ptr<const Skeleton> LoadFromFile(const std::string& filename);

    int GetBoneIndex(const std::string& name) const;

    size_t GetNumBones() const { return bones_.size(); }
    const Bone& GetBone(size_t idx) const { return bones_[idx]; }

    AnimIdx GetAnimationIdx(const std::string& name) const;
    const Animation* GetAnimation(AnimIdx idx) const;
    const Animation* GetAnimation(const std::string& name) const;

private:
    void AddBone(const std::string& name, const std::string& parent_name, const Transform& transform);
    void AddAnimation(const std::string& name, const std::shared_ptr<const Animation>& anim);

private:
    std::vector<Bone> bones_;
    std::map<std::string, int> bone_map_;

    std::vector<std::shared_ptr<const Animation>> anims_;
    std::map<std::string, AnimIdx> anim_idxs_;
};

} // namespace assets