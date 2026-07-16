#include "vertex_pack.hpp"

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

gfx::VertexPackOffsets gfx::GetVertexPackOffsets(MeshVertexAttributeFlags attrs)
{
    VertexPackOffsets o{};

    if (attrs & MESH_VERTEX_ATTR_POSITION)
    {
        o.offset_pos = o.stride;
        o.stride += 3 * sizeof(float);
    }

    if (attrs & MESH_VERTEX_ATTR_NORMAL)
    {
        o.offset_normal = o.stride;
        o.stride += 3 * sizeof(float);
    }

    if (attrs & MESH_VERTEX_ATTR_COLOR)
    {
        o.offset_color = o.stride;
        o.stride += 1 * sizeof(uint32_t);
    }

    if (attrs & MESH_VERTEX_ATTR_UV0)
    {
        o.offset_uv0 = o.stride;
        o.stride += 2 * sizeof(float);
    }

    if (attrs & MESH_VERTEX_ATTR_UV1)
    {
        o.offset_uv1 = o.stride;
        o.stride += 2 * sizeof(float);
    }

    if (attrs & MESH_VERTEX_ATTR_BONE_DATA)
    {
        o.offset_bone = o.stride;
        o.stride += 4 * sizeof(uint8_t); // indices
        o.stride += 4 * sizeof(float);   // weights
    }

    return o;
}

gfx::VertexPackOffsets gfx::PackVertexData(const MeshDescriptor& desc, const MeshVertexData& vertex_data,
                                           std::vector<uint8_t>& dst)
{
    auto o = GetVertexPackOffsets(desc.attributes);

    dst.resize(o.stride * vertex_data.count);

    // PACK
    if (desc.attributes & MESH_VERTEX_ATTR_POSITION)
    {
        PackVertexAttrsArray<MeshVertexPosition>(vertex_data.position, vertex_data.count, dst.data() + o.offset_pos,
                                                 o.stride);
    }

    if (desc.attributes & MESH_VERTEX_ATTR_NORMAL)
    {
        PackVertexAttrsArray<MeshVertexNormal>(vertex_data.normal, vertex_data.count, dst.data() + o.offset_normal,
                                               o.stride);
    }

    if (desc.attributes & MESH_VERTEX_ATTR_COLOR)
    {
        PackVertexAttrsArray<MeshVertexColor>(vertex_data.color, vertex_data.count, dst.data() + o.offset_color,
                                              o.stride);
    }

    if (desc.attributes & MESH_VERTEX_ATTR_UV0)
    {
        PackVertexAttrsArray<MeshVertexUv>(vertex_data.uv0, vertex_data.count, dst.data() + o.offset_uv0, o.stride);
    }

    if (desc.attributes & MESH_VERTEX_ATTR_UV1)
    {
        PackVertexAttrsArray<MeshVertexUv>(vertex_data.uv1, vertex_data.count, dst.data() + o.offset_uv1, o.stride);
    }

    if (desc.attributes & MESH_VERTEX_ATTR_BONE_DATA)
    {
        PackVertexAttrsArray<MeshVertexBoneData>(vertex_data.bone, vertex_data.count, dst.data() + o.offset_bone,
                                                 o.stride);
    }

    return o;
}
