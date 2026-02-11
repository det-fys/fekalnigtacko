#pragma once

#include "gfx/surface.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>
#include <map>

namespace assets
{

struct MeshVertexBoneInfluence
{
    int bone_index = -1;
    float weight = 0.0f;
};

struct MeshVertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec2 lightmap_uv;
    MeshVertexBoneInfluence bones[4];
};

struct MeshTriangle
{
    uint32_t vert[3];
};

struct Mesh
{
    gfx::MeshFlags mflags = gfx::MF_NONE;
    std::vector<gfx::Surface> surfaces;
    std::map<std::string, size_t> surface_names;
};

class MeshBuilder
{
public:
    MeshBuilder(gfx::MeshFlags mflags);

    void BeginSurface(gfx::SurfaceFlags sflags, const std::string& name, std::shared_ptr<const gfx::Texture> texture);
    void AddVertex(const MeshVertex& v);
    void AddTriangle(const MeshTriangle& t);
    
    void Build();

    void SetMeshFlag(gfx::MeshFlags flag) { mflags_ |= flag; }

    std::shared_ptr<const Mesh> GetMesh() const { return mesh_; }
    
private:
    void FinalizeSurface();

private:
    gfx::MeshFlags mflags_ = 0;
    std::vector<MeshVertex> verts_;
    std::vector<MeshTriangle> tris_;
    std::shared_ptr<Mesh> mesh_;
};

} // namespace assets