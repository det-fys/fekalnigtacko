#pragma once

#include <string>
#include <memory>

#include "skeleton.hpp"
#include "utils/defs.hpp"
#include "utils/aabb.hpp"
#include "collision/trianglemesh.hpp"
#include "asset_manager.hpp"

#include "gfx/mesh_desc.hpp"
#include "gfx/material_desc.hpp"

namespace assets
{

struct ModelVertexData
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<gfx::MeshVertexBoneData> bones;
};

struct ModelSurface
{
    std::string name;
    uint32_t tri_offset = 0;
    uint32_t tri_count = 0;
    std::string texture_name;
    gfx::MaterialProperties properties;
};

struct ModelCollisionSurface
{
    collision::Material material = collision::PM_NONE;
    uint32_t tri_offset = 0;
    uint32_t tri_count = 0;
};

enum ModelCollisionShapeType
{
    MODEL_COLLISION_SHAPE_BOX,
    MODEL_COLLISION_SHAPE_SPHERE,
};

struct ModelCollisionShape
{
    ModelCollisionShapeType type;
    glm::vec3 size;
    Transform transform;
};

struct ModelDescriptor
{
    std::shared_ptr<const Skeleton> skeleton;
    ModelVertexData verts;
    std::vector<gfx::MeshTriangle> tris;
    std::vector<ModelSurface> surfaces;
    std::vector<ModelCollisionSurface> col_surfaces;
    collision::Material col_material = collision::PM_NONE;
    std::vector<ModelCollisionShape> col_shapes;
    bool make_convex_hull = false;
    bool make_triangle_mesh = false;
    glm::vec3 col_offset = glm::vec3(0.0f);
    std::map<std::string, std::string> params;
    std::map<std::string, Transform> locations;
};

class Model : public Asset
{
public:
    Model(ModelDescriptor desc);

    static std::shared_ptr<Model> Load(const std::string& name);
    static std::shared_ptr<Model> LoadFromFile(const std::string& filename);
    
    const ModelVertexData& GetVertices() const { return vertices_; }
    const std::vector<gfx::MeshTriangle>& GetTriangles() const { return tris_; }
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
    ModelVertexData vertices_;
    std::vector<gfx::MeshTriangle> tris_;
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