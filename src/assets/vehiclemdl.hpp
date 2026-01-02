#pragma once

#include "model.hpp"
#include "utils/transform.hpp"

namespace assets
{

enum VehicleWheelType
{
    WHEEL_REAR = 1,
    WHEEL_RIGHT = 2,

    WHEEL_FL = 0,
    WHEEL_FR = WHEEL_RIGHT,
    WHEEL_RL = WHEEL_REAR | WHEEL_RIGHT,
    WHEEL_RR = WHEEL_REAR | WHEEL_RIGHT,
};

struct VehicleWheel
{
    VehicleWheelType type = WHEEL_FL;
    std::shared_ptr<const Model> model;
    glm::vec3 position;
    float radius;
};

class VehicleModel
{
public:
    VehicleModel() = default;

    static std::shared_ptr<const VehicleModel> LoadFromFile(const std::string& filename);

    const std::shared_ptr<const Model>& GetModel() const { return basemodel_; } 
    const std::vector<VehicleWheel>& GetWheels() const { return wheels_; }

private:
    std::shared_ptr<const Model> basemodel_;
    std::vector<VehicleWheel> wheels_;


};

}