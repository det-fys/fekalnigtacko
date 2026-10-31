#include "path_gen.hpp"

mg::PathGenerator::PathGenerator(const ResourceSet& res) : res_(res) {}

void mg::PathGenerator::Generate(const std::map<uint32_t, Node>& nodes)
{
    Clear();

    std::map<uint32_t, uint32_t> node2junction; // node id -> junction idx

    // iterate over junctions and create paths from their links
    for (const auto& [node_id, node] : nodes)
    {
        if (node.mesh_id == INVALID_RES_ID)
            continue; // not junction

        Junction junction{};
        junction.pos = node.pos;
        junction.mesh_id = node.mesh_id;

        const auto& mesh = res_.GetMeshes().GetByIndex(junction.mesh_id);

        for (uint32_t i = 0; i < node.links.size(); ++i)
        {
            const auto& node_link = node.links[i];
            auto& junction_link = junction.links[i];

            if (node_link == 0)
                continue; // no link

            auto path_info = CreateSplinePath(nodes, node_id, i);
            if (!path_info)
                continue; // failed to create path

            std::tie(junction_link.path_id, junction_link.path_from_end) = *path_info;

            const auto& mesh_link = mesh.links[i];
            junction_link.margin = mesh_link.margin;
            junction_link.profile_id = mesh_link.profile_id;
        }

        auto junction_idx = static_cast<uint32_t>(jp_.junctions.size());
        jp_.junctions.emplace_back(junction);
        node2junction[node_id] = junction_idx;
    }

    // update path endpoints with junction indices
    for (auto& path : jp_.paths)
    {
        for (uint32_t i = 0; i < 2; ++i)
        {
            auto& endpoint = path.endpoints[i];
            endpoint.junction_id = node2junction.at(endpoint.junction_node_id);
        }
    }

}

void mg::PathGenerator::Clear()
{
    // clear temp stuff
    points_.clear();
    link2path_.clear();

    // clear output
    jp_.junctions.clear();
    jp_.paths.clear();
    jp_.path_points.clear();
}

std::optional<std::tuple<uint32_t, bool>> mg::PathGenerator::CreateSplinePath(const std::map<uint32_t, Node>& nodes,
                                                                              uint32_t start_junction_id,
                                                                              uint32_t start_link_idx)
{
    auto start_key = std::make_tuple(start_junction_id, start_link_idx);

    if (auto it = link2path_.find(start_key); it != link2path_.end())
    {
        return it->second; // already processed
    }

    const auto& start_node = nodes.at(start_junction_id);

    const auto& start_mesh = res_.GetMeshes().GetByIndex(start_node.mesh_id);
    if (start_link_idx >= start_mesh.links.size())
        return std::nullopt; // invalid link index

    auto profile_id = start_mesh.links[start_link_idx].profile_id;

    auto second_node_id = start_node.links[start_link_idx];
    if (second_node_id == 0)
        return std::nullopt; // invalid link

    const auto& second_node = nodes.at(second_node_id);

    points_.clear();

    // find the point before junction for correct direction
    auto prev_node_id = start_node.GetPathContinuation(res_, second_node_id);
    points_.push_back(prev_node_id > 0 ? nodes.at(prev_node_id).pos : start_node.pos);

    // add start and second node
    points_.push_back(start_node.pos);
    // spline_points.push_back(second_node.pos);

    // traverse the spline until we reach another junction
    prev_node_id = start_junction_id;
    uint32_t current_node_id = second_node_id;

    while (true)
    {
        auto& current_node = nodes.at(current_node_id);
        points_.push_back(current_node.pos);

        auto next_node_id = current_node.GetPathContinuation(res_, prev_node_id);

        // terminate if current node is a junction
        if (current_node.mesh_id != mg::INVALID_RES_ID)
        {
            // add next node position for correct direction
            points_.push_back(next_node_id > 0 ? nodes.at(next_node_id).pos : current_node.pos);

            break;
        }

        if (next_node_id == 0)
        {
            // reached end without junction -> invalid path
            return std::nullopt;
        }

        prev_node_id = current_node_id;
        current_node_id = next_node_id;
    }

    const auto& end_junction_id = current_node_id;
    const auto& end_node = nodes.at(end_junction_id);

    auto end_link_idx = nodes.at(end_junction_id).GetLinkIdx(prev_node_id);
    if (end_link_idx == INVALID_ID)
        return std::nullopt; // end junction does not link to this path ??

    // check profile match
    const auto& end_mesh = res_.GetMeshes().GetByIndex(end_node.mesh_id);
    if (end_link_idx >= end_mesh.links.size())
        return std::nullopt; // invalid link index

    if (end_mesh.links[end_link_idx].profile_id != profile_id)
        return std::nullopt; // profile mismatch

    auto end_key = std::make_tuple(end_junction_id, end_link_idx);

    // store path
    auto path_id = static_cast<uint32_t>(jp_.paths.size());
    auto& path = jp_.paths.emplace_back();
    path.profile_id = profile_id;

    path.points_start = static_cast<uint32_t>(jp_.path_points.size());
    path.points_count = static_cast<uint32_t>(points_.size());
    jp_.path_points.insert(jp_.path_points.end(), points_.begin(), points_.end());
    
    path.endpoints[0].junction_node_id = start_junction_id;
    path.endpoints[0].link_idx = start_link_idx;
    path.endpoints[1].junction_node_id = end_junction_id;
    path.endpoints[1].link_idx = end_link_idx;

    // store path mapping for both ends
    link2path_[start_key] = std::make_tuple(path_id, false);
    link2path_[end_key] = std::make_tuple(path_id, true);

    return std::make_tuple(path_id, false);
}

std::span<const glm::dvec3> mg::PathSet::GetPathPoints(uint32_t path_id) const
{
    auto& path = paths[path_id];
    return std::span<const glm::dvec3>(path_points.data() + path.points_start, path.points_count);
}
