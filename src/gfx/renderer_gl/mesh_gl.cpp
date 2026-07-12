#include "mesh_gl.hpp"

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

gfx::GLMesh::GLMesh(const MeshDescriptor& desc) : desc_(desc), va_(GetVertexAttrs(desc), GetVAFlags(desc))
{
    //
}

template <typename T>
static void PackVertexAttrsArray(std::span<const T> data, size_t max_count, uint8_t* dst, size_t stride)
{
    // Safety check: ensure we actually have enough data to pack
    assert(data.size() >= max_count);

    for (size_t i = 0; i < max_count; ++i)
    {
        // std::memcpy handles unaligned memory safely and optimizes beautifully
        std::memcpy(dst + (i * stride), &data[i], sizeof(T));
    }
}

void gfx::GLMesh::SetVertexData(const MeshVertexData& data)
{
    size_t stride = 0;
    size_t offset_pos = 0;
    size_t offset_normal = 0;
    size_t offset_color = 0;
    size_t offset_uv0 = 0;
    size_t offset_uv1 = 0;
    size_t offset_bone = 0;

    if (desc_.attributes & MESH_VERTEX_ATTR_POSITION)
    {
        offset_pos = stride;
        stride += 3 * sizeof(float);
    }

    if (desc_.attributes & MESH_VERTEX_ATTR_NORMAL)
    {
        offset_normal = stride;
        stride += 3 * sizeof(float);
    }

    if (desc_.attributes & MESH_VERTEX_ATTR_COLOR)
    {
        offset_color = stride;
        stride += 1 * sizeof(uint32_t);
    }

    if (desc_.attributes & MESH_VERTEX_ATTR_UV0)
    {
        offset_uv0 = stride;
        stride += 2 * sizeof(float);
    }

    if (desc_.attributes & MESH_VERTEX_ATTR_UV1)
    {
        offset_uv1 = stride;
        stride += 2 * sizeof(float);
    }

    if (desc_.attributes & MESH_VERTEX_ATTR_BONE_DATA)
    {
        offset_bone = stride;
        stride += 4 * sizeof(uint8_t); // indices
        stride += 4 * sizeof(float);   // weights
    }

    static std::vector<uint8_t> buffer;
    buffer.resize(stride * data.count);

    // PACK
    if (desc_.attributes & MESH_VERTEX_ATTR_POSITION)
    {
        PackVertexAttrsArray<MeshVertexPosition>(data.position, data.count, buffer.data() + offset_pos, stride);
    }

    if (desc_.attributes & MESH_VERTEX_ATTR_NORMAL)
    {
        PackVertexAttrsArray<MeshVertexNormal>(data.normal, data.count, buffer.data() + offset_normal, stride);
    }

    if (desc_.attributes & MESH_VERTEX_ATTR_COLOR)
    {
        PackVertexAttrsArray<MeshVertexColor>(data.color, data.count, buffer.data() + offset_color, stride);
    }

    if (desc_.attributes & MESH_VERTEX_ATTR_UV0)
    {
        PackVertexAttrsArray<MeshVertexUv>(data.uv0, data.count, buffer.data() + offset_uv0, stride);
    }

    if (desc_.attributes & MESH_VERTEX_ATTR_UV1)
    {
        PackVertexAttrsArray<MeshVertexUv>(data.uv1, data.count, buffer.data() + offset_uv1, stride);
    }

    if (desc_.attributes & MESH_VERTEX_ATTR_BONE_DATA)
    {
        PackVertexAttrsArray<MeshVertexBoneData>(data.bone, data.count, buffer.data() + offset_bone, stride);
    }

    va_.SetVBOData(buffer.data(), buffer.size());
}

void gfx::GLMesh::SetTriangleData(const MeshTriangleData& data)
{
    std::span<const GLuint> data_uint{reinterpret_cast<const GLuint*>(data.triangles.data()),
                                      data.triangles.size() * 3};

    va_.SetIndices(data_uint.data(), data_uint.size());
}
