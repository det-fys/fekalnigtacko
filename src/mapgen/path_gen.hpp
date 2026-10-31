#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <tuple>
#include <optional>

#include "map_layout.hpp"
#include "defs.hpp"

namespace mg
{

struct JunctionLink
{
    uint32_t path_id = INVALID_ID;
    bool path_from_end = false;
    uint32_t profile_id = INVALID_RES_ID;
    double margin = 0.0;
};

struct Junction
{
    glm::dvec3 pos{0.0};
    //uint32_t node_id = INVALID_ID;
    uint32_t mesh_id = INVALID_RES_ID;
    std::array<JunctionLink, 4> links;
};

struct PathEndpoint
{
    uint32_t junction_node_id = INVALID_ID; // original node id
    uint32_t junction_id = INVALID_ID;     // idx in junctions vector
    uint32_t link_idx = INVALID_ID;
};

struct Path
{
    std::array<PathEndpoint, 2> endpoints;
    uint32_t profile_id = INVALID_ID;

    uint32_t points_start = 0;
    uint32_t points_count = 0;
};

struct PathSet
{
    std::vector<Junction> junctions;

    std::vector<Path> paths;
    std::vector<glm::dvec3> path_points;
    
    std::span<const glm::dvec3> GetPathPoints(uint32_t path_id) const;
};

class PathGenerator
{
public:
    PathGenerator(const ResourceSet& res);

    void Generate(const std::map<uint32_t, Node>& nodes);

    const PathSet& GetPathSet() const { return jp_; }

private:
    void Clear();

    std::optional<std::tuple<uint32_t, bool>> CreateSplinePath(const std::map<uint32_t, Node>& nodes,
                                                               uint32_t start_junction_id, uint32_t start_link_idx);

private:
    const ResourceSet& res_;

    PathSet jp_;

    std::map<std::tuple<uint32_t, uint32_t>, std::tuple<uint32_t, bool>>
        link2path_; // (node_id, link_index) -> (path_id, from_end)

    std::vector<glm::dvec3> points_; // temp storage for path points
};

} // namespace mg