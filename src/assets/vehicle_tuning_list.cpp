#include "vehicle_tuning_list.hpp"

#include "cmdfile.hpp"

std::unique_ptr<const assets::VehicleTuningList> assets::VehicleTuningList::LoadFromFile(const std::string& filename)
{
    auto tuninglist = std::make_unique<VehicleTuningList>();

    if (!fs::FileExists(filename))
        return tuninglist; // empty

    LoadCMDFile(filename, [&](const std::string& command, std::istringstream& iss) {
        if (command == "wheel")
        {
            VehicleWheelPreset wheel{};
            wheel.displayname = ParseString(iss);
            iss >> wheel.price >> wheel.model;

            tuninglist->wheels.emplace_back(wheel);
        }
    });

    return tuninglist;
}