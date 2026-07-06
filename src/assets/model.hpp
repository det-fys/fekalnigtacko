#pragma once

#include <string>
#include <memory>

#include "skeleton.hpp"
#include "utils/defs.hpp"
#include "utils/aabb.hpp"
#include "collision/trianglemesh.hpp"
#include "asset_manager.hpp"

namespace assets
{

struct ModelVertexBoneInfluence
{
    int bone_index = -1;
    float weight = 0.0f;
};

struct ModelVertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec2 lightmap_uv;
    ModelVertexBoneInfluence bones[4];
};

struct ModelTriangle
{
    uint32_t vert[3];
};

struct ModelSurface
{
    std::string name;
    size_t first_tri = 0;
    size_t num_tris = 0;
    std::string texture_name;
    bool two_sided = false;
    bool object_color = false;
    bool object_color_mult = false;
    bool multicolor = false;
    bool blend = false;
    bool blend_additive = false;
    bool unlit = false;
    bool translucent = false;
};

class Model : public Asset
{
public:
    Model() = default;
    static std::shared_ptr<Model> Load(const std::string& name);
    static std::shared_ptr<Model> LoadFromFile(const std::string& filename);
    
    const std::span<const ModelVertex> GetVertices() const { return vertices_; }
    const std::span<const ModelTriangle> GetTriangles() const { return tris_; }
    const std::span<const ModelSurface> GetSurfaces() const { return surfaces_; }
    bool GetSurfaceIndex(const std::string& name, size_t& idx) const;

    const glm::vec3& GetColOffset() const { return col_offset_; }
    const collision::TriangleMesh* GetColMesh() const { return cmesh_.get(); }
    btCollisionShape* GetColShape() const { return cshape_.get(); }
    bool IsColShapeBulletTarget() const { return cshape_is_bullet_target_; }

    const std::shared_ptr<const Skeleton>& GetSkeleton() const { return skeleton_; }
    const AABB3& GetAABB() const { return aabb_; }

    const std::string* GetParam(const std::string& key) const;
    bool GetParamFloat(const std::string& key, float& out) const;
    
    const Transform* GetLocation(const std::string& key) const;
    
private:
    std::vector<ModelVertex> vertices_;
    std::vector<ModelTriangle> tris_;
    std::vector<ModelSurface> surfaces_;
    std::map<std::string, size_t> surface_indices_;
    
    glm::vec3 col_offset_ = glm::vec3(0.0f);
    std::unique_ptr<collision::TriangleMesh> cmesh_;
    // std::vector<ModelCollisionShape> cshapes_;
    std::vector<std::unique_ptr<btCollisionShape>> subshapes_;
    std::unique_ptr<btCollisionShape> cshape_;
    bool cshape_is_bullet_target_ = false;

    std::shared_ptr<const Skeleton> skeleton_;

    AABB3 aabb_;

    std::map<std::string, std::string> params_;
    std::map<std::string, Transform> locations_;

};

}