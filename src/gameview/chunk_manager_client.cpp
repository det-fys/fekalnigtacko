#include "chunk_manager_client.hpp"

game::view::ClientChunkManager::ClientChunkManager(std::shared_ptr<const mg::ResourceSet> res, const mg::MapConfig& map_cfg)
    : mg::ChunkManager(std::move(res), map_cfg)
{
}

void game::view::ClientChunkManager::Draw(const gfx::DrawContext& ctx) const
{
    static FarMatrix identity{1.0f};

    AABB2 frustum_aabb(ctx.frustum.GetAABB().min, ctx.frustum.GetAABB().max);

    auto [min_coord, max_coord] = mg::GetChunkRange(GetMapConfig(), frustum_aabb, false);
    auto [loaded_min, loaded_max] = GetLoadedChunkRange();

    min_coord = glm::max(min_coord, loaded_min);
    max_coord = glm::min(max_coord, loaded_max);

    for (int y = min_coord.y; y <= max_coord.y; ++y)
    {
        for (int x = min_coord.x; x <= max_coord.x; ++x)
        {
            glm::ivec2 coord(x, y);
            auto it = chunks_.find(coord);
            if (it == chunks_.end())
                continue;

            const ChunkView& chunk = it->second;

            if (!ctx.frustum.IsAABBVisible(chunk.aabb))
                continue;

            if (ctx.pass == gfx::DRAW_PASS_MAIN && chunk.model)
                chunk.model->Draw(ctx, identity, {});

            for (const auto& obj : chunk.objs)
            {
                if (!ctx.frustum.IsAABBVisible(obj.aabb))
                    continue;

                obj.model->Draw(ctx, obj.matrix, {});
            }
        }
    }

    // for (const auto& [coord, chunk] : chunks_)
    // {
    //     if (!ctx.frustum.IsAABBVisible(chunk.aabb))
    //         continue;

    //     if (ctx.pass == gfx::DRAW_PASS_MAIN && chunk.model)
    //         chunk.model->Draw(ctx, identity, {});

    //     for (const auto& obj : chunk.objs)
    //     {
    //         if (!ctx.frustum.IsAABBVisible(obj.aabb))
    //             continue;

    //         obj.model->Draw(ctx, obj.trans, {});
    //     }
    // }
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
        obj_view.matrix = obj.trans;
        obj_view.aabb = obj.aabb;
        obj_view.model = assets::AssetManager::GetInstance().Get<ModelView>(obj.model->GetAssetName());

        chunk.objs.push_back(std::move(obj_view));
    }
}

void game::view::ClientChunkManager::OnChunkRemoved(const glm::ivec2& coord)
{
    auto it = chunks_.find(coord);
    if (it != chunks_.end())
        chunks_.erase(it);
}
