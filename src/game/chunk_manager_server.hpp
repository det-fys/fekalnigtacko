#pragma once

#include "mapgen/chunk_manager.hpp"

namespace game
{

class World;

struct ServerChunkData
{
    glm::ivec2 coord{0};
    std::shared_ptr<const collision::TriangleMesh> col_mesh;
    std::unique_ptr<btRigidBody> col_body;
};

class ServerChunkManager : public mg::ChunkManager
{
public:
    ServerChunkManager(std::shared_ptr<const mg::ResourceSet> res, const mg::MapConfig& map_cfg, World& world);
    DELETE_COPY_MOVE(ServerChunkManager)

    ~ServerChunkManager() override;

protected:
    virtual void GetChunkParams(mg::ChunkParams& out_params) override;

    virtual void OnChunkGenerated(const mg::ChunkGenData& gen_data) override;
    virtual void OnChunkRemoved(const glm::ivec2& coord) override;

private:
    void CreateChunkCollision(ServerChunkData& chunk_data);
    void RemoveChunkCollision(ServerChunkData& chunk_data);

private:
    World& world_;

    std::unordered_map<glm::ivec2, ServerChunkData> chunks_;
};

} // namespace game