#pragma once

#include "mapgen/chunk_manager.hpp"
#include "gfx/scene.hpp"
#include "modelview.hpp"

namespace game::view
{

struct ChunkObjView
{
    glm::mat4 trans{1.0f};
    AABB3 aabb{};
    std::shared_ptr<const ModelView> model;
};

struct ChunkView
{
    AABB3 aabb{};
    std::shared_ptr<const ModelView> model;
    std::vector<ChunkObjView> objs;
};

class ClientChunkManager : public mg::ChunkManager
{
public:
    ClientChunkManager(std::shared_ptr<const mg::ResourceSet> res, const mg::MapConfig& map_cfg);

    void Draw(const gfx::DrawContext& ctx) const;

protected:
    virtual void OnChunkGenerated(const mg::ChunkGenData& gen_data) override;
    virtual void OnChunkRemoved(const glm::ivec2& coord) override;

private:
    std::unordered_map<glm::ivec2, ChunkView> chunks_;
};


}