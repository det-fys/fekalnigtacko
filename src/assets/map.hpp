#pragma once

#include <memory>
#include <string>

#include "model.hpp"
#include "utils/transform.hpp"

namespace assets
{

struct MapStaticObject
{
    Transform transform;
    std::shared_ptr<const Model> model;
    glm::vec3 color = glm::vec3(1.0f);
};

class Map
{
public:
    Map() = default;
    static std::shared_ptr<const Map> LoadFromFile(const std::string& filename);

private:
    std::shared_ptr<const Model> basemodel_;
    std::vector<MapStaticObject> static_objects_;
};

} // namespace assets