#include "mesh.hpp"

#include <stdexcept>

gfx::Mesh::Mesh(const MeshDescriptor& desc)
{
    id_ = Renderer::GetInstance().CreateMesh(desc);
}

void gfx::Mesh::SetVertexData(const MeshVertexData& data)
{
    Renderer::GetInstance().SetMeshVertexData(id_, data);
}

void gfx::Mesh::SetTriangleData(const MeshTriangleData& data)
{
    Renderer::GetInstance().SetMeshTriangleData(id_, data);
}

gfx::Mesh::~Mesh()
{
    Renderer::GetInstance().ReleaseMesh(id_);
}
