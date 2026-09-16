#include "chunk.hpp"

#include <cmath>
#include <cstdint>
#include <set>

#include <tpp_interface.hpp>
#include "FastNoiseLite.h"

#include "utils/spline.hpp"

namespace
{

constexpr uint32_t INVALID_ID = 0xFFFFFFFF;

struct JunctionLinkInfo
{
    uint32_t path_id = INVALID_ID;
    bool path_from_end = false;
    uint32_t profile_id = 0;
    float margin = 0.0f;
};

struct JunctionInfo
{
    uint32_t mesh_id = 0;
    uint32_t base_vertex = 0;
    std::array<JunctionLinkInfo, 4> links;
};

struct PathEndpointInfo
{
    uint32_t junction_node_id = INVALID_ID; // original node id
    uint32_t junction_idx = INVALID_ID;     // idx in junctions vector
    uint32_t link_idx = INVALID_ID;
};

struct PathInfo
{
    SplinePath spline;
    std::array<PathEndpointInfo, 2> endpoints;
};

struct RawMeshVertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;

    uint32_t hm_vertex_idx = INVALID_ID;
    uint32_t out_vertex_idx = INVALID_ID;
};

struct RawMeshTriangle
{
    mg::Triangle tri;
    mg::TemplateMaterialRef material;
};

struct ChunkGenContext
{
    const mg::ResourceSet* res;
    const mg::MapConfig* map_cfg;
    glm::ivec2 chunk_coord;
    mg::Chunk* chunk;
    AABB2 bounds;

    // seeds
    uint32_t heightmap_seed;

    // tiles
    float tile_size_m;
    uint32_t tiles_border;
    uint32_t tiles_stride;
    std::vector<mg::ChunkTile> tiles;
    AABB2 tiles_aabb;

    // splines
    const std::map<uint32_t, mg::ChunkSplineNode>* spline_nodes;
    
    std::vector<JunctionInfo> junctions;
    std::vector<PathInfo> paths;
    std::map<std::tuple<uint32_t, uint32_t>, std::tuple<uint32_t, bool>>
        link2path; // (node_id, link_index) -> (path_id, from_end)

    // raw mesh - both visual/collision and heightmesh
    std::vector<RawMeshVertex> raw_verts;
    std::vector<RawMeshTriangle> raw_tris;

    // heightmesh
    std::vector<glm::vec3> hm_verts;
    std::vector<mg::Triangle> hm_tris;
};

struct PoissonCellData
{
    glm::vec2 pos;
    float priority;
};

// Fast, deterministic pseudo-random number generator for a specific grid cell
PoissonCellData GetPoissonCellCandidate(int cx, int cy, float cell_size, uint32_t seed)
{
    // Spatial hash seed based on grid coordinates
    uint32_t h = seed + static_cast<uint32_t>(cx) * 374761393U + static_cast<uint32_t>(cy) * 668265263U;
    h = (h ^ (h >> 13)) * 1274126177U;
    h = h ^ (h >> 16);

    // Helper lambda to extract a random float in [0.0, 1.0) and advance the hash state
    auto next_float = [&h]() -> float {
        h = h * 747796405U + 2891336453U;
        uint32_t word = ((h >> ((h >> 28) + 4)) ^ h) * 277803737U;
        uint32_t res = (word >> 22) ^ word;
        return static_cast<float>(res >> 8) / 16777216.0f; // 24-bit float resolution
    };

    float ox = next_float();
    float oy = next_float();
    float priority = next_float();

    // Convert grid coordinates + local offset to world space
    glm::vec2 pos = glm::vec2(cx + ox, cy + oy) * cell_size;
    return {pos, priority};
}
//
// void ChunkPoisson(uint32_t seed, glm::ivec2 coord, float size, float r, std::vector<glm::vec2>& out_points,
//                  float border_size = 0.0f)
//{
//    float cell_size = r / std::sqrt(2.0f);
//    float r_sq = r * r;
//
//    // Strict chunk bounds
//    glm::vec2 chunk_min = glm::vec2(coord) * size;
//    glm::vec2 chunk_max = chunk_min + glm::vec2(size);
//
//    // Expanded bounds including the requested border
//    glm::vec2 eval_min = chunk_min - glm::vec2(border_size);
//    glm::vec2 eval_max = chunk_max + glm::vec2(border_size);
//
//    // Calculate grid coverage using the EXPANDED bounds
//    int start_cx = static_cast<int>(std::floor(eval_min.x / cell_size));
//    int start_cy = static_cast<int>(std::floor(eval_min.y / cell_size));
//    int end_cx = static_cast<int>(std::ceil(eval_max.x / cell_size));
//    int end_cy = static_cast<int>(std::ceil(eval_max.y / cell_size));
//
//    for (int cy = start_cy; cy < end_cy; ++cy)
//    {
//        for (int cx = start_cx; cx < end_cx; ++cx)
//        {
//            PoissonCellData target = GetPoissonCellCandidate(cx, cy, r, cell_size, seed);
//
//            // BOUNDARY CHECK: Evaluate against the expanded bounds instead of strict bounds
//            if (target.pos.x < eval_min.x || target.pos.x >= eval_max.x || target.pos.y < eval_min.y ||
//                target.pos.y >= eval_max.y)
//            {
//                continue;
//            }
//
//            bool is_valid = true;
//
//            // Check 5x5 surrounding grid neighborhood
//            for (int ny = cy - 2; ny <= cy + 2 && is_valid; ++ny)
//            {
//                for (int nx = cx - 2; nx <= cx + 2; ++nx)
//                {
//                    if (nx == cx && ny == cy)
//                    {
//                        continue;
//                    }
//
//                    PoissonCellData neighbor = GetPoissonCellCandidate(nx, ny, r, cell_size, seed);
//
//                    float dx = target.pos.x - neighbor.pos.x;
//                    float dy = target.pos.y - neighbor.pos.y;
//                    float dist_sq = dx * dx + dy * dy;
//
//                    if (dist_sq < r_sq && neighbor.priority > target.priority)
//                    {
//                        is_valid = false;
//                        break;
//                    }
//                }
//            }
//
//            if (is_valid)
//            {
//                out_points.emplace_back(target.pos);
//            }
//        }
//    }
//}

void ChunkPoissonVariable(uint32_t seed, glm::ivec2 coord, float chunk_size, float r_min, float r_max,
                          float border_size, std::vector<glm::vec2>& out_points,
                          std::function<float(glm::vec2)> radius_fn, std::function<bool(glm::vec2)> filter_fn)
{
    // Base cell size on minimum radius to guarantee <= 1 candidate per cell
    float cell_size = r_min / std::sqrt(2.0f);

    // Calculate maximum search radius in terms of grid cells
    int search_k = static_cast<int>(std::ceil(r_max / cell_size));

    // Strict chunk bounds
    glm::vec2 chunk_min = glm::vec2(coord) * chunk_size;
    glm::vec2 chunk_max = chunk_min + glm::vec2(chunk_size);

    // Expanded bounds including border
    glm::vec2 eval_min = chunk_min - glm::vec2(border_size);
    glm::vec2 eval_max = chunk_max + glm::vec2(border_size);

    // Grid coverage using EXPANDED bounds
    int start_cx = static_cast<int>(std::floor(eval_min.x / cell_size));
    int start_cy = static_cast<int>(std::floor(eval_min.y / cell_size));
    int end_cx = static_cast<int>(std::ceil(eval_max.x / cell_size));
    int end_cy = static_cast<int>(std::ceil(eval_max.y / cell_size));

    for (int cy = start_cy; cy < end_cy; ++cy)
    {
        for (int cx = start_cx; cx < end_cx; ++cx)
        {
            PoissonCellData target = GetPoissonCellCandidate(cx, cy, cell_size, seed);

            // BOUNDARY CHECK: Evaluate against expanded bounds
            if (target.pos.x < eval_min.x || target.pos.x >= eval_max.x || target.pos.y < eval_min.y ||
                target.pos.y >= eval_max.y)
            {
                continue;
            }

            // Query local radius at target position
            float r_target = glm::clamp(radius_fn(target.pos), r_min, r_max);

            bool is_valid = true;

            // Dynamically scaled neighborhood search kernel [-search_k, search_k]
            for (int ny = cy - search_k; ny <= cy + search_k && is_valid; ++ny)
            {
                for (int nx = cx - search_k; nx <= cx + search_k; ++nx)
                {
                    if (nx == cx && ny == cy)
                    {
                        continue;
                    }

                    PoissonCellData neighbor = GetPoissonCellCandidate(nx, ny, cell_size, seed);

                    // Deterministic tie-breaker for identical priorities across chunk boundaries
                    bool neighbor_wins = (neighbor.priority > target.priority) ||
                                         (neighbor.priority == target.priority && (nx > cx || (nx == cx && ny > cy)));

                    if (!neighbor_wins)
                    {
                        continue; // Target has higher priority; neighbor cannot suppress it
                    }

                    // Quick bounding box check before calculating squared distance
                    float dx = target.pos.x - neighbor.pos.x;
                    float dy = target.pos.y - neighbor.pos.y;

                    if (glm::abs(dx) > r_max || glm::abs(dy) > r_max)
                    {
                        continue;
                    }

                    float r_neighbor = glm::clamp(radius_fn(neighbor.pos), r_min, r_max);

                    // Effective clearance between the two points
                    float eff_r = glm::min(r_target, r_neighbor);

                    if ((dx * dx + dy * dy) < (eff_r * eff_r))
                    {
                        is_valid = false;
                        break; // Exits nx loop; outer ny loop exits via '&& is_valid'
                    }
                }
            }

            if (is_valid && (!filter_fn || filter_fn(target.pos)))
            {
                out_points.emplace_back(target.pos);
            }
        }
    }
}

mg::ChunkTile* GetTileAtPos(ChunkGenContext& ctx, const glm::vec2& pos)
{
    if (!ctx.tiles_aabb.Contains(pos))
    {
        return nullptr;
    }

    glm::ivec2 tile_idx = glm::floor((pos - ctx.tiles_aabb.min) / ctx.tile_size_m);
    
    if (tile_idx.x < 0 || tile_idx.x >= static_cast<int>(ctx.tiles_stride) || tile_idx.y < 0 ||
        tile_idx.y >= static_cast<int>(ctx.tiles_stride))
    {
        return nullptr;
    }

    return &ctx.tiles[tile_idx.y * ctx.tiles_stride + tile_idx.x];
}

void InitHeightmap(ChunkGenContext& ctx)
{
    FastNoiseLite noise;
    noise.SetSeed(ctx.heightmap_seed);
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFrequency(0.00001f);
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise.SetFractalOctaves(5);

    FastNoiseLite noise2;
    noise2.SetSeed(ctx.heightmap_seed ^ 0xAAAAAAAA);
    noise2.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise2.SetFrequency(0.001f);
    noise2.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise2.SetFractalOctaves(5);

    for (uint32_t y = 0; y < ctx.tiles_stride; ++y)
    {
        for (uint32_t x = 0; x < ctx.tiles_stride; ++x)
        {
            glm::vec2 tile_pos = ctx.tiles_aabb.min + glm::vec2(x, y) * ctx.tile_size_m;
            float nx = tile_pos.x;
            float ny = tile_pos.y;
            float height = (noise.GetNoise(nx, ny) * 0.5f + 0.5f) * 1500.0f;
            //height *= glm::mix(0.1f, 1.0f, GetHeightFalloff(nx, ny, ctx.map_cfg->chunks * ctx.map_cfg->chunk_size_m));
            height -= 500.0f;

            height += (noise2.GetNoise(nx, ny) * 0.5f + 0.5f) * 25.0f; // add finer detail

            height -= 250.0f;

            auto& tile = ctx.tiles[y * ctx.tiles_stride + x];
            tile.height = height;
        }
    }
}

uint32_t GetPathContinuation(const mg::ChunkSplineNode& node, uint32_t prev_node_id)
{
    if (node.links[0] == prev_node_id)
        return node.links[1];

    if (node.links[1] == prev_node_id)
        return node.links[0];

    return 0;
}

uint32_t GetLinkIndex(const mg::ChunkSplineNode& node, uint32_t link_node_id)
{
    for (uint32_t i = 0; i < node.links.size(); ++i)
    {
        if (node.links[i] == link_node_id)
            return i;
    }
    return INVALID_ID; // not found
}

std::optional<std::tuple<uint32_t, bool>> CreateSplinePath(ChunkGenContext& ctx, uint32_t start_junction_id,
                                                           uint32_t start_link_idx)
{
    auto start_key = std::make_tuple(start_junction_id, start_link_idx);

    if (auto it = ctx.link2path.find(start_key); it != ctx.link2path.end())
    {
        return it->second; // already processed
    }

    const auto& start_node = ctx.spline_nodes->at(start_junction_id);
    auto second_node_id = start_node.links[start_link_idx];
    if (second_node_id == 0)
        return std::nullopt; // invalid link

    const auto& second_node = ctx.spline_nodes->at(second_node_id);
    
    std::vector<glm::vec3> spline_points;
    spline_points.reserve(16);

    // find the point before junction for correct direction
    auto prev_node_id = GetPathContinuation(start_node, second_node_id);
    spline_points.push_back(prev_node_id > 0 ? ctx.spline_nodes->at(prev_node_id).pos : start_node.pos);

    // add start and second node
    spline_points.push_back(start_node.pos);
    //spline_points.push_back(second_node.pos);

    // traverse the spline until we reach another junction
    prev_node_id = start_junction_id;
    uint32_t current_node_id = second_node_id;

    while (true)
    {
        auto& current_node = ctx.spline_nodes->at(current_node_id);
        spline_points.push_back(current_node.pos);

        auto next_node_id = GetPathContinuation(current_node, prev_node_id);

        // terminate if current node is a junction
        if (current_node.type == mg::CHUNK_SPLINE_NODE_JUNCTION)
        {
            // add next node position for correct direction
            spline_points.push_back(next_node_id > 0 ? ctx.spline_nodes->at(next_node_id).pos : current_node.pos);
            
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

    auto end_link_idx = GetLinkIndex(ctx.spline_nodes->at(end_junction_id), prev_node_id);
    auto end_key = std::make_tuple(end_junction_id, end_link_idx);

    // store path
    auto path_id = static_cast<uint32_t>(ctx.paths.size());
    auto& path = ctx.paths.emplace_back();
    path.spline = SplinePath(spline_points);
    path.endpoints[0].junction_node_id = start_junction_id;
    path.endpoints[0].link_idx = start_link_idx;
    path.endpoints[1].junction_node_id = end_junction_id;
    path.endpoints[1].link_idx = end_link_idx;

    // store path mapping for both ends
    ctx.link2path[start_key] = std::make_tuple(path_id, false);
    ctx.link2path[end_key] = std::make_tuple(path_id, true);

    return std::make_tuple(path_id, false);
}

void MakeJunctionsAndPaths(ChunkGenContext& ctx)
{
    std::map<uint32_t, uint32_t> node2junction; // node id -> junction idx

    // iterate over junctions and create paths from their links
    for (const auto& [node_id, node] : *ctx.spline_nodes)
    {
        if (node.type != mg::CHUNK_SPLINE_NODE_JUNCTION)
            continue; // not junction

        JunctionInfo junction{};
        junction.mesh_id = node.res_id;

        const auto& mesh = ctx.res->GetMeshes()[junction.mesh_id];

        for (uint32_t i = 0; i < node.links.size(); ++i)
        {
            const auto& node_link = node.links[i];
            auto& junction_link = junction.links[i];

            if (node_link == 0)
                continue; // no link

            auto path_info = CreateSplinePath(ctx, node_id, i);
            if (!path_info)
                continue; // failed to create path

            std::tie(junction_link.path_id, junction_link.path_from_end) = *path_info;

            const auto& mesh_link = mesh.links[i];
            junction_link.margin = mesh_link.margin;
            junction_link.profile_id = mesh_link.profile_id;
        
        }

        auto junction_idx = static_cast<uint32_t>(ctx.junctions.size());
        ctx.junctions.emplace_back(junction);
        node2junction[node_id] = junction_idx;
    }

    // update path endpoints with junction indices
    for (auto& path : ctx.paths)
    {
        for (uint32_t i = 0; i < 2; ++i)
        {
            auto& endpoint = path.endpoints[i];
            endpoint.junction_idx = node2junction.at(endpoint.junction_node_id);
        }
    }
}

void AddTriangle(ChunkGenContext& ctx, uint32_t v0, uint32_t v1, uint32_t v2, mg::TemplateMaterialRef material)
{
    auto& verts = ctx.raw_verts;

    // smooth vertex normals
    auto normal = glm::normalize(glm::cross(verts[v1].pos - verts[v0].pos, verts[v2].pos - verts[v0].pos));
    verts[v0].normal += normal;
    verts[v1].normal += normal;
    verts[v2].normal += normal;

    auto centroid = (verts[v0].pos + verts[v1].pos + verts[v2].pos) / 3.0f;
    if (!ctx.bounds.Contains(centroid))
    {
        return; // triangle outside chunk bounds
    }

    // add triangle
    auto& tri = ctx.raw_tris.emplace_back();
    tri.tri[0] = v0;
    tri.tri[1] = v1;
    tri.tri[2] = v2;
    tri.material = material;
}

void GenerateJunctionMesh(ChunkGenContext& ctx, JunctionInfo& junction)
{
    const auto& mesh = ctx.res->GetMeshes()[junction.mesh_id];

    junction.base_vertex = static_cast<uint32_t>(ctx.raw_verts.size());

    for (const auto& vert : mesh.verts)
    {
        auto& chunk_vert = ctx.raw_verts.emplace_back();
        chunk_vert.uv = vert.uv;
        //chunk_vert.color = 0xFFFFFFFF; // TODO
        chunk_vert.normal = glm::vec3(0.0f);
        
        chunk_vert.pos = glm::vec3(0.0f);

        for (uint32_t i = 0; i < mesh.links.size(); ++i)
        {
            auto& mesh_link = mesh.links[i];
            auto& junction_link = junction.links[i];

            if (junction_link.path_id == INVALID_ID)
                continue; // no path for this link

            const auto& path = ctx.paths[junction_link.path_id];

            glm::vec3 spline_pos;
            if (!junction_link.path_from_end)
            {
                spline_pos = mesh_link.start_matrix * glm::vec4(vert.pos, 1.0f);
            }
            else
            {
                spline_pos = mesh_link.end_matrix * glm::vec4(vert.pos, 1.0f);
                spline_pos.y += path.spline.GetTotalLength();
            }

            auto w = vert.link_weights[i];
            chunk_vert.pos += path.spline.DeformVertex(spline_pos) * w;
        }
    }

    for (const auto& tri : mesh.tris)
    {
        AddTriangle(ctx, junction.base_vertex + tri.tri[0], junction.base_vertex + tri.tri[1],
                    junction.base_vertex + tri.tri[2], tri.material);
    }
    
}

void LoadJunctionLinkProfileVertices(ChunkGenContext& ctx, const JunctionInfo& junction, uint32_t link_idx,
                                     std::span<uint32_t> out_verts)
{
    const auto& mesh = ctx.res->GetMeshes()[junction.mesh_id];
    const auto& mesh_link = mesh.links[link_idx];
    for (uint32_t i = 0; i < mesh_link.profile_vert_mappings.size(); ++i)
    {
        const auto& profile_vert = mesh_link.profile_vert_mappings[i];
        out_verts[i] = junction.base_vertex + profile_vert;
    }
}

void InsertPathProfileVertices(ChunkGenContext& ctx, const mg::TemplateProfile& profile, const SplinePath& spline,
                               float t, float uv_t, std::span<uint32_t> out_verts)
{
    auto base_vertex = static_cast<uint32_t>(ctx.raw_verts.size());
    ctx.raw_verts.resize(base_vertex + profile.verts.size());
    for (uint32_t i = 0; i < profile.verts.size(); ++i)
    {
        const auto& profile_vert = profile.verts[i];
        auto& chunk_vert = ctx.raw_verts[base_vertex + i];

        chunk_vert.pos = spline.DeformVertex(glm::vec3(profile_vert.pos.x, t, profile_vert.pos.z));
        chunk_vert.normal = glm::vec3(0.0f); // profile_vert.normal; // TODO: transform normal
        chunk_vert.uv = profile_vert.uv + profile_vert.uv_advance * uv_t;
        //chunk_vert.color = 0xFFFFFFFF; // TODO: color

        out_verts[i] = base_vertex + i;
    }
}

void TriangulatePathSegment(ChunkGenContext& ctx, const mg::TemplateProfile& profile, std::span<uint32_t> start_verts,
                            std::span<uint32_t> end_verts, bool is_final)
{
    // TODO: fix uvs on final segment

    for (uint32_t i = 0; i < profile.edges.size(); ++i)
    {
        const auto& start_edge = profile.edges[i];
        const auto& end_edge = is_final ? profile.edges_reverse[i] : profile.edges[i];

        AddTriangle(ctx, start_verts[start_edge.verts[0]], start_verts[start_edge.verts[1]],
                    end_verts[end_edge.verts[0]], start_edge.material);

        AddTriangle(ctx, end_verts[end_edge.verts[1]], end_verts[end_edge.verts[0]], start_verts[start_edge.verts[1]],
                    end_edge.material);
    }
}

void GeneratePathMesh(ChunkGenContext& ctx, const PathInfo& path)
{
    std::array<uint32_t, 2> endpoint_profiles;

    for (uint32_t i = 0; i < 2; ++i)
    {
        const auto& junction = ctx.junctions[path.endpoints[i].junction_idx];
        endpoint_profiles[i] = junction.links[path.endpoints[i].link_idx].profile_id;
    }

    if (endpoint_profiles[0] != endpoint_profiles[1])
    {
        return; // incompatible ends
    }

    const auto& profile = ctx.res->GetProfiles()[endpoint_profiles[0]];

    const auto verts_count = static_cast<uint32_t>(profile.verts.size());
    std::vector<uint32_t> vertices(verts_count * 2);
    std::span<uint32_t> start_verts(vertices.data(), verts_count);
    std::span<uint32_t> end_verts(vertices.data() + verts_count, verts_count);

    const auto& spline = path.spline;

    const auto& start_endpoint = path.endpoints[0];
    const auto& end_endpoint = path.endpoints[1];

    // load start verts from junction mesh
    LoadJunctionLinkProfileVertices(ctx, ctx.junctions[start_endpoint.junction_idx], start_endpoint.link_idx,
                                    start_verts);

    constexpr float step_size = 3.0f; // meters
    constexpr float min_step = step_size * 0.5f;
    const auto start_margin = ctx.junctions[start_endpoint.junction_idx].links[start_endpoint.link_idx].margin;
    const auto start_t = start_margin + step_size;
    const auto end_margin = ctx.junctions[end_endpoint.junction_idx].links[end_endpoint.link_idx].margin;
    const auto end_t = path.spline.GetTotalLength() - end_margin - min_step;
    for (float t = start_t; t < end_t; t += step_size)
    {
        InsertPathProfileVertices(ctx, profile, spline, t, t - start_margin, end_verts);
        TriangulatePathSegment(ctx, profile, start_verts, end_verts, false);

        std::swap(start_verts, end_verts);
    }

    // make final segment connecting to the end junction
    LoadJunctionLinkProfileVertices(ctx, ctx.junctions[end_endpoint.junction_idx], end_endpoint.link_idx,
                                    end_verts);
    TriangulatePathSegment(ctx, profile, start_verts, end_verts, true);


}

void GenerateJunctionAndPathMeshes(ChunkGenContext& ctx)
{
    for (auto& junction : ctx.junctions)
    {
        GenerateJunctionMesh(ctx, junction);
    }

    for (const auto& path : ctx.paths)
    {
        GeneratePathMesh(ctx, path);
    }
}

void GenerateHeightmesh(ChunkGenContext& ctx)
{
    for (const auto& tri : ctx.raw_tris)
    {
        if (tri.material.type != mg::TPL_MATERIAL_HEIGHTMESH)
            continue; // not heightmesh

        const auto& v0 = ctx.raw_verts[tri.tri[0]];
        const auto& v1 = ctx.raw_verts[tri.tri[1]];
        const auto& v2 = ctx.raw_verts[tri.tri[2]];

        mg::Triangle hm_tri{};
        for (uint32_t i = 0; i < 3; ++i)
        {
            auto& v = ctx.raw_verts[tri.tri[i]];
            if (v.hm_vertex_idx == INVALID_ID)
            {
                v.hm_vertex_idx = static_cast<uint32_t>(ctx.hm_verts.size());
                ctx.hm_verts.emplace_back(v.pos);
            }
            hm_tri[i] = v.hm_vertex_idx;
        }

        ctx.hm_tris.emplace_back(hm_tri);
    }
}

void AppendObjectsHeightmeshes(ChunkGenContext& ctx, std::span<const mg::ChunkStaticObject> objs)
{
    for (const auto& obj : objs)
    {
        if (!obj.heightmesh)
            continue;

        auto& verts = obj.heightmesh->GetVerts();
        auto& tris = obj.heightmesh->GetTris();

        uint32_t base_idx = static_cast<uint32_t>(ctx.hm_verts.size());
        
        for (const auto& vert : verts)
        {
            glm::vec4 world_pos = obj.trans * glm::vec4(vert, 1.0f);
            ctx.hm_verts.emplace_back(world_pos);
        }

        for (const auto& tri : tris)
        {
            ctx.hm_tris.emplace_back(base_idx + tri[0], base_idx + tri[1], base_idx + tri[2]);
        }
    }
}

float SignedDistanceToLine(const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p)
{
    glm::vec2 seg = p1 - p0;
    glm::vec2 to_p= p - p0;
    float cross = seg.x * to_p.y - seg.y * to_p.x; // signed area * 2
    return cross / glm::length(seg);                     // now in distance units
}

void RasterizeHeightmesh(ChunkGenContext& ctx)
{
    struct TileUpdate
    {
        float override_height[3] = { 0.0f, 0.0f, 0.0f };
        float override_height_weight[3] = { 0.0f, 0.0f, 0.0f };
    };

    std::vector<TileUpdate> tile_updates(ctx.tiles.size());

    const glm::ivec2 grid_size(ctx.tiles_stride);
    const float eps = 1e-6f;

    for (const auto& tri : ctx.hm_tris)
    {
        std::array<glm::vec2, 3> pos_ts;
        AABB2 tri_aabb_ts;

        for (int i = 0; i < 3; ++i)
        {
            glm::vec3 world_pos = ctx.hm_verts[tri[i]];
            glm::vec2 tile_pos = (glm::vec2(world_pos) - ctx.tiles_aabb.min) / ctx.tile_size_m;
            pos_ts[i] = tile_pos;
            tri_aabb_ts.AddPoint(tile_pos);
        }

        AABB2 grid_aabb{glm::vec2(0.0f), glm::vec2(grid_size)};
        if (!grid_aabb.CollidesWith(tri_aabb_ts))
            continue;

        float area2 = (pos_ts[1].x - pos_ts[0].x) * (pos_ts[2].y - pos_ts[0].y) -
                      (pos_ts[1].y - pos_ts[0].y) * (pos_ts[2].x - pos_ts[0].x);

        if (std::abs(area2) < eps)
            continue; // Skip degenerate triangles

        const int margin = 5;
        auto p0 = glm::clamp(glm::ivec2(glm::floor(tri_aabb_ts.min)) - margin, glm::ivec2(0), grid_size);
        auto p1 = glm::clamp(glm::ivec2(glm::ceil(tri_aabb_ts.max)) + margin + 1, glm::ivec2(0), grid_size);

        for (int y = p0.y; y < p1.y; ++y)
        {
            for (int x = p0.x; x < p1.x; ++x)
            {
                glm::vec2 tile_center = glm::vec2(x, y) + 0.5f;

                // Edge functions for barycentric coordinates
                float e0 = (pos_ts[1].x - pos_ts[0].x) * (tile_center.y - pos_ts[0].y) -
                           (pos_ts[1].y - pos_ts[0].y) * (tile_center.x - pos_ts[0].x);
                float e1 = (pos_ts[2].x - pos_ts[1].x) * (tile_center.y - pos_ts[1].y) -
                           (pos_ts[2].y - pos_ts[1].y) * (tile_center.x - pos_ts[1].x);
                float e2 = (pos_ts[0].x - pos_ts[2].x) * (tile_center.y - pos_ts[2].y) -
                           (pos_ts[0].y - pos_ts[2].y) * (tile_center.x - pos_ts[2].x);

                // Handle back-facing triangles
                if (area2 < 0.0f)
                {
                    e0 = -e0;
                    e1 = -e1;
                    e2 = -e2;
                }

                bool inside = (e0 >= 0.0f && e1 >= 0.0f && e2 >= 0.0f);

                auto& tile = ctx.tiles[y * ctx.tiles_stride + x];
                auto& tile_update = tile_updates[y * ctx.tiles_stride + x];

                if (inside)
                {
                    // Calculate normalized barycentric weights
                    float inv_area = 1.0f / std::abs(area2);
                    float v = e0 * inv_area; // weight for vert 2
                    float w = e1 * inv_area; // weight for vert 0
                    float u = e2 * inv_area; // weight for vert 1

                    float height = u * ctx.hm_verts[tri[0]].z + w * ctx.hm_verts[tri[1]].z + v * ctx.hm_verts[tri[2]].z;

                    tile_update.override_height[0] += height;
                    tile_update.override_height_weight[0] += 1.0f;
                    tile.obstruction_level = 255;
                }
                //else
                //{
                //    // Compute approximate distance to triangle for margin obstruction
                //    float sd0 = SignedDistanceToLine(pos_ts[0], pos_ts[1], tile_center);
                //    float sd1 = SignedDistanceToLine(pos_ts[1], pos_ts[2], tile_center);
                //    float sd2 = SignedDistanceToLine(pos_ts[2], pos_ts[0], tile_center);
                //    if (area2 < 0.0f)
                //    {
                //        sd0 = -sd0;
                //        sd1 = -sd1;
                //        sd2 = -sd2;
                //    }

                //    float sd = glm::min(glm::min(sd0, sd1), sd2);
                //    if (sd >= -static_cast<float>(margin))
                //    {
                //        tile.obstruction_level = glm::max(tile.obstruction_level, static_cast<uint8_t>(128));
                //    }
                //}
            }
        }
    }

    // bleed obstruction levels
    for (uint32_t i = 0; i < 4; ++i)
    {
        for (int x = 0; x < static_cast<int>(ctx.tiles_stride); ++x)
        {
            for (int y = 0; y < static_cast<int>(ctx.tiles_stride); ++y)
            {
                auto& tile = ctx.tiles[y * ctx.tiles_stride + x];

                if (tile.obstruction_level == 255)
                    continue;

                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dx == 0 && dy == 0)
                            continue;
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < static_cast<int>(ctx.tiles_stride) && ny >= 0 &&
                            ny < static_cast<int>(ctx.tiles_stride))
                        {
                            auto& neighbor_tile = ctx.tiles[ny * ctx.tiles_stride + nx];
                            tile.obstruction_level = glm::max(
                                tile.obstruction_level, static_cast<uint8_t>(neighbor_tile.obstruction_level / 2));
                        }
                    }
                }
            }
        }
    }
    //
    // Linear Height Ramp via 2-Pass Distance Field
    //
    constexpr int radius = 30;
    int size = static_cast<int>(ctx.tiles_stride);
    float radius_tiles = static_cast<float>(radius);

    struct DistTile
    {
        float dist = 1e9f; // Distance in tile units
        float height = 0.0f;
    };
    std::vector<DistTile> dt(size * size);

    // 1. Initialize building source tiles
    for (int i = 0; i < size * size; ++i)
    {
        if (ctx.tiles[i].obstruction_level == 255)
        {
            dt[i].dist = 0.0f;
            dt[i].height = tile_updates[i].override_height[0];
        }
    }

    constexpr float d_ortho = 1.0f;      // Straight neighbor distance
    constexpr float d_diag = 1.4142135f; // Diagonal neighbor distance

    // 2. Pass 1: Forward Sweep (Top-Left to Bottom-Right)
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            int idx = y * size + x;
            if (dt[idx].dist == 0.0f)
                continue;

            auto propagate = [&](int nx, int ny, float step) {
                if (nx >= 0 && nx < size && ny >= 0 && ny < size)
                {
                    int nidx = ny * size + nx;
                    if (dt[nidx].dist + step < dt[idx].dist)
                    {
                        dt[idx].dist = dt[nidx].dist + step;
                        dt[idx].height = dt[nidx].height;
                    }
                }
            };

            propagate(x - 1, y, d_ortho);    // Left
            propagate(x, y - 1, d_ortho);    // Top
            propagate(x - 1, y - 1, d_diag); // Top-Left
            propagate(x + 1, y - 1, d_diag); // Top-Right
        }
    }

    // 3. Pass 2: Backward Sweep (Bottom-Right to Top-Left)
    for (int y = size - 1; y >= 0; --y)
    {
        for (int x = size - 1; x >= 0; --x)
        {
            int idx = y * size + x;

            auto propagate = [&](int nx, int ny, float step) {
                if (nx >= 0 && nx < size && ny >= 0 && ny < size)
                {
                    int nidx = ny * size + nx;
                    if (dt[nidx].dist + step < dt[idx].dist)
                    {
                        dt[idx].dist = dt[nidx].dist + step;
                        dt[idx].height = dt[nidx].height;
                    }
                }
            };

            propagate(x + 1, y, d_ortho);    // Right
            propagate(x, y + 1, d_ortho);    // Bottom
            propagate(x + 1, y + 1, d_diag); // Bottom-Right
            propagate(x - 1, y + 1, d_diag); // Bottom-Left
        }
    }

    // 4. Apply True Linear Blend
    for (int i = 0; i < size * size; ++i)
    {
        auto& tile = ctx.tiles[i];

        // Keep building footprints perfectly flat
        if (tile.obstruction_level == 255)
            continue;

        if (dt[i].dist <= radius_tiles)
        {
            // Exact linear falloff from 1.0 at building wall to 0.0 at radius
            float dist = dt[i].dist;
            float weight = 1.0f - (dist / radius_tiles);

            weight = glm::smoothstep(0.0f, 1.0f, weight); // Smooth the transition
            
            tile.height = glm::mix(tile.height, dt[i].height, weight);
        }
    }
}

void TriangulateTerrain(ChunkGenContext& ctx)
{
    const float chunk_tile_m = ctx.tile_size_m;

    auto terrain_points_offset = ctx.hm_verts.size();
    std::vector<glm::vec2> terrain_points;

    const float r = 3.0f;
    const float r_min = 2.0f;
    const float r_border = r * 5.0f;

    const auto& chunk_aabb = ctx.bounds;

    // append corners
    auto c0 = glm::vec2(ctx.chunk_coord) * ctx.map_cfg->chunk_size_m - r_border;
    auto c1 = c0 + (ctx.map_cfg->chunk_size_m + r_border * 2.0f);
    terrain_points.emplace_back(c0);
    terrain_points.emplace_back(glm::vec2(c1.x, c0.y));
    terrain_points.emplace_back(c1);
    terrain_points.emplace_back(glm::vec2(c0.x, c1.y));

    ChunkPoissonVariable(
        ctx.map_cfg->seed, ctx.chunk_coord, ctx.map_cfg->chunk_size_m, r_min, r, r_border, terrain_points,
        [&](glm::vec2 pos) {
            auto tile = GetTileAtPos(ctx, pos);

            float density = tile ? static_cast<float>(tile->obstruction_level) / 64.0f : 0.0f;
            density = glm::clamp(density, 0.0f, 1.0f);
            return glm::mix(r, r_min, density);
        },
        [&](glm::vec2 pos) {
            auto tile = GetTileAtPos(ctx, pos);
            if (!tile)
            {
                return false;
            }

            return tile->obstruction_level < 127;
        });

    std::vector<tpp::Delaunay::Point> del_points;
    for (const auto& p : ctx.hm_verts)
    {
        del_points.emplace_back(p.x, p.y);
    }

    for (const auto& p : terrain_points)
    {
        del_points.emplace_back(p.x, p.y);
    }

    tpp::Delaunay delaunay(del_points);

    std::vector<int> segments;

    // add corners segments
    segments.push_back(static_cast<int>(terrain_points_offset + 0));
    segments.push_back(static_cast<int>(terrain_points_offset + 1));
    segments.push_back(static_cast<int>(terrain_points_offset + 1));
    segments.push_back(static_cast<int>(terrain_points_offset + 2));
    segments.push_back(static_cast<int>(terrain_points_offset + 2));
    segments.push_back(static_cast<int>(terrain_points_offset + 3));
    segments.push_back(static_cast<int>(terrain_points_offset + 3));
    segments.push_back(static_cast<int>(terrain_points_offset + 0));

    // extract edges from tris
    std::set<std::tuple<uint32_t, uint32_t>> mesh_edges;
    std::set<mg::Triangle> mesh_tris_set;
    for (auto tri : ctx.hm_tris)
    {
        const auto& v = tri.verts;
        for (int i = 0; i < 3; ++i)
        {
            size_t v0 = v[i];
            size_t v1 = v[(i + 1) % 3];
            if (v0 > v1)
                std::swap(v0, v1);
            mesh_edges.emplace(v0, v1);
        }

        // add sorted tri to set
        tri.Sort();
        mesh_tris_set.insert(tri);
    }

    for (const auto& [v0, v1] : mesh_edges)
    {
        segments.push_back(static_cast<int>(v0));
        segments.push_back(static_cast<int>(v1));
    }

    delaunay.setSegmentConstraint(segments);

    delaunay.Triangulate();

    std::map<uint32_t, uint32_t> vertex_map; // maps delaunay vertex index to raw vertex index

    std::vector<glm::vec3> normals(terrain_points_offset + terrain_points.size(), glm::vec3(0.0f));

    for (const auto& face : delaunay.faces())
    {
        std::array<int, 3> v = {face.Org(), face.Dest(), face.Apex()};
        if (v[0] < 0 || v[1] < 0 || v[2] < 0)
        {
            continue; // skip invalid faces
        }

        std::array<glm::vec3, 3> vert_pos;
        for (int i = 0; i < 3; ++i)
        {
            if (v[i] < static_cast<int>(terrain_points_offset))
            {
                vert_pos[i] = ctx.hm_verts[v[i]];
            }
            else
            {
                int terrain_idx = v[i] - static_cast<int>(terrain_points_offset);
                auto tile = GetTileAtPos(ctx, terrain_points[terrain_idx]);
                auto height = tile ? tile->height : 0.0f;
                vert_pos[i] = glm::vec3(terrain_points[terrain_idx], height);
            }
        }

        // accumulate normals
        auto tri_normal = glm::normalize(glm::cross(vert_pos[1] - vert_pos[0], vert_pos[2] - vert_pos[0]));
        for (int i = 0; i < 3; ++i)
        {
            normals[v[i]] += tri_normal;
        }

        // check if triangle is not a heightmesh triangle
        mg::Triangle tri_check(v[0], v[1], v[2]);
        tri_check.Sort();

        if (mesh_tris_set.contains(tri_check))
        {
            continue; // skip heightmesh triangles
        }

        auto centroid = (vert_pos[0] + vert_pos[1] + vert_pos[2]) / 3.0f;

        if (!chunk_aabb.Contains(centroid))
        {
            continue;
        }

        auto& tri = ctx.raw_tris.emplace_back();
        tri.material.type = mg::TPL_MATERIAL_TERRAIN;

        // push vertices
        for (int i = 0; i < 3; ++i)
        {
            auto it = vertex_map.find(v[i]);

            if (it == vertex_map.end())
            {
                uint32_t new_idx = static_cast<uint32_t>(ctx.raw_verts.size());

                auto pos = vert_pos[i];
                auto& vert = ctx.raw_verts.emplace_back();
                vert.pos = pos; // Placeholder height; replace with actual terrain height
                vert.uv = glm::vec2(pos) * 0.1f;

                vertex_map[v[i]] = new_idx;

                tri.tri[i] = new_idx;

                //ctx.chunk->aabb.AddPoint(vert.pos);
            }
            else
            {
                tri.tri[i] = it->second;
            }
        }
    }

    // finalize normals
    for (const auto& [v_idx, c_idx] : vertex_map)
    {
        auto& vert = ctx.raw_verts[c_idx];
        vert.normal = glm::normalize(normals[v_idx]);
    }
}

void GenerateOutputMesh(ChunkGenContext& ctx)
{
    // TODO: get these from somewhere
    auto terrain_material_id = ctx.res->GetMaterialIndexByName("grass"); 
    const float terrain_uv_scale = 0.1f;                                 

    // set material for terrain triangles
    for (auto& tri : ctx.raw_tris)
    {
        if (tri.material.type == mg::TPL_MATERIAL_TERRAIN)
        {
            tri.material.id = terrain_material_id;
        }
    }

    // sort by material for batching
    std::sort(ctx.raw_tris.begin(), ctx.raw_tris.end(),
              [](const RawMeshTriangle& a, const RawMeshTriangle& b) { return a.material.id < b.material.id; });

    auto& chunk_mesh = ctx.chunk->mesh;

    for (const auto& tri : ctx.raw_tris)
    {
        if (tri.material.type == mg::TPL_MATERIAL_HEIGHTMESH)
            continue; // dont export that

        // add surface range if material changed
        if (chunk_mesh.surfaces.empty() || chunk_mesh.surfaces.back().material_id != tri.material.id)
        {
            auto& surface = chunk_mesh.surfaces.emplace_back();
            surface.material_id = tri.material.id;
            surface.tri_offset = static_cast<uint32_t>(chunk_mesh.tris.size());
            surface.tri_count = 1;
        }
        else
        {
            ++chunk_mesh.surfaces.back().tri_count;
        }

        const bool is_terrain = tri.material.type == mg::TPL_MATERIAL_TERRAIN;

        auto& out_tri = ctx.chunk->mesh.tris.emplace_back();

        for (uint32_t i = 0; i < 3; ++i)
        {
            auto& vert = ctx.raw_verts[tri.tri[i]];

            // check if already in output
            if (vert.out_vertex_idx != INVALID_ID)
            {
                out_tri[i] = vert.out_vertex_idx;
                continue;
            }

            vert.out_vertex_idx = static_cast<uint32_t>(chunk_mesh.verts.size());
            out_tri[i] = vert.out_vertex_idx;

            auto& out_vert = chunk_mesh.verts.emplace_back();

            // copy position
            out_vert.pos = vert.pos;

            // normalize normal
            out_vert.normal = glm::dot(vert.normal, vert.normal) > 0.0001f ? glm::normalize(vert.normal)
                                                                           : glm::vec3(0.0f, 0.0f, 1.0f);

            // calc uv
            out_vert.uv = is_terrain ? vert.pos * terrain_uv_scale : vert.uv;

            // TODO: color
            out_vert.color = 0xFFFFFFFF;
        }
    }
}


} // namespace

AABB2 mg::GetChunkAABB(const MapConfig& cfg, const glm::ivec2& chunk_pos, bool include_border)
{
    glm::vec2 min = glm::vec2(chunk_pos) * cfg.chunk_size_m;
    glm::vec2 max = min + glm::vec2(cfg.chunk_size_m);

    const float border_m = static_cast<float>(cfg.chunk_border) * cfg.chunk_size_m / static_cast<float>(cfg.chunk_tiles);

    if (include_border)
    {
        min -= border_m;
        max += border_m;
    }
    return AABB2(min, max);
}

std::tuple<glm::ivec2, glm::ivec2> mg::GetChunkRange(const MapConfig& cfg, const AABB2& aabb, bool include_margin)
{
    auto bounds_min = aabb.min;
    auto bounds_max = aabb.max;
    
    if (include_margin)
    {
        float border_m = static_cast<float>(cfg.chunk_border) * cfg.chunk_size_m / static_cast<float>(cfg.chunk_tiles);
        //border_m += 30.0f; // additional margin for heightmap bleed
        bounds_min -= border_m;
        bounds_max += border_m;
    }

    int half_chunks = static_cast<int>(cfg.chunks / 2);

    glm::ivec2 min_chunk = glm::clamp(glm::ivec2(glm::floor(bounds_min / cfg.chunk_size_m)), -half_chunks, half_chunks);
    glm::ivec2 max_chunk = glm::clamp(glm::ivec2(glm::floor(bounds_max / cfg.chunk_size_m)), -half_chunks, half_chunks);

    return {min_chunk, max_chunk};
}

mg::Chunk mg::GenerateChunk(const ResourceSet& res, const MapConfig& cfg, const ChunkParams& params)
{
    mg::Chunk chunk{};
    chunk.coord = params.coord;

    ChunkGenContext ctx{};
    ctx.res = &res;
    ctx.map_cfg = &cfg;
    ctx.spline_nodes = &params.nodes;
    ctx.chunk_coord = params.coord;
    ctx.chunk = &chunk;
    ctx.bounds = GetChunkAABB(cfg, params.coord);

    ctx.heightmap_seed = params.heightmap_seed;

    ctx.tiles_border = cfg.chunk_border;
    ctx.tiles_stride = cfg.chunk_tiles + cfg.chunk_border * 2;
    ctx.tiles.resize(ctx.tiles_stride * ctx.tiles_stride);
    ctx.tile_size_m = cfg.chunk_size_m / static_cast<float>(cfg.chunk_tiles);
    ctx.tiles_aabb.min = ctx.bounds.min - glm::vec2(cfg.chunk_border) * ctx.tile_size_m;
    ctx.tiles_aabb.max = ctx.bounds.max + glm::vec2(cfg.chunk_border) * ctx.tile_size_m;

    InitHeightmap(ctx);

    MakeJunctionsAndPaths(ctx);
    GenerateJunctionAndPathMeshes(ctx);
    
    GenerateHeightmesh(ctx);
    AppendObjectsHeightmeshes(ctx, params.objs);
    RasterizeHeightmesh(ctx);
    TriangulateTerrain(ctx);

    GenerateOutputMesh(ctx);

    return chunk;
}
