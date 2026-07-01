#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <sstream>

#include "game/transform_node.hpp"
#include "model.hpp"
#include "utils/aabb.hpp"

namespace assets
{

struct MapStaticObject
{
    game::TransformNode node;
    AABB3 aabb;
    size_t model_idx = 0;
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
    size_t first_obj = 0;
    size_t num_objs = 0;
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

struct MapLocation
{
    Transform transform;
};

class Map : public Asset
{
public:
    Map() = default;
    static std::shared_ptr<Map> Load(const std::string& name);
    static std::shared_ptr<Map> LoadFromFile(const std::string& filename);

    const std::shared_ptr<const Model>& GetBaseModel() const { return basemodel_; }
    const std::vector<std::shared_ptr<const Model>>& GetObjModels() const { return obj_models_; }
    const std::vector<Chunk>& GetChunks() const { return chunks_; }
    const std::vector<MapStaticObject>& GetStaticObjects() const { return objs_; }
    const MapGraph* GetGraph(const std::string& name) const;
    std::span<const MapLocation> GetLocations(const std::string& name) const;

private:
    std::shared_ptr<const Model> basemodel_;
    std::vector<std::shared_ptr<const Model>> obj_models_;
    std::vector<Chunk> chunks_;
    std::vector<MapStaticObject> objs_;
    std::map<std::string, MapGraph> graphs_;
    std::map<std::string, std::vector<MapLocation>> locations_;

    friend class MapLoader;
};

enum MapLoadingState
{
    ML_INIT,
    ML_READ_MODELS,
    ML_LOAD_BASEMODEL,
    ML_LOAD_MODELS,
    ML_STRUCTS,
    ML_FINISHED,
};

class MapLoader
{
public:
    MapLoader(const std::string& filename);

    bool Next();
    int GetPercent() const;

    std::shared_ptr<Map> GetMap() const;

private:
    void ReadModels();
    void LoadBaseModel();
    bool LoadNextModel();
    void LoadStructs();
    
private:
    std::istringstream map_iss_;

    MapLoadingState state_ = ML_INIT;

    std::string basemodel_name_;
    std::vector<std::string> model_names_;

    std::shared_ptr<Map> map_;
};

} // namespace assets