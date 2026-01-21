#include "map.hpp"

#include <algorithm>

#include "cache.hpp"
#include "cmdfile.hpp"
#include "utils/files.hpp"

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

    LoadCMDFile(filename, [&](const std::string& command, std::istringstream& iss) {
        if (command == "basemodel")
        {
            std::string model_name;
            iss >> model_name;

            map->basemodel_ = CacheManager::GetModel("data/" + model_name + ".mdl");
        }
        else if (command == "static")
        {
            MapStaticObject obj;

            std::string model_name;
            iss >> model_name;

            obj.model = assets::CacheManager::GetModel("data/" + model_name + ".mdl");

            glm::vec3 angles;

            auto trans = &obj.node.local;

            iss >> trans->position.x >> trans->position.y >> trans->position.z;
            iss >> angles.x >> angles.y >> angles.z;
            trans->SetAngles(angles);
            iss >> trans->scale;

            obj.node.UpdateMatrix();

            std::string flag;
            while (iss >> flag)
            {
                if (flag == "+color")
                {
                    iss >> obj.color.r >> obj.color.g >> obj.color.b;
                }
            }

            map->static_objects_.push_back(std::move(obj));
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
void assets::Map::Draw(gfx::DrawList& dlist) const
{
    if (!basemodel_ || !basemodel_->GetMesh())
        return;

    const auto& surfaces = basemodel_->GetMesh()->surfaces;

    for (const auto& surface : surfaces)
    {
        gfx::DrawSurfaceCmd cmd;
        cmd.surface = &surface;
        dlist.AddSurface(cmd);
    }

    for (const auto& obj : static_objects_)
    {
        if (!obj.model || !obj.model->GetMesh())
            continue;

        const auto& surfaces = obj.model->GetMesh()->surfaces;

        for (const auto& surface : surfaces)
        {
            gfx::DrawSurfaceCmd cmd;
            cmd.surface = &surface;
            cmd.matrices = &obj.node.matrix;
            // cmd.color_mod = glm::vec4(obj.color, 1.0f);
            dlist.AddSurface(cmd);
        }
    }
}
#endif // CLIENT