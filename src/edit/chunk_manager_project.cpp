#include "chunk_manager_project.hpp"

#include "map_project.hpp"

#include <set>

edit::ProjectChunkManager::ProjectChunkManager(std::shared_ptr<const mg::ResourceSet> res, const mg::MapConfig& map_cfg,
                                               Project& project)
    : ClientChunkManager(std::move(res), map_cfg), project_(project)
{
}

void edit::ProjectChunkManager::GetChunkParams(mg::ChunkParams& out_params)
{
    project_.GetChunkParams(out_params);
}

void edit::ProjectChunkManager::OnChunkGenerated(const mg::ChunkGenData& gen_data)
{
    Super::OnChunkGenerated(gen_data);

    auto& chunk_data = chunks_[gen_data.coord];

    chunk_data.vis_verts.clear();
    chunk_data.vis_edges.clear();
    chunk_data.vis_tris.clear();

    if (!gen_data.model || gen_data.model->GetTriangles().empty())
        return;

    auto& verts = gen_data.model->GetVertices();
    auto& tris = gen_data.model->GetTriangles();

    // add verts
    for (const auto& v : verts.positions)
    {
        chunk_data.vis_verts.emplace_back(v.x, v.y);
    }

    // add tris
    std::set<std::tuple<uint32_t, uint32_t>> edges_set;
    for (const auto& t : tris)
    {
        const auto& v = t.vertices;

        chunk_data.vis_tris.emplace_back(v[0], v[1], v[2]);

        edges_set.emplace(std::min(v[0], v[1]), std::max(v[0], v[1]));
        edges_set.emplace(std::min(v[1], v[2]), std::max(v[1], v[2]));
        edges_set.emplace(std::min(v[2], v[0]), std::max(v[2], v[0]));
    }

    // add edges
    for (const auto& e : edges_set)
    {
        chunk_data.vis_edges.emplace_back(e);
    }
}

void edit::ProjectChunkManager::OnChunkRemoved(const glm::ivec2& coord)
{
    Super::OnChunkRemoved(coord);

    chunks_.erase(coord);
}
