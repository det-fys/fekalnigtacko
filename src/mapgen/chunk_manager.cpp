#include "chunk_manager.hpp"
#include <glm/gtx/norm.hpp>

mg::ChunkManager::ChunkManager(std::shared_ptr<const ResourceSet> res, const MapConfig& map_cfg)
    : res_(std::move(res)), map_cfg_(map_cfg)
{
}

void mg::ChunkManager::BeginFrame(int64_t time)
{
    time_ = time;
}

void mg::ChunkManager::RequestChunk(const glm::ivec2& coord, uint32_t lod, float priority)
{
    if (lod == LOD_INVALID)
        return;

    auto& chunk_data = chunks_[coord];
    
    if (chunk_data.request_time == time_)
    {
        // also just requested this frame - max the lod
        chunk_data.request_lod = glm::max(chunk_data.request_lod, lod);
        chunk_data.request_priority = glm::max(chunk_data.request_priority, priority);
    }
    else
    {
        // not requested this frame yet - set the lod
        chunk_data.request_time = time_;
        chunk_data.request_lod = lod;
        chunk_data.request_priority = priority;
    }
}

void mg::ChunkManager::InvalidateChunk(const glm::ivec2& coord)
{
    auto it = chunks_.find(coord);
    if (it == chunks_.end())
        return;

    auto& chunk_data = it->second;
    chunk_data.valid_lod = LOD_INVALID;
}

void mg::ChunkManager::InvalidateAllChunks()
{
    for (auto& pair : chunks_)
    {
        pair.second.valid_lod = LOD_INVALID;
    }
}

void mg::ChunkManager::RequestArea(const glm::vec2& center, std::span<const float> lod_distances)
{
    // |     LOD0     |     LOD1     |     LOD2     |     LOD3     |
    //             dist[0]        dist[1]        dist[2]        dist[3]

    auto max_dist = lod_distances.back();

    auto [min_chunk, max_chunk] =
        mg::GetChunkRange(map_cfg_, AABB2(center - glm::vec2(max_dist), center + glm::vec2(max_dist)));


    for (int chunk_y = min_chunk.y; chunk_y <= max_chunk.y; ++chunk_y)
    {
        for (int chunk_x = min_chunk.x; chunk_x <= max_chunk.x; ++chunk_x)
        {
            glm::ivec2 chunk_coord(chunk_x, chunk_y);
            auto chunk_center = (glm::vec2(chunk_coord) + 0.5f) * map_cfg_.chunk_size_m;

            auto dist2 = glm::distance2(center, chunk_center);

            // find lod for this distance
            uint32_t lod = LOD_INVALID;
            for (uint32_t i = lod_distances.size(); i-- > 0;)
            {
                auto lod_dist = lod_distances[i];
                if (dist2 > lod_dist * lod_dist)
                    break;
                
                lod = i;
            }

            if (lod == LOD_INVALID)
                continue;

            RequestChunk(chunk_coord, lod, -dist2);
        }
    }
}

void mg::ChunkManager::EndFrame()
{
    CheckGeneratedChunk();
    UpdateChunks();
}

mg::ChunkState mg::ChunkManager::GetChunkState(const glm::ivec2& coord) const
{
    auto it = chunks_.find(coord);
    if (it == chunks_.end())
        return CHUNK_STATE_NONE;

    const auto& chunk_data = it->second; 

    if (chunk_data.in_progress)
        return CHUNK_STATE_IN_PROGRESS;

    if (chunk_data.valid_lod == chunk_data.request_lod)
        return CHUNK_STATE_READY;

    return CHUNK_STATE_REQUESTED;
}

void mg::ChunkManager::UpdateChunks()
{
    glm::ivec2 update_coord{0};
    uint32_t update_lod = LOD_INVALID;
    float update_priority = std::numeric_limits<float>::lowest();

    for (auto it = chunks_.begin(); it != chunks_.end();)
    {
        auto& chunk_data = it->second;

        // check timeout
        bool to_remove = time_ - chunk_data.request_time > timeout_;
        if (to_remove)
        {
            OnChunkRemoved(it->first);
            it = chunks_.erase(it);
            continue;
        }

        // check if chunk needs update
        if (chunk_data.request_lod != chunk_data.valid_lod && chunk_data.request_priority > update_priority)
        {
            update_coord = it->first;
            update_lod = chunk_data.request_lod;
            update_priority = chunk_data.request_priority;
        }

        ++it;
    }

    if (update_lod != LOD_INVALID)
    {
        ScheduleChunkUpdate(update_coord, update_lod);
    }

}

void mg::ChunkManager::ScheduleChunkUpdate(const glm::ivec2& coord, uint32_t lod)
{
    if (future_chunk_.valid())
        return; // already generating a chunk

    auto& chunk_data = chunks_[coord];
    chunk_data.valid_lod = lod;
    chunk_data.in_progress = true;

    mg::ChunkParams params{};
    params.coord = coord;
    params.lod = lod;
    params.heightmap_seed = map_cfg_.seed;
    GetChunkParams(params);

    auto func = [res = res_, map_cfg = map_cfg_, params = std::move(params)]() mutable {
        auto mg_chunk = mg::GenerateChunk(*res, map_cfg, params);
        auto gen_data = FinalizeChunk(*res, std::move(mg_chunk));
        return gen_data;
    };

    future_chunk_ = worker_.Schedule(std::move(func));
}

mg::ChunkGenData mg::ChunkManager::FinalizeChunk(const mg::ResourceSet& res, mg::Chunk&& chunk)
{
    ChunkGenData gen_data{};
    gen_data.coord = chunk.coord;
    // gen_data.lod = chunk.mgchunk.lod;
    gen_data.aabb = chunk.aabb;

    // make chunk model
    assets::ModelDescriptor model_desc{};
    // model_desc.make_triangle_mesh = true;

    for (const auto& vert : chunk.mesh.verts)
    {
        model_desc.verts.positions.push_back(vert.pos);
        model_desc.verts.normals.push_back(vert.normal);
        model_desc.verts.uvs.push_back(vert.uv);
        model_desc.verts.colors.push_back(vert.color);
    }

    for (const auto& tri : chunk.mesh.tris)
    {
        model_desc.tris.emplace_back(tri[0], tri[1], tri[2]);
    }

    for (const auto& surface : chunk.mesh.surfaces)
    {
        const auto& mg_material = res.GetMaterials().GetByIndex(surface.material_id);
        
        auto& model_surface = model_desc.surfaces.emplace_back();
        model_surface.name = mg_material.name;
        model_surface.tri_offset = surface.tri_offset;
        model_surface.tri_count = surface.tri_count;
        model_surface.material = mg_material;
    }

    gen_data.model = std::make_shared<assets::Model>(std::move(model_desc));

    // append chunk objs
    for (const auto& mgobj : chunk.objs)
    {
        auto& obj = gen_data.objs.emplace_back();
        obj.trans = mgobj.trans;
        obj.model = res.GetModels().GetByIndex(mgobj.model_id).model;        
        obj.aabb = TransformAABB(obj.model->GetAABB(), obj.trans);
    }

    return gen_data;
}

void mg::ChunkManager::CheckGeneratedChunk()
{
    if (!future_chunk_.valid())
        return;

    if (future_chunk_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;

    auto gen_data = future_chunk_.get();

    auto& chunk_data = chunks_[gen_data.coord];
    chunk_data.gen = std::move(gen_data);
    chunk_data.in_progress = false;
    
    OnChunkGenerated(chunk_data.gen);
}

