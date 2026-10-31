#pragma once

#include <cstdint>
#include <map>
#include <vector>
#include <array>

#include "map_config.hpp"
#include "resources.hpp"
#include "utils/transform.hpp"

namespace mg
{

struct StaticObject
{
    FarMatrix trans{1.0f};
    uint32_t model_id = INVALID_RES_ID;
};

struct Node
{
    glm::dvec3 pos{0.0};
    std::array<uint32_t, 4> links = {0, 0, 0, 0};
    uint32_t mesh_id = INVALID_RES_ID;

    uint32_t GetLinkIdx(uint32_t node_id) const;
    uint32_t GetPathContinuation(const ResourceSet& res, uint32_t from_node_id) const;
};

struct MapLayout
{
    MapConfig cfg{};
    std::vector<StaticObject> objs;
    std::map<uint32_t, Node> nodes;
};

} // namespace mg