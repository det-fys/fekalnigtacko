#pragma once

#include <string>
#include <memory>

#include "utils/defs.hpp"

#include "collision/trianglemesh.hpp"

#ifdef CLIENT
#include "mesh_builder.hpp"
#endif

namespace assets
{

enum ModelCollisionShapeType
{
    MCS_NONE,
    
    MCS_BOX,
    MCS_SPHERE,
};

struct ModelCollisionShape
{
    ModelCollisionShapeType type = MCS_NONE;
    glm::vec3 origin = glm::vec3(0.0f);
    union
    {
        float radius;
        glm::vec3 half_extents;
    };
};

class Model
{
public:
    Model() = default;
    static std::shared_ptr<const Model> LoadFromFile(const std::string& filename);

    const collision::TriangleMesh* GetColMesh() const { return cmesh_.get(); }
    const std::vector<ModelCollisionShape>& GetColShapes() const { return cshapes_; }

    CLIENT_ONLY(const std::shared_ptr<const Mesh>& GetMesh() const { return mesh_; })

private:
    std::unique_ptr<collision::TriangleMesh> cmesh_;
    std::vector<ModelCollisionShape> cshapes_;

    CLIENT_ONLY(std::shared_ptr<const Mesh> mesh_;)

};

}