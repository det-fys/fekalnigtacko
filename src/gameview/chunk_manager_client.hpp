#pragma once

#include "mapgen/chunk_manager.hpp"
#include "gfx/scene.hpp"
#include "modelview.hpp"

namespace game::view
{

struct ChunkObjView
{
    FarMatrix matrix{};
    AABB3 aabb{};
    std::shared_ptr<const ModelView> model;
    glm::vec4 color{1.0f};
};

struct ChunkView
{
    AABB3 aabb{};
    FarMatrix matrix{};
    std::shared_ptr<const ModelView> model;
    std::vector<ChunkObjView> objs;
};

class ClientChunkManager : public mg::ChunkManager
{
public:
    ClientChunkManager(std::shared_ptr<const mg::ResourceSet> res, const mg::MapConfig& map_cfg);

    void Draw(const gfx::DrawContext& ctx) const;

protected:
    virtual void GetChunkParams(mg::ChunkParams& out_params) override {}

    virtual void OnChunkGenerated(const mg::ChunkGenData& gen_data) override;
    virtual void OnChunkRemoved(const glm::ivec2& coord) override;

private:
    std::unordered_map<glm::ivec2, ChunkView> chunks_;
};


}