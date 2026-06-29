#include "map.hpp"

#include <algorithm>

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
    MapLoader loader(filename);
    while (loader.Next()) {}
    return loader.GetMap();
}

const assets::MapGraph* assets::Map::GetGraph(const std::string& name) const
{
    auto it = graphs_.find(name);
    if (it != graphs_.end())
        return &it->second;
    return nullptr;
}

std::span<const assets::MapLocation> assets::Map::GetLocations(const std::string& name) const
{
    auto it = locations_.find(name);
    if (it == locations_.end())
        return std::span<const MapLocation>();

    return it->second;
}

// MapLoader

assets::MapLoader::MapLoader(const std::string& filename) 
    : map_iss_(fs::ReadFileAsStream(filename)), map_(std::make_shared<Map>())
{
}

bool assets::MapLoader::Next()
{
    switch (state_)
    {
    case ML_INIT:
        state_ = ML_READ_MODELS;
        return true;

    case ML_READ_MODELS:
        ReadModels();
        state_ = ML_LOAD_BASEMODEL;
        return true;

    case ML_LOAD_BASEMODEL:
        LoadBaseModel();
        state_ = ML_LOAD_MODELS;
        return true;

    case ML_LOAD_MODELS:
        if (LoadNextModel())
            return true;

        state_ = ML_STRUCTS;
        return true;

    case ML_STRUCTS:
        LoadStructs();
        state_ = ML_FINISHED;
        return true;

    default:
        return false;
    }
}

int assets::MapLoader::GetPercent() const
{
    switch (state_)
    {
    case ML_INIT:
    case ML_READ_MODELS:
        return 0;

    case ML_LOAD_BASEMODEL:
        return 10;

    case ML_LOAD_MODELS:
        return 60 + (model_names_.size() > 0 ? (models_.size() * 30 / model_names_.size()) : 30);

    case ML_STRUCTS:
        return 90;

    default:
        return 100;
    }
}

std::shared_ptr<const assets::Map> assets::MapLoader::GetMap() const
{
    if (state_ != ML_FINISHED)
        return nullptr;

    return map_;
}

void assets::MapLoader::ReadModels()
{
    LoadCMDStream(map_iss_, [&](const std::string& command, CmdLineStream& iss) {
        if (command == "basemodel")
        {
            iss >> basemodel_name_;
        }
        else if (command == "model")
        {
            std::string model_name;
            iss >> model_name;
            model_names_.emplace_back(std::move(model_name));
        }
        else if (command == "endmodels")
        {
            return false;
        }

        return true;
    });
}

void assets::MapLoader::LoadBaseModel()
{
    map_->basemodel_ = CacheManager::GetModel("data/" + basemodel_name_ + ".mdl");
}

bool assets::MapLoader::LoadNextModel()
{
    if (models_.size() >= model_names_.size())
        return false;

    const auto& model_name = model_names_[models_.size()];
    models_.push_back(assets::CacheManager::GetModel("data/" + model_name + ".mdl"));
    
    return true;
}

void assets::MapLoader::LoadStructs()
{
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

    LoadCMDStream(map_iss_, [&](const std::string& command, CmdLineStream& iss) {
        if (command == "static")
        {
            if (!chunk)
                throw std::runtime_error("static in map without chunk");

            MapStaticObject obj;
            size_t model_idx;
            iss >> model_idx;

            if (model_idx >= models_.size())
                throw std::runtime_error("static in map with out of range model idx");

            obj.model = models_[model_idx];

            glm::vec3 angles;

            auto& trans = obj.node.local;
            ParseTransform(iss, trans);

            obj.node.UpdateMatrix();

            obj.aabb = TransformAABB(obj.model->GetAABB(), obj.node.matrix);
            chunk->aabb.AddAABB(obj.aabb);

            std::string flag;
            while (!iss.Eol())
            {
                iss >> flag;

                if (flag == "+color")
                {
                    iss >> obj.color.r >> obj.color.g >> obj.color.b;
                }
            }

            map_->objs_.push_back(std::move(obj));
            chunk->num_objs++;
        }
        else if (command == "chunk")
        {
            glm::ivec2 coord;
            chunk = &map_->chunks_.emplace_back();
            iss >> coord.x >> coord.y;
            iss >> chunk->aabb.min.x >> chunk->aabb.min.y >> chunk->aabb.min.z;
            iss >> chunk->aabb.max.x >> chunk->aabb.max.y >> chunk->aabb.max.z;

            chunk->first_obj = map_->objs_.size();
        }
        else if (command == "surface")
        {
            std::string name;
            size_t first, count;
            iss >> name >> first >> count;

            if (!chunk)
                throw std::runtime_error("surface in map without chunk");

#ifdef CLIENT
            if (!map_->basemodel_)
                throw std::runtime_error("surface in map with no basemodel");

            auto mesh = map_->basemodel_->GetMesh();

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

            graph = &map_->graphs_[graph_name];
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
        else if (command == "loc")
        {
            std::string loc_name;
            iss >> loc_name;
            Transform& trans = map_->locations_[loc_name].emplace_back().transform;
            ParseTransform(iss, trans);
        }

        return true;
    });

    if (graph)
        ProcessGraph();

}
