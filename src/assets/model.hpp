#pragma once

#include <string>
#include <memory>

#include "skeleton.hpp"
#include "utils/defs.hpp"
#include "utils/aabb.hpp"
#include "collision/trianglemesh.hpp"

#ifdef CLIENT
#include "mesh_builder.hpp"
#endif

namespace assets
{

// enum ModelCollisionShapeType
// {
//     MCS_NONE,
    
//     MCS_BOX,
//     MCS_SPHERE,
// };

// struct ModelCollisionShape
// {
//     ModelCollisionShapeType type = MCS_NONE;
//     glm::vec3 origin = glm::vec3(0.0f);
//     union
//     {
//         float radius;
//         glm::vec3 half_extents;
//     };
// };

class Model
{
public:
    Model() = default;
    static std::shared_ptr<const Model> LoadFromFile(const std::string& filename);

    const std::string& GetName() const { return name_; }
    
    const glm::vec3& GetColOffset() const { return col_offset_; }
    const collision::TriangleMesh* GetColMesh() const { return cmesh_.get(); }
    btCollisionShape* GetColShape() const { return cshape_.get(); }

    const std::shared_ptr<const Skeleton>& GetSkeleton() const { return skeleton_; }
    CLIENT_ONLY(const std::shared_ptr<const Mesh>& GetMesh() const { return mesh_; })
    const AABB3& GetAABB() const { return aabb_; }

    const std::string* GetParam(const std::string& key) const;
    bool GetParamFloat(const std::string& key, float& out) const;

private:
    std::string name_;
    glm::vec3 col_offset_ = glm::vec3(0.0f);
    std::unique_ptr<collision::TriangleMesh> cmesh_;
    // std::vector<ModelCollisionShape> cshapes_;
    std::vector<std::unique_ptr<btCollisionShape>> subshapes_;
    std::unique_ptr<btCollisionShape> cshape_;

    std::shared_ptr<const Skeleton> skeleton_;
    CLIENT_ONLY(std::shared_ptr<const Mesh> mesh_;);
    AABB3 aabb_;

    std::map<std::string, std::string> params_;

};

}