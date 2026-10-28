#include "map_layout_file.hpp"

void mg::SaveMapLayout(const std::string& file_name, const ResourceSet& res, const MapLayout& map)
{
    std::ostringstream oss;

    // write objs
    for (const auto& obj : map.objs)
    {
        auto& mgmodel = res.GetModels().GetByIndex(obj.model_id);

        const auto& trans = obj.trans;
        oss << "obj " << mgmodel.name << ' ';
        oss << trans.position.x << ' ' << trans.position.y << ' ' << trans.position.z << ' ';
        oss << trans.basis[0][0] << ' ' << trans.basis[0][1] << ' ' << trans.basis[0][2] << ' ';
        oss << trans.basis[1][0] << ' ' << trans.basis[1][1] << ' ' << trans.basis[1][2] << ' ';
        oss << trans.basis[2][0] << ' ' << trans.basis[2][1] << ' ' << trans.basis[2][2] << '\n';
    }

    // write waypoints
    for (const auto& [id, node] : map.nodes)
    {
        const auto& pos = node.pos;

        oss << "node " << id << ' ';

        oss << pos.x << ' ' << pos.y << ' ' << pos.z << ' ';

        // links
        for (auto link_id : node.links)
        {
            oss << ' ' << link_id;
        }
        oss << ' ';

        // mesh
        if (node.mesh_id != mg::INVALID_RES_ID)
        {
            oss << " +mesh " << res.GetMeshes().GetByIndex(node.mesh_id).name;
        }

        oss << '\n';
    }

    fs::WriteFile(file_name, oss.str());
}

mg::MapLayout mg::LoadMapLayout(const std::string& file_name, const ResourceSet& res)
{
    MapLayout map{};

    assets::LoadCMDFile(file_name, [&](const std::string& cmd, CmdLineStream& iss) {
        if (cmd == "obj")
        {
            std::string model_name;
            FarMatrix trans{1.0f};
            iss >> model_name;
            iss >> trans.position.x >> trans.position.y >> trans.position.z;
            for (int i = 0; i < 3; ++i)
                iss >> trans.basis[i][0] >> trans.basis[i][1] >> trans.basis[i][2];

            auto model_id = res.GetModels().GetIndexByName(model_name);
            if (model_id == INVALID_RES_ID)
                throw std::runtime_error("Unknown model: " + model_name);

            auto& obj = map.objs.emplace_back();
            obj.trans = trans;
            obj.model_id = model_id;
        }
        else if (cmd == "node")
        {
            uint32_t id;
            iss >> id;

            auto& node = map.nodes[id];

            iss >> node.pos.x >> node.pos.y >> node.pos.z;

            // read links
            for (int i = 0; i < 4; ++i)
            {
                iss >> node.links[i];
            }

            while (!iss.Eol())
            {
                std::string param;
                iss >> param;

                if (param == "+mesh")
                {
                    std::string mesh_name;
                    iss >> mesh_name;
                    node.mesh_id = res.GetMeshes().GetIndexByName(mesh_name);
                }
                else
                {
                    throw std::runtime_error("Unknown waypoint parameter: " + param);
                }
            }
        }
        else
        {
            throw std::runtime_error("Unknown command: " + cmd);
        }
    });

    return map;
}
