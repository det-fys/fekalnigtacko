#include "chunk_manager_client.hpp"

game::view::ClientChunkManager::ClientChunkManager(std::shared_ptr<const mg::ResourceSet> res, const mg::MapConfig& map_cfg)
    : mg::ChunkManager(std::move(res), map_cfg)
{
}

void game::view::ClientChunkManager::Draw(const gfx::DrawContext& ctx) const
{
    static glm::mat4 identity{1.0f};

    for (const auto& [coord, chunk] : chunks_)
    {
        if (!ctx.frustum.IsAABBVisible(chunk.aabb))
            continue;

        if (ctx.pass == gfx::DRAW_PASS_MAIN && chunk.model)
            chunk.model->Draw(ctx, identity, {});

        for (const auto& obj : chunk.objs)
        {
            if (!ctx.frustum.IsAABBVisible(obj.aabb))
                continue;

            obj.model->Draw(ctx, obj.trans, {});
        }
    }
}

void game::view::ClientChunkManager::OnChunkGenerated(const mg::ChunkGenData& gen_data)
{
    ChunkView& chunk = chunks_[gen_data.coord];
    chunk.aabb = gen_data.aabb;
    chunk.model.reset();
    chunk.model = std::make_shared<ModelView>(gen_data.model);

    chunk.objs.clear();
    chunk.objs.reserve(gen_data.objs.size());
    for (const auto& obj : gen_data.objs)
    {
        ChunkObjView obj_view{};
        obj_view.trans = obj.trans;
        obj_view.aabb = obj.aabb;
        obj_view.model = std::make_shared<ModelView>(obj.model);

        chunk.objs.push_back(std::move(obj_view));
    }
}

void game::view::ClientChunkManager::OnChunkRemoved(const glm::ivec2& coord)
{
    auto it = chunks_.find(coord);
    if (it != chunks_.end())
        chunks_.erase(it);
}
