#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace assets
{

struct VehicleWheelPreset
{
    uint32_t price;
    std::string displayname;
    std::string model;

};

struct VehicleTuningList
{
    std::vector<VehicleWheelPreset> wheels;

    static std::unique_ptr<const VehicleTuningList> LoadFromFile(const std::string& filename);
};


}