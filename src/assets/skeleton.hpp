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

class Skeleton
{
public:
    Skeleton() = default;
    static std::shared_ptr<const Skeleton> LoadFromFile(const std::string& filename);

    int GetBoneIndex(const std::string& name) const;

    size_t GetNumBones() const { return bones_.size(); }
    const Bone& GetBone(size_t idx) const { return bones_[idx]; }

    const Animation* GetAnimation(const std::string& name) const;

private:
    void AddBone(const std::string& name, const std::string& parent_name, const Transform& transform);
    void AddAnimation(const std::string& name, const std::shared_ptr<const Animation>& anim) { anims_[name] = anim; }

private:
    std::vector<Bone> bones_;
    std::map<std::string, int> bone_map_;

    std::map<std::string, std::shared_ptr<const Animation>> anims_;
};

} // namespace assets