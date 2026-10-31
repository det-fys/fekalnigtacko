#pragma once

#include <map>
#include <unordered_set>

#include <glm/gtx/hash.hpp>

#include "map_layout.hpp"
#include "path_gen.hpp"
#include "chunk.hpp"
#include "utils/spline.hpp"

namespace mg
{

struct MapLayoutChunk
{
    std::vector<StaticObject> objs;
    PathSet jp{};
};

class MapLayoutChunkizer
{
public:
    MapLayoutChunkizer() = default;
    void Chunkize(const ResourceSet& res, const MapLayout& map, const PathSet& jp);

    const MapLayoutChunk* GetChunkLayout(const glm::ivec2& coord) const;

private:
    void Clear();
    void ChunkizeObjs(const ResourceSet& res, const MapLayout& map);
    void ChunkizePaths(const ResourceSet& res, const MapLayout& map, const PathSet& jp);

    std::unordered_map<glm::ivec2, MapLayoutChunk> chunks_;

    // (x, y, junction idx) -> junction idx in chunk
    std::map<std::tuple<int, int, uint32_t>, uint32_t> chunk_junctions_;

    SplinePath temp_spline_{};
    std::unordered_set<glm::ivec2> chunk_coords_;
};



}