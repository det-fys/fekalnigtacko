#include "vehiclemdl.hpp"

#include "cache.hpp"
#include "cmdfile.hpp"

std::shared_ptr<const assets::VehicleModel> assets::VehicleModel::LoadFromFile(const std::string& filename)
{
    auto veh = std::make_shared<VehicleModel>();

    LoadCMDFile(filename, [&](const std::string& command, CmdLineStream& iss) {
        if (command == "basemodel")
        {
            std::string model_name;
            iss >> model_name;

            veh->basemodel_ = CacheManager::GetModel("data/" + model_name + ".mdl");
        }
        else if (command == "wheel")
        {
            VehicleWheel wheel;

            std::string type_str;
            std::string model_name;
            iss >> type_str >> model_name;

            if (type_str == "FL")
                wheel.type = WHEEL_FL;
            else if (type_str == "FR")
                wheel.type = WHEEL_FR;
            else if (type_str == "RL")
                wheel.type = WHEEL_RL;
            else if (type_str == "RR")
                wheel.type = WHEEL_RR;

            wheel.model = assets::CacheManager::GetModel("data/" + model_name + ".mdl");

            iss >> wheel.position.x >> wheel.position.y >> wheel.position.z;
            iss >> wheel.radius;

            veh->wheels_.emplace_back(wheel);
        }
        else if (command == "loc")
        {
            std::string loc_name;
            iss >> loc_name;
            Transform& trans = veh->locations_[loc_name];
            ParseTransform(iss, trans);
        }
    });

    return veh;
}

const Transform* assets::VehicleModel::GetLocation(const std::string& name) const
{
    auto it = locations_.find(name);
    if (it != locations_.end())
        return &it->second;

    return nullptr;
}
