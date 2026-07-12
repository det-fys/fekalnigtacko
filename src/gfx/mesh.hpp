#pragma once

#include "renderer.hpp"
#include "utils/defs.hpp"

namespace gfx
{

class Mesh
{
public:
    Mesh(const MeshDescriptor& desc);
    DELETE_COPY_MOVE(Mesh);

    void SetVertexData(const MeshVertexData& data);
    void SetTriangleData(const MeshTriangleData& data);

    MeshID GetID() const { return id_; }

    ~Mesh();

private:
    MeshID id_;
};

} // namespace gfx
