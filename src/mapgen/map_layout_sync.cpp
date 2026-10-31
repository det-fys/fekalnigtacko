#include "map_layout_sync.hpp"

#include "defs.hpp"
#include "net/utils.hpp"

namespace
{

using namespace mg;

uint64_t EncodeID(uint32_t id)
{
    if (id == INVALID_ID)
        return 0;

    return static_cast<uint64_t>(id) + 1;
}

uint32_t DecodeID(uint64_t encoded)
{
    if (encoded == 0)
        return INVALID_ID;

    return static_cast<uint32_t>(encoded - 1);
}

} // namespace

void mg::WriteMapLayoutChunk(net::OutMessage& msg, const MapLayoutChunk& map_chunk)
{
    // write objs
    msg.WriteVarUInt(static_cast<uint64_t>(map_chunk.objs.size()));
    for (const auto& obj : map_chunk.objs)
    {
        msg.WriteVarUInt(EncodeID(obj.model_id));

        net::WriteFarPosition(msg, obj.trans.position);

        net::QuatQ rot_q;
        net::EncodeRotation(glm::quat(obj.trans.basis), rot_q);
        net::WriteRotationQ(msg, rot_q);
    }

    // write junctions
    msg.WriteVarUInt(static_cast<uint64_t>(map_chunk.jp.junctions.size()));
    for (const auto& junction : map_chunk.jp.junctions)
    {
        net::WriteFarPosition(msg, junction.pos);
        msg.WriteVarUInt(EncodeID(junction.mesh_id));
    }

    // write paths
    msg.WriteVarUInt(static_cast<uint64_t>(map_chunk.jp.paths.size()));
    for (uint32_t i = 0; i < map_chunk.jp.paths.size(); ++i)
    {
        const auto& path = map_chunk.jp.paths[i];

        // write endpoints
        for (uint32_t j = 0; j < 2; ++j)
        {
            const auto& endpoint = path.endpoints[j];
            msg.WriteVarUInt(EncodeID(endpoint.junction_id));
            msg.Write(static_cast<uint8_t>(endpoint.link_idx));
        }

        // write points
        auto path_points = map_chunk.jp.GetPathPoints(i);
        if (path_points.size() < 4)
            continue; // invalid path??

        msg.WriteVarUInt(static_cast<uint64_t>(path_points.size() - 2));
        for (uint32_t j = 0; j < path_points.size(); ++j)
        {
            // skip second and second to last points as these can be derived from junction positions
            if (j == 1 || j == path_points.size() - 2)
                continue;

            net::WriteFarPosition(msg, path_points[j]);
        }
    }
}

bool mg::ReadMapLayoutChunk(net::InMessage& msg, MapLayoutChunk& map_chunk, const ResourceSet& res)
{
    constexpr uint64_t MAX_OBJS = 1000;
    constexpr uint64_t MAX_JUNCTIONS = 100;
    constexpr uint64_t MAX_PATHS = 100;
    constexpr uint64_t MAX_PATH_POINTS = 1000;

    // read objs
    uint64_t num_objs = 0;
    if (!msg.ReadVarUInt(num_objs) || num_objs > MAX_OBJS)
        return false;

    map_chunk.objs.resize(num_objs);

    for (uint64_t i = 0; i < num_objs; ++i)
    {
        uint64_t model_id_encoded;
        if (!msg.ReadVarUInt(model_id_encoded))
            return false;

        auto& obj = map_chunk.objs[i];

        // read model
        obj.model_id = DecodeID(model_id_encoded);
        if (!res.GetModels().TryGetByIndex(obj.model_id))
            return false; // invalid model id

        // read pos
        if (!net::ReadFarPosition(msg, obj.trans.position))
            return false;

        // read rot
        net::QuatQ rot_q;
        if (!net::ReadRotationQ(msg, rot_q))
            return false;

        glm::quat rot;
        net::DecodeRotation(rot_q, rot);
        obj.trans.basis = glm::mat3_cast(rot);
    }

    // read junctions
    uint64_t num_junctions = 0;
    if (!msg.ReadVarUInt(num_junctions) || num_junctions > MAX_JUNCTIONS)
        return false;

    map_chunk.jp.junctions.resize(num_junctions);

    for (uint64_t i = 0; i < num_junctions; ++i)
    {
        auto& junction = map_chunk.jp.junctions[i];

        // read pos
        if (!net::ReadFarPosition(msg, junction.pos))
            return false;

        // read mesh id
        uint64_t mesh_id_encoded;
        if (!msg.ReadVarUInt(mesh_id_encoded))
            return false;

        junction.mesh_id = DecodeID(mesh_id_encoded);
        if (!res.GetMeshes().TryGetByIndex(junction.mesh_id))
            return false; // invalid mesh id
    }

    // read paths
    uint64_t num_paths = 0;
    if (!msg.ReadVarUInt(num_paths) || num_paths > MAX_PATHS)
        return false;

    map_chunk.jp.paths.resize(num_paths);

    for (uint64_t i = 0; i < num_paths; ++i)
    {
        auto& path = map_chunk.jp.paths[i];

        // read endpoints
        for (uint32_t j = 0; j < 2; ++j)
        {
            auto& endpoint = path.endpoints[j];

            // read junction
            uint64_t junction_id_encoded;
            if (!msg.ReadVarUInt(junction_id_encoded))
                return false;

            endpoint.junction_id = DecodeID(junction_id_encoded);
            if (endpoint.junction_id >= map_chunk.jp.junctions.size())
                return false; // invalid junction id
            auto& junction = map_chunk.jp.junctions[endpoint.junction_id];

            // read link idx
            uint8_t link_idx;
            if (!msg.Read(link_idx) || link_idx >= junction.links.size())
                return false;
            endpoint.link_idx = link_idx;

            // update junction link to point to this path
            auto& junction_link = junction.links[link_idx];
            junction_link.path_id = static_cast<uint32_t>(i);
            junction_link.path_from_end = j == 1; // true if this is the second endpoint
        }

        // read path points
        uint64_t num_points = 0;
        if (!msg.ReadVarUInt(num_points) || num_points < 2 || num_points > MAX_PATH_POINTS)
            return false;

        auto& path_points = map_chunk.jp.path_points;
        path.points_start = static_cast<uint32_t>(path_points.size());
        path.points_count = static_cast<uint32_t>(num_points + 2); // +2 for the junction points

        path_points.resize(path.points_start + path.points_count);

        for (uint32_t j = 0; j < path.points_count; ++j)
        {
            auto& point = path_points[path.points_start + j];

            if (j == 1)
            {
                point = map_chunk.jp.junctions[path.endpoints[0].junction_id].pos;
                continue;
            }

            if (j == num_points)
            {
                point = map_chunk.jp.junctions[path.endpoints[1].junction_id].pos;
                continue;
            }

            if (!net::ReadFarPosition(msg, point))
                return false;
        }
    }

    return true;
}

MapLayoutChunk mg::ReencodeMapLayoutChunk(std::vector<char>& buffer, const MapLayoutChunk& map_chunk,
                                          const ResourceSet& res)
{
    buffer.clear();
    net::OutMessage msg(buffer);
    mg::WriteMapLayoutChunk(msg, map_chunk);

    // std::cout << "Chunk layout buffer size: " << chunk_layout_buffer_.size() << std::endl;

    net::InMessage msg_in(buffer.data(), buffer.size());
    mg::MapLayoutChunk new_chunk{};
    mg::ReadMapLayoutChunk(msg_in, new_chunk, res);

    return new_chunk;
}
