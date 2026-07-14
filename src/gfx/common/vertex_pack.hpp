#pragma once

#include "../mesh_desc.hpp"
#include <vector>

namespace gfx
{

struct VertexPackOffsets
{
    size_t stride = 0;
    size_t offset_pos = 0;
    size_t offset_normal = 0;
    size_t offset_color = 0;
    size_t offset_uv0 = 0;
    size_t offset_uv1 = 0;
    size_t offset_bone = 0;
};

VertexPackOffsets GetVertexPackOffsets(MeshVertexAttributeFlags attrs);

VertexPackOffsets PackVertexData(const MeshDescriptor& desc, const MeshVertexData& vertex_data,
                                 std::vector<uint8_t>& dst);

}
