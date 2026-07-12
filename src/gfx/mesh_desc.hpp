#pragma once

#include <limits>
#include <array>
#include <span>

#include <glm/glm.hpp>

#include "id.hpp"

namespace gfx
{

using MeshID = ID;

using MeshVertexAttributeFlags = uint8_t;
enum MeshVertexAttributeFlag : MeshVertexAttributeFlags
{
    MESH_VERTEX_ATTR_POSITION = 1,
    MESH_VERTEX_ATTR_NORMAL = 2,
    MESH_VERTEX_ATTR_COLOR = 4,
    MESH_VERTEX_ATTR_UV0 = 8,
    MESH_VERTEX_ATTR_UV1 = 16,
    MESH_VERTEX_ATTR_BONE_DATA = 32,
};

using MeshVertexPosition = glm::vec3;
using MeshVertexNormal = glm::vec3;
using MeshVertexColor = uint32_t;
using MeshVertexUv = glm::vec2;

constexpr size_t MAX_VERTEX_BONE_INFLUENCES = 4;

using BoneIndex = uint8_t;
constexpr BoneIndex NO_BONE = std::numeric_limits<BoneIndex>().max();

struct MeshVertexBoneData
{
    std::array<BoneIndex, MAX_VERTEX_BONE_INFLUENCES> bone_indices;
    std::array<float, MAX_VERTEX_BONE_INFLUENCES> bone_weights;
};

struct MeshVertexData
{
    std::span<const MeshVertexPosition> position;
    std::span<const MeshVertexNormal> normal;
    std::span<const MeshVertexColor> color;
    std::span<const MeshVertexUv> uv0;
    std::span<const MeshVertexUv> uv1;
    std::span<const MeshVertexBoneData> bone;
    uint32_t count = 0;
};

using MeshIndex = uint32_t;

struct MeshTriangle
{
    std::array<MeshIndex, 3> vertices;

    MeshTriangle() = default;

    MeshTriangle(MeshIndex v0, MeshIndex v1, MeshIndex v2)
    {
        vertices[0] = v0;
        vertices[1] = v1;
        vertices[2] = v2;
    }
};

struct MeshTriangleData
{
    std::span<const MeshTriangle> triangles;
};

struct MeshDescriptor
{
    MeshVertexAttributeFlags attributes = 0;
    bool use_index_buffer = false;
    bool dynamic = false;
};

}