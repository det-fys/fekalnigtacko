#include "map_layout_chunkizer.hpp"

#include "chunk.hpp"

void mg::MapLayoutChunkizer::Chunkize(const ResourceSet& res, const MapLayout& map, const PathSet& jp)
{
    Clear();
    ChunkizeObjs(res, map);
    ChunkizePaths(res, map, jp);
}

const mg::MapLayoutChunk* mg::MapLayoutChunkizer::GetChunkLayout(const glm::ivec2& coord) const
{
    auto it = chunks_.find(coord);
    return it == chunks_.end() ? nullptr : &it->second;
}

void mg::MapLayoutChunkizer::Clear()
{
    chunks_.clear();
    chunk_coords_.clear();
    chunk_junctions_.clear();
}

void mg::MapLayoutChunkizer::ChunkizeObjs(const ResourceSet& res, const MapLayout& map)
{
    for (const auto& obj : map.objs)
    {
        if (obj.model_id == INVALID_RES_ID)
            continue; // invalid model

        const auto& model = res.GetModels().GetByIndex(obj.model_id);
        auto obj_aabb = TransformAABB(model.model->GetAABB(), obj.trans);

        auto [min_chunk, max_chunk] = mg::GetChunkRange(map.cfg, AABBD2(obj_aabb.min, obj_aabb.max), true);
        for (int x = min_chunk.x; x <= max_chunk.x; ++x)
        {
            for (int y = min_chunk.y; y <= max_chunk.y; ++y)
            {
                glm::ivec2 chunk_coord{x, y};
                auto& chunk = chunks_[chunk_coord];
                chunk.objs.emplace_back(obj);
            }
        }
    }
}

void mg::MapLayoutChunkizer::ChunkizePaths(const ResourceSet& res, const MapLayout& map, const PathSet& jp)
{
    // mark junctions
    for (uint32_t i = 0; i < jp.junctions.size(); ++i)
    {
        const auto& junction = jp.junctions[i];

        constexpr double margin = 15.0; // TODO: calculate actual radius from mesh bounds
        auto [min_chunk, max_chunk] =
            mg::GetChunkRange(map.cfg, AABBD2(junction.pos - margin, junction.pos + margin), true);
        for (int x = min_chunk.x; x <= max_chunk.x; ++x)
        {
            for (int y = min_chunk.y; y <= max_chunk.y; ++y)
            {
                chunk_junctions_[{x, y, i}] = INVALID_ID;
            }
        }
    }

    // traverse paths
    for (uint32_t i = 0; i < jp.paths.size(); ++i)
    {
        const auto& path = jp.paths[i];
        auto path_points = jp.GetPathPoints(i);
        temp_spline_.SetPoints(path_points);

        constexpr double step = 40.0;
        constexpr double radius = step / 2.0;

        chunk_coords_.clear();

        // determine which chunks this path intersects with
        for (double t = 0.0; t < (temp_spline_.GetTotalLength() + radius); t += step)
        {
            auto pos = temp_spline_.DeformVertex(glm::dvec3(0.0, t, 0.0));
            auto [min_chunk, max_chunk] = mg::GetChunkRange(map.cfg, AABBD2(pos - radius, pos + radius), true);
            for (int x = min_chunk.x; x <= max_chunk.x; ++x)
            {
                for (int y = min_chunk.y; y <= max_chunk.y; ++y)
                {
                    glm::ivec2 chunk_coord{x, y};
                    chunk_coords_.insert(chunk_coord);
                }
            }
        }

        // add path to all chunks that intersect with it
        for (const auto& chunk_coord : chunk_coords_)
        {
            auto& chunk = chunks_[chunk_coord];

            // insert path to chunk
            auto& chunk_path = chunk.jp.paths.emplace_back(path);

            // insert points
            chunk_path.points_start = static_cast<uint32_t>(chunk.jp.path_points.size());
            chunk.jp.path_points.insert(chunk.jp.path_points.end(), path_points.begin(), path_points.end());

            // request endpoint junctions
            for (uint32_t i = 0; i < 2; ++i)
            {
                auto junction_id = path.endpoints[i].junction_id;
                chunk_junctions_[{chunk_coord.x, chunk_coord.y, junction_id}] = INVALID_ID;
            }
        }
    }

    // add junctions to chunks in the original order
    for (auto& [key, chunk_junction_id] : chunk_junctions_)
    {
        auto& [chunk_x, chunk_y, junction_id] = key;
        auto& chunk = chunks_[{chunk_x, chunk_y}];
        auto& junction = jp.junctions[junction_id];
        
        // insert junction to chunk and update mapping
        chunk_junction_id = static_cast<uint32_t>(chunk.jp.junctions.size());
        auto& chunk_junction = chunk.jp.junctions.emplace_back(junction);

        // reset links (will be updated later with correct junction 
        // indices of the paths the chunk acutally contains)
        for (auto& link : chunk_junction.links)
        {
            link.path_id = INVALID_ID;
        }
    }

    // update path endpoints with junction indices in the chunk
    for (auto& [chunk_coord, chunk] : chunks_)
    {
        for (uint32_t i = 0; i < chunk.jp.paths.size(); ++i)
        {
            auto& path = chunk.jp.paths[i];

            for (uint32_t j = 0; j < 2; ++j)
            {
                auto junction_id = path.endpoints[j].junction_id;
                auto chunk_junction_id = chunk_junctions_.at({chunk_coord.x, chunk_coord.y, junction_id});
                path.endpoints[j].junction_id = chunk_junction_id;

                auto& chunk_junction = chunk.jp.junctions[chunk_junction_id];
                auto& chunk_link = chunk_junction.links[path.endpoints[j].link_idx];
                chunk_link.path_id = i;
                //chunk_link.path_from_end = j == 1; // true if end point //// this should not change
            }
        }
    }


}