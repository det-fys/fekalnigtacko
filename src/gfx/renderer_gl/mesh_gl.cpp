#include "mesh_gl.hpp"

#include "../common/vertex_pack.hpp"

static int GetVertexAttrs(const gfx::MeshDescriptor& desc)
{
    int va_attrs = 0;
    if (desc.attributes & gfx::MESH_VERTEX_ATTR_POSITION)
        va_attrs |= gfx::VA_POSITION;

    if (desc.attributes & gfx::MESH_VERTEX_ATTR_NORMAL)
        va_attrs |= gfx::VA_NORMAL;

    if (desc.attributes & gfx::MESH_VERTEX_ATTR_COLOR)
        va_attrs |= gfx::VA_COLOR;

    if (desc.attributes & gfx::MESH_VERTEX_ATTR_UV0)
        va_attrs |= gfx::VA_UV;

    if (desc.attributes & gfx::MESH_VERTEX_ATTR_UV1)
        va_attrs |= gfx::VA_LIGHTMAP_UV;

    if (desc.attributes & gfx::MESH_VERTEX_ATTR_BONE_DATA)
        va_attrs |= gfx::VA_BONE_INDICES | gfx::VA_BONE_WEIGHTS;

    return va_attrs;
}

static int GetVAFlags(const gfx::MeshDescriptor& desc)
{
    int flags = 0;

    if (desc.dynamic)
        flags |= gfx::VF_DYNAMIC;

    if (desc.use_index_buffer)
        flags |= gfx::VF_CREATE_EBO;

    return flags;
}

gfx::MeshGL::MeshGL(const MeshDescriptor& desc) : desc_(desc), va_(GetVertexAttrs(desc), GetVAFlags(desc))
{
    //
}

void gfx::MeshGL::SetVertexData(const MeshVertexData& data)
{
    static std::vector<uint8_t> buffer;
    PackVertexData(desc_, data, buffer);
    va_.SetVBOData(buffer.data(), buffer.size());
}

void gfx::MeshGL::SetTriangleData(const MeshTriangleData& data)
{
    std::span<const GLuint> data_uint{reinterpret_cast<const GLuint*>(data.triangles.data()),
                                      data.triangles.size() * 3};

    va_.SetIndices(data_uint.data(), data_uint.size());
}
