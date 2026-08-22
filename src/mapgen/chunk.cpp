#include "chunk.hpp"

#include <cmath>
#include <cstdint>
#include <set>

#include <tpp_interface.hpp>

namespace
{

struct ChunkGenContext
{
    const mg::MapConfig* map_cfg;
    glm::ivec2 chunk_coord;
    mg::Chunk* chunk;
    AABB2 bounds;

    // tiles
    float tile_size_m;
    uint32_t tiles_border;
    uint32_t tiles_stride;
    std::vector<mg::ChunkTile> tiles;
    AABB2 tiles_aabb;

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

void TriangulateTerrain(ChunkGenContext& ctx)
{
    const float chunk_tile_m = ctx.tile_size_m;

    auto terrain_points_offset = ctx.hm_verts.size();
    std::vector<glm::vec2> terrain_points;

    const float r = 5.0f;
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

    std::map<uint32_t, uint32_t> vertex_map; // maps delaunay vertex index to chunk vertex index

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
                vert_pos[i] = glm::vec3(terrain_points[terrain_idx],
                                        0.0f); // Placeholder height; replace with actual terrain height

            }
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

        auto& tri = ctx.chunk->mesh.tris.emplace_back();

        // push vertices
        for (int i = 0; i < 3; ++i)
        {
            auto it = vertex_map.find(v[i]);

            if (it == vertex_map.end())
            {
                uint32_t new_idx = static_cast<uint32_t>(ctx.chunk->mesh.verts.size());

                auto pos = vert_pos[i];
                auto& vert = ctx.chunk->mesh.verts.emplace_back();
                vert.pos = pos; // Placeholder height; replace with actual terrain height
                vert.normal = glm::vec3(0.0f, 0.0f, 1.0f); // Placeholder normal; replace with actual terrain normal
                vert.uv = glm::vec2(pos) * 0.25f;

                vertex_map[v[i]] = new_idx;

                tri[i] = new_idx;

                ctx.chunk->aabb.AddPoint(vert.pos);
            }
            else
            {
                tri[i] = it->second;
            }
        }
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

                if (inside)
                {
                    // Calculate normalized barycentric weights
                    float inv_area = 1.0f / std::abs(area2);
                    float v = e0 * inv_area; // weight for vert 2
                    float w = e1 * inv_area; // weight for vert 0
                    float u = e2 * inv_area; // weight for vert 1

                    float height = u * ctx.hm_verts[tri[0]].z + w * ctx.hm_verts[tri[1]].z + v * ctx.hm_verts[tri[2]].z;

                    tile.height += height;
                    tile.height_weight += 1.0f;
                    tile.obstruction_level = 255;
                }
                else
                {
                    // Compute approximate distance to triangle for margin obstruction
                    float sd0 = SignedDistanceToLine(pos_ts[0], pos_ts[1], tile_center);
                    float sd1 = SignedDistanceToLine(pos_ts[1], pos_ts[2], tile_center);
                    float sd2 = SignedDistanceToLine(pos_ts[2], pos_ts[0], tile_center);
                    if (area2 < 0.0f)
                    {
                        sd0 = -sd0;
                        sd1 = -sd1;
                        sd2 = -sd2;
                    }

                    float sd = glm::min(glm::min(sd0, sd1), sd2);
                    if (sd >= -static_cast<float>(margin))
                    {
                        tile.obstruction_level = glm::max(tile.obstruction_level, static_cast<uint8_t>(128));
                    }
                }
            }
        }
    }

    // Normalize heights across updated tiles
    for (auto& tile : ctx.tiles)
    {
        if (tile.height_weight > 0.0f)
        {
            tile.height /= tile.height_weight;
            tile.height_weight = 1.0f;
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

std::tuple<glm::ivec2, glm::ivec2> mg::GetChunkRange(const MapConfig& cfg, const AABB2& aabb)
{
    int half_chunks = static_cast<int>(cfg.chunks / 2);

    glm::ivec2 min_chunk = glm::clamp(glm::ivec2(glm::floor(aabb.min / cfg.chunk_size_m)), -half_chunks, half_chunks);
    glm::ivec2 max_chunk = glm::clamp(glm::ivec2(glm::ceil(aabb.max / cfg.chunk_size_m)), -half_chunks, half_chunks);

    return {min_chunk, max_chunk};
}

mg::Chunk mg::GenerateChunk(const MapConfig& cfg, const ChunkParams& params)
{
    mg::Chunk chunk{};

    ChunkGenContext ctx{};
    ctx.map_cfg = &cfg;
    ctx.chunk_coord = params.coord;
    ctx.chunk = &chunk;
    ctx.bounds = GetChunkAABB(cfg, params.coord);

    ctx.tiles_border = cfg.chunk_border;
    ctx.tiles_stride = cfg.chunk_tiles + cfg.chunk_border * 2;
    ctx.tiles.resize(ctx.tiles_stride * ctx.tiles_stride);
    ctx.tile_size_m = cfg.chunk_size_m / static_cast<float>(cfg.chunk_tiles);
    ctx.tiles_aabb.min = ctx.bounds.min - glm::vec2(cfg.chunk_border) * ctx.tile_size_m;
    ctx.tiles_aabb.max = ctx.bounds.max + glm::vec2(cfg.chunk_border) * ctx.tile_size_m;

    AppendObjectsHeightmeshes(ctx, params.objs);
    RasterizeHeightmesh(ctx);
    TriangulateTerrain(ctx);

    return chunk;
}
