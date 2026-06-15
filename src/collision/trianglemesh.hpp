#pragma once

#include <memory>
#include <span>
#include <vector>

#include <btBulletCollisionCommon.h>
#include <glm/glm.hpp>

#include "shape_info.hpp"
#include "utils/defs.hpp"

namespace collision
{

class TriangleMesh
{
public:
    TriangleMesh();
    DELETE_COPY_MOVE(TriangleMesh)

    void BeginMaterial(Material material);
    void AddTriangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);
    void Build();

    btBvhTriangleMeshShape* GetShape() const { return bt_shape_.get(); }

private:
    Material current_material_ = PM_NONE;
    btTriangleMesh bt_mesh_;
    std::unique_ptr<btBvhTriangleMeshShape> bt_shape_;
    std::vector<Material> tri_materials_;
    ShapeInfo shape_info_;
};

} // namespace collision