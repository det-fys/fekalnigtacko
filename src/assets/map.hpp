#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "game/transform_node.hpp"
#include "model.hpp"
#include "utils/aabb.hpp"

#ifdef CLIENT
#include "gameview/draw_args.hpp"
#endif // CLIENT

namespace assets
{

struct ChunkStaticObject
{
    game::TransformNode node;
    AABB3 aabb;
    std::shared_ptr<const Model> model;
    glm::vec3 color = glm::vec3(1.0f);
};

struct ChunkSurfaceRange
{
    size_t idx = 0;
    size_t first = 0;
    size_t count = 0;

    ChunkSurfaceRange(size_t idx, size_t first, size_t count) : idx(idx), first(first), count(count) {}
};

struct Chunk
{
    AABB3 aabb;
    std::vector<ChunkSurfaceRange> surfaces;
    std::vector<ChunkStaticObject> objs;
};

struct MapGraphNode
{
    glm::vec3 position = glm::vec3(0.0f);
    size_t num_nbs = 0;
    size_t nbs = 0;
};

struct MapGraph
{
    std::vector<MapGraphNode> nodes;
    std::vector<size_t> nbs;
};

class Map
{
public:
    Map() = default;
    static std::shared_ptr<const Map> LoadFromFile(const std::string& filename);

    const std::shared_ptr<const Model>& GetBaseModel() const { return basemodel_; }
    const std::vector<Chunk>& GetChunks() const { return chunks_; }
    const MapGraph* GetGraph(const std::string& name) const;

    CLIENT_ONLY(void Draw(const game::view::DrawArgs& args) const;)

private:
    CLIENT_ONLY(void DrawChunk(const game::view::DrawArgs& args, const Mesh& basemesh, const Chunk& chunk) const;)

private:
    std::shared_ptr<const Model> basemodel_;
    std::vector<Chunk> chunks_;
    std::map<std::string, MapGraph> graphs_;
};

} // namespace assets