#include "map.hpp"
#include "cache.hpp"
#include "utils/files.hpp"
#include "cmdfile.hpp"

std::shared_ptr<const assets::Map> assets::Map::LoadFromFile(const std::string& filename)
{
    auto map = std::make_shared<Map>();

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

            iss >> obj.transform.position.x >> obj.transform.position.y >> obj.transform.position.z;
            iss >> angles.x >> angles.y >> angles.z;
            obj.transform.SetAngles(angles);
            iss >> obj.transform.scale;

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
    });

    return map;
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
}
#endif // CLIENT