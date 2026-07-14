#pragma once

#include "vertex_array.hpp"
#include "../mesh_desc.hpp"

namespace gfx
{

class MeshGL
{
public:
    MeshGL(const MeshDescriptor& desc);

    void SetVertexData(const MeshVertexData& data);
    void SetTriangleData(const MeshTriangleData& data);

    MeshVertexAttributeFlags GetAttrs() const { return desc_.attributes; }
    GLuint GetVaoId() const { return va_.GetVAOId(); }

private:
    MeshDescriptor desc_;
    VertexArray va_;


};



}