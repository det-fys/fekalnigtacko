#pragma once

#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include "mapgen/chunk.hpp"
#include "mapgen/resources.hpp"
#include "mapgen/map_config.hpp"
#include "utils/worker.hpp"

namespace mg
{

constexpr uint32_t LOD_INVALID = 0xFFFFFFFF;

struct ChunkObj
{
    glm::mat4 trans{1.0f};
    AABB3 aabb{};
    std::shared_ptr<const assets::Model> model;
};

struct ChunkGenData
{
    glm::ivec2 coord{0};
    uint32_t lod = LOD_INVALID;
    AABB3 aabb{};
    std::shared_ptr<const assets::Model> model;
    std::vector<ChunkObj> objs;
};

struct ChunkData
{
    int64_t request_time = 0;
    uint32_t request_lod = LOD_INVALID;
    float request_priority = 0.0f;

    uint32_t valid_lod = LOD_INVALID; // queued or generated
    bool in_progress = false;

    ChunkGenData gen{};
};

enum ChunkState
{
    CHUNK_STATE_NONE,
    CHUNK_STATE_REQUESTED,
    CHUNK_STATE_IN_PROGRESS,
    CHUNK_STATE_READY,
};

class ChunkManager
{
public:
    ChunkManager(std::shared_ptr<const ResourceSet> res, const MapConfig& map_cfg);

    void BeginFrame(int64_t time);
    void RequestChunk(const glm::ivec2& coord, uint32_t lod, float priority);
    void InvalidateChunk(const glm::ivec2& coord);
    void InvalidateAllChunks();
    void RequestArea(const glm::vec2& center, std::span<const float> lod_distances);
    void EndFrame();

    ChunkState GetChunkState(const glm::ivec2& coord) const;

    virtual ~ChunkManager() = default;

protected:
    virtual void GetChunkParams(mg::ChunkParams& out_params) = 0;

    virtual void OnChunkGenerated(const ChunkGenData& gen_data) {}
    virtual void OnChunkRemoved(const glm::ivec2& coord) {}

private:
    void UpdateChunks();
    void ScheduleChunkUpdate(const glm::ivec2& coord, uint32_t lod);
    static ChunkGenData FinalizeChunk(const mg::ResourceSet& res, mg::Chunk&& chunk);
    void CheckGeneratedChunk();

private:
    std::shared_ptr<const ResourceSet> res_;
    MapConfig map_cfg_;
    
    int64_t time_ = 0;
    int64_t timeout_ = 1000;
    WorkerThread worker_;
    std::future<ChunkGenData> future_chunk_;
    std::unordered_map<glm::ivec2, ChunkData> chunks_;
};

} // namespace mg