#pragma once

#include <string>
#include <span>
#include <vector>
#include <tuple>

#include "utils/aabb.hpp"
#include "utils/transform.hpp"
#include "map_config.hpp"
#include "defs.hpp"
#include "heightmesh.hpp"
#include "resources.hpp"
#include "map_layout.hpp"

namespace mg
{

struct ChunkMeshVertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    uint32_t color = 0xFFFFFFFF;
};

struct ChunkMeshSurface
{
    uint32_t tri_offset = 0;
    uint32_t tri_count = 0;
    uint32_t material_id;
};

struct ChunkMesh
{
    std::vector<ChunkMeshVertex> verts;
    std::vector<Triangle> tris;
    std::vector<ChunkMeshSurface> surfaces;
};

using ChunkTileFlags = uint8_t;
enum ChunkTileFlag : ChunkTileFlags
{
    CHUNK_TILE_OBSTRUCTED = 1,
    CHUNK_TILE_OBSTRUCTED_MARGIN = 2,
};

struct ChunkTile
{
    float height = 0.0f;
    uint8_t obstruction_level = 0;
    uint8_t forest_level = 0;
    ChunkTileFlags flags = 0;
};

struct ChunkParams
{
    MapLayout map{};
    glm::ivec2 coord{0};
    uint32_t lod = 0;
    bool collision_only = false;
};

struct ChunkOutputObject
{
    FarMatrix trans{1.0f};
    uint32_t model_id = 0;
    uint32_t color = 0xFFFFFFFF;
};

struct Chunk
{
    glm::ivec2 coord{0};
    glm::dvec3 center{0.0};
    uint32_t lod = 0;
    ChunkMesh mesh{};
    std::vector<ChunkOutputObject> objs;
    AABB3 aabb;
};

AABBD2 GetChunkAABB(const MapConfig& cfg, const glm::ivec2& chunk_pos, bool include_border = false);
std::tuple<glm::ivec2, glm::ivec2> GetChunkRange(const MapConfig& cfg, const AABBD2& aabb, bool include_margin = false);

Chunk GenerateChunk(const ResourceSet& res, const ChunkParams& params);

}

