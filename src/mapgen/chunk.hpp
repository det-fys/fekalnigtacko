#pragma once

#include <string>
#include <span>
#include <vector>
#include <tuple>

#include "utils/aabb.hpp"
#include "map_config.hpp"
#include "defs.hpp"
#include "heightmesh.hpp"
#include "resources.hpp"

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
    std::string material;
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
    ChunkTileFlags flags = 0;
};

struct ChunkStaticObject
{
    glm::mat4 trans{1.0f};
    //AABB3 aabb{};
    std::string model_name;
    std::shared_ptr<const HeightMesh> heightmesh;
};

enum ChunkSplineNodeType : uint8_t
{
    CHUNK_SPLINE_NODE_NORMAL,
    CHUNK_SPLINE_NODE_JUNCTION,
};

struct ChunkSplineNode
{
    glm::vec3 pos{0.0f};
    std::array<uint32_t, 4> links = { 0, 0, 0, 0 };
    ChunkSplineNodeType type = CHUNK_SPLINE_NODE_NORMAL;
    uint32_t res_id = 0; // mesh id for CHUNK_SPLINE_NODE_MESH, might be used for something else for other types
};

struct ChunkParams
{
    glm::ivec2 coord;
    std::vector<ChunkStaticObject> objs;
    uint32_t heightmap_seed = 420;
    std::map<uint32_t, ChunkSplineNode> nodes;
};

struct Chunk
{
    glm::ivec2 coord;
    ChunkMesh mesh{};
    std::vector<ChunkStaticObject> objs;
    AABB3 aabb;
};

AABB2 GetChunkAABB(const MapConfig& cfg, const glm::ivec2& chunk_pos, bool include_border = false);
std::tuple<glm::ivec2, glm::ivec2> GetChunkRange(const MapConfig& cfg, const AABB2& aabb, bool include_margin = false);

Chunk GenerateChunk(const ResourceSet& res, const MapConfig& cfg, const ChunkParams& params);

}

