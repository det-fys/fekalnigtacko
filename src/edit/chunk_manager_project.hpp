#pragma once

#include "gameview/chunk_manager_client.hpp"

namespace edit
{

class Project;

struct ProjectChunkData
{
    // visualization
    std::vector<glm::vec2> vis_verts;
    std::vector<std::tuple<uint32_t, uint32_t>> vis_edges;
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> vis_tris;
};

class ProjectChunkManager : public game::view::ClientChunkManager
{
public:
    using Super = game::view::ClientChunkManager;

    ProjectChunkManager(std::shared_ptr<const mg::ResourceSet> res, const mg::MapConfig& map_cfg, Project& project);

    const std::unordered_map<glm::ivec2, ProjectChunkData>& GetChunks() const { return chunks_; }

protected:
    virtual void GetChunkParams(mg::ChunkParams& out_params) override;

    virtual void OnChunkGenerated(const mg::ChunkGenData& gen_data) override;
    virtual void OnChunkRemoved(const glm::ivec2& coord) override;

private:
    Project& project_;

    std::unordered_map<glm::ivec2, ProjectChunkData> chunks_;    

};
} // namespace edit