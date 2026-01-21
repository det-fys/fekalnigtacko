#pragma once

#include <memory>
#include <string>
#include <map>
#include <vector>

#include "model.hpp"
#include "game/transform_node.hpp"

#ifdef CLIENT
#include "gfx/draw_list.hpp"
#endif // CLIENT

namespace assets
{

struct MapStaticObject
{
    game::TransformNode node;
    std::shared_ptr<const Model> model;
    glm::vec3 color = glm::vec3(1.0f);
};

struct MapGraphNode
{
    glm::vec3 position = glm::vec3(0.0f);
    size_t num_nbs = 0;
    size_t nbs = 0;
};

struct MapGraph
{
    std::vector<MapGraphNode> nodes;
    std::vector<size_t> nbs;
};

class Map
{
public:
    Map() = default;
    static std::shared_ptr<const Map> LoadFromFile(const std::string& filename);

    const std::shared_ptr<const Model>& GetBaseModel() const { return basemodel_; }
    const std::vector<MapStaticObject>& GetStaticObjects() const { return static_objects_; }
    const MapGraph* GetGraph(const std::string& name) const;

    CLIENT_ONLY(void Draw(gfx::DrawList& dlist) const;)

private:
    std::shared_ptr<const Model> basemodel_;
    std::vector<MapStaticObject> static_objects_;
    std::map<std::string, MapGraph> graphs_;
};

} // namespace assets