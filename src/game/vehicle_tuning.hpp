#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>
#include <map>
#include <array>

namespace game
{

struct VehicleWheelTuningContext
{
    bool front;
    std::string modelname;
    float radius;
    float z_offset;
    float friction;
    float suspension_stiffness;
    float suspension_max_force;
    float suspension_rest_length;
    float suspension_travel;
    float roll_influence;
    float steering_factor;
    float braking_factor;
    float engine_factor;
};

struct VehicleTuningContext
{
    std::array<uint32_t, 5> colors;
    float mass;
    float engine_force;
    float braking_force;
    float health;
    float steering;
    std::vector<VehicleWheelTuningContext> wheels;
    std::string spz;
};

using VehicleTuningFunction = std::function<void(VehicleTuningContext&)>;

struct VehicleTuningPart
{
    std::string id;
    std::string displayname;
    int64_t price;
    bool stock;
    bool reserved;

    std::vector<VehicleTuningFunction> funcs;
};

struct VehicleTuningGroup
{
    std::string id;
    std::string displayname;
    std::map<std::string, VehicleTuningPart> parts;
};

struct VehicleTuningList
{
    std::vector<VehicleTuningFunction> default_funcs;
    std::vector<VehicleTuningGroup> groups;

    static std::unique_ptr<const VehicleTuningList> LoadFromFile(const std::string& filename);
};

struct VehicleTuning
{
    std::string model;
    std::string spz;
    std::map<std::string, std::string> parts; // group : part
};


}