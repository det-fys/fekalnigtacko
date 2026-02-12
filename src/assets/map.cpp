#include "map.hpp"

#include <algorithm>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include "cache.hpp"
#include "cmdfile.hpp"
#include "utils/files.hpp"

static AABB3 TransformAABB(const AABB3& aabb, const glm::mat4& mat)
{
    const glm::vec3 corners[] = {
        glm::vec3(aabb.min.x, aabb.min.y, aabb.min.z), glm::vec3(aabb.max.x, aabb.min.y, aabb.min.z),
        glm::vec3(aabb.min.x, aabb.max.y, aabb.min.z), glm::vec3(aabb.max.x, aabb.max.y, aabb.min.z),
        glm::vec3(aabb.min.x, aabb.min.y, aabb.max.z), glm::vec3(aabb.max.x, aabb.min.y, aabb.max.z),
        glm::vec3(aabb.min.x, aabb.max.y, aabb.max.z), glm::vec3(aabb.max.x, aabb.max.y, aabb.max.z),
    };

    AABB3 new_aabb;

    for (size_t i = 0; i < 8; ++i)
    {
        glm::vec3 p = mat * glm::vec4(corners[i], 1.0f);
        new_aabb.AddPoint(p);
    }

    return new_aabb;
}

std::shared_ptr<const assets::Map> assets::Map::LoadFromFile(const std::string& filename)
{
    auto map = std::make_shared<Map>();

    MapGraph* graph = nullptr;
    std::vector<std::tuple<size_t, size_t>> graph_edges;

    auto ProcessGraph = [&]() {
        std::sort(graph_edges.begin(), graph_edges.end());
        for (const auto& [from_idx, to_idx] : graph_edges)
        {
            graph->nbs.push_back(to_idx);

            // update node info
            auto& node = graph->nodes[from_idx];
            if (node.nbs == 0)
                node.nbs = graph->nbs.size() - 1;
            node.num_nbs++;
        }
    };

    Chunk* chunk = nullptr;

    LoadCMDFile(filename, [&](const std::string& command, std::istringstream& iss) {
        if (command == "basemodel")
        {
            std::string model_name;
            iss >> model_name;

            map->basemodel_ = CacheManager::GetModel("data/" + model_name + ".mdl");
        }
        else if (command == "static")
        {
            if (!chunk)
                throw std::runtime_error("static in map without chunk");

            ChunkStaticObject obj;
            std::string model_name;
            iss >> model_name;

            obj.model = assets::CacheManager::GetModel("data/" + model_name + ".mdl");

            glm::vec3 angles;

            auto& trans = obj.node.local;
            ParseTransform(iss, trans);

            obj.node.UpdateMatrix();

            obj.aabb = TransformAABB(obj.model->GetAABB(), obj.node.matrix);
            chunk->aabb.AddAABB(obj.aabb);

            std::string flag;
            while (iss >> flag)
            {
                if (flag == "+color")
                {
                    iss >> obj.color.r >> obj.color.g >> obj.color.b;
                }
            }

            chunk->objs.push_back(std::move(obj));
        }
        else if (command == "chunk")
        {
            glm::ivec2 coord;
            chunk = &map->chunks_.emplace_back();
            iss >> coord.x >> coord.y;
            iss >> chunk->aabb.min.x >> chunk->aabb.min.y >> chunk->aabb.min.z;
            iss >> chunk->aabb.max.x >> chunk->aabb.max.y >> chunk->aabb.max.z;
        }
        else if (command == "surface")
        {
            std::string name;
            size_t first, count;
            iss >> name >> first >> count;

            if (!chunk)
                throw std::runtime_error("surface in map without chunk");

#ifdef CLIENT
            if (!map->basemodel_)
                throw std::runtime_error("surface in map with no basemodel");

            auto mesh = map->basemodel_->GetMesh();

            if (!mesh)
                throw std::runtime_error("surface in map with no basemodel mesh");

            auto it = mesh->surface_names.find(name);
            if (it == mesh->surface_names.end())
                throw std::runtime_error("surface name not found");

            if (first + count > mesh->surfaces[it->second].count)
                throw std::runtime_error("surface invalid range");

            chunk->surfaces.emplace_back(it->second, first, count);

#endif /* CLIENT */
        }

        else if (command == "graph")
        {
            if (graph)
                ProcessGraph();

            std::string graph_name;
            iss >> graph_name;

            graph = &map->graphs_[graph_name];
            graph_edges.clear();
        }
        else if (command == "n")
        {
            if (!graph)
                throw std::runtime_error("Map file error: 'n' command without active graph");

            MapGraphNode node;

            iss >> node.position.x >> node.position.y >> node.position.z;

            graph->nodes.emplace_back(std::move(node));
        }
        else if (command == "e")
        {
            if (!graph)
                throw std::runtime_error("Map file error: 'e' command without active graph");

            size_t from_idx, to_idx;
            iss >> from_idx >> to_idx;

            graph_edges.emplace_back(from_idx, to_idx);
        }
    });

    if (graph)
        ProcessGraph();

    return map;
}

const assets::MapGraph* assets::Map::GetGraph(const std::string& name) const
{
    auto it = graphs_.find(name);
    if (it != graphs_.end())
        return &it->second;
    return nullptr;
}

#ifdef CLIENT
void assets::Map::Draw(const game::view::DrawArgs& args) const
{
    if (!basemodel_ || !basemodel_->GetMesh())
        return;

    const auto& mesh = *basemodel_->GetMesh();

    const float max_dist = args.render_distance + 200.0f;
    const float max_dist2 = max_dist * max_dist;

    for (auto& chunk : chunks_)
    {
        glm::vec3 center = (chunk.aabb.min + chunk.aabb.max) * 0.5f;
        if (glm::distance2(args.eye, center) > max_dist2)
            continue;

        if (!args.frustum.IsAABBVisible(chunk.aabb))
            continue;

        DrawChunk(args, mesh, chunk);
    }
}

void assets::Map::DrawChunk(const game::view::DrawArgs& args, const Mesh& basemesh, const Chunk& chunk) const
{
    for (const auto& surface_range : chunk.surfaces)
    {
        auto& surface = basemesh.surfaces[surface_range.idx];

        gfx::DrawSurfaceCmd cmd;
        cmd.surface = &surface;
        cmd.first = surface_range.first;
        cmd.count = surface_range.count;
        args.dlist.AddSurface(cmd);
    }

    for (const auto& obj : chunk.objs)
    {
        if (!obj.model || !obj.model->GetMesh())
            continue;

        if (!args.frustum.IsAABBVisible(obj.aabb))
            continue;

        const auto& surfaces = obj.model->GetMesh()->surfaces;

        for (const auto& surface : surfaces)
        {
            gfx::DrawSurfaceCmd cmd;
            cmd.surface = &surface;
            cmd.matrices = &obj.node.matrix;
            // cmd.color_mod = glm::vec4(obj.color, 1.0f);
            args.dlist.AddSurface(cmd);
        }
    }
}

#endif // CLIENT
