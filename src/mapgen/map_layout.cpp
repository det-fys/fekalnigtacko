#include "map_layout.hpp"

uint32_t mg::Node::GetLinkIdx(uint32_t node_id) const
{
    for (uint32_t i = 0; i < links.size(); ++i)
    {
        if (links[i] == node_id)
            return i;
    }

    return INVALID_ID; // not found
}

uint32_t mg::Node::GetPathContinuation(const ResourceSet& res, uint32_t from_node_id) const
{
    // TODO: make it possible to specify continuations per mesh link

    if (links[0] == from_node_id)
        return links[1];

    if (links[1] == from_node_id)
        return links[0];

    return 0;
}
