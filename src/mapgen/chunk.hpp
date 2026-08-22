#pragma once

#include <string>
#include <span>
#include <vector>
#include <tuple>

#include "utils/aabb.hpp"
#include "map_config.hpp"
#include "defs.hpp"
#include "heightmesh.hpp"


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
    float height_weight = 0.0f;
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

struct ChunkParams
{
    glm::ivec2 coord;
    std::span<ChunkStaticObject> objs;
};

struct Chunk
{
    ChunkMesh mesh{};
    std::vector<ChunkStaticObject> objs;
};

AABB2 GetChunkAABB(const MapConfig& cfg, const glm::ivec2& chunk_pos, bool include_border = false);
std::tuple<glm::ivec2, glm::ivec2> GetChunkRange(const MapConfig& cfg, const AABB2& aabb);

Chunk GenerateChunk(const MapConfig& cfg, const ChunkParams& params);

}

