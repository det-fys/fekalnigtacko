#define GLM_ENABLE_EXPERIMENTAL
#include "global_layout.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <queue>
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

#include <tpp_interface.hpp>

#include "FastNoiseLite.h"
#include "common.hpp"

#include <glm/gtx/norm.hpp>

mg::GlobalLayoutGenerator::GlobalLayoutGenerator() {}

void mg::GlobalLayoutGenerator::Setup(const MapConfig& cfg)
{
    cfg_ = cfg;
    global_ = std::make_shared<GlobalLayout>();

    InitGlobalLayout();
}

void mg::GlobalLayoutGenerator::Generate()
{
    GenerateHeightmap();

    GenerateLandGraph();
    GenerateLandPolys();
    RemoveUselessRegionVerticesAndEdges();
    GenerateLandVertexEdgeInfo();

    GenerateCities();
    GenerateRoads();
}

void mg::GlobalLayoutGenerator::InitGlobalLayout()
{
    // generate subseeds
    rng_.seed(cfg_.seed);
    global_->heightmap_seed = rng_();
    global_->cities_seed = rng_();

    // calculate world size
    global_->world_size = static_cast<float>(cfg_.chunks) * cfg_.chunk_size_m;
}

static float MySmoothstep(float x)
{
    const float x2 = x * x;
    return x2 / (x2 + (x - 1.0f) * (x - 1.0f));
}

static float MySmoothstep(float edge0, float edge1, float x)
{
    if (x <= edge0)
        return 0.0f;
    if (x >= edge1)
        return 1.0f;
    return MySmoothstep((x - edge0) / (edge1 - edge0));
    // return (x - edge0) / (edge1 - edge0);
}

static float GetHeightFalloff(float x, float y, float world_size)
{
    float half_world_size = world_size * 0.5f;
    float dist = glm::length(glm::vec2(x, y) - half_world_size);

    const float shore_d = half_world_size * 0.5f;
    const float border_d = -half_world_size * 0.3f;
    return 1.0f - MySmoothstep(half_world_size - shore_d, half_world_size - border_d, dist);
}

void mg::GlobalLayoutGenerator::GenerateHeightmap()
{
    FastNoiseLite noise;
    noise.SetSeed(global_->heightmap_seed);

    // configure noise parameters
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFrequency(0.00001f);

    noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise.SetFractalOctaves(5);

    // generate area height ranges
    global_->areas.resize(cfg_.chunks * cfg_.chunks);

    for (auto& area : global_->areas)
    {
        area.min_terrain_z = std::numeric_limits<float>::max();
        area.max_terrain_z = std::numeric_limits<float>::lowest();
    }

    const uint32_t size = cfg_.chunks * cfg_.chunk_global_tiles;
    const float tile_size_m = cfg_.chunk_size_m / static_cast<float>(cfg_.chunk_global_tiles);
    // global_->heightmap.resize(size * size);
    for (uint32_t y = 0; y < size; ++y)
    {
        for (uint32_t x = 0; x < size; ++x)
        {
            float nx = static_cast<float>(x) * tile_size_m;
            float ny = static_cast<float>(y) * tile_size_m;
            float height = (noise.GetNoise(nx, ny) * 0.5f + 0.5f) * 1500.0f;
            height *= glm::mix(0.1f, 1.0f, GetHeightFalloff(nx, ny, global_->world_size));
            height -= 500.0f;
            // global_->heightmap[y * size + x] = height;

            auto& area = global_->areas[(y / cfg_.chunk_global_tiles) * cfg_.chunks + (x / cfg_.chunk_global_tiles)];
            area.min_terrain_z = glm::min(area.min_terrain_z, height);
            area.max_terrain_z = glm::max(area.max_terrain_z, height);
        }
    }

    // global height range
    global_->min_terrain_z = std::numeric_limits<float>::max();
    global_->max_terrain_z = std::numeric_limits<float>::lowest();

    for (const auto& area : global_->areas)
    {
        global_->min_terrain_z = glm::min(global_->min_terrain_z, area.min_terrain_z);
        global_->max_terrain_z = glm::max(global_->max_terrain_z, area.max_terrain_z);
    }
}

static std::pair<uint32_t, uint32_t> MakeEdgeKey(uint32_t a, uint32_t b)
{
    if (a > b)
    {
        std::swap(a, b);
    }
    return {a, b};
}

void mg::GlobalLayoutGenerator::GenerateLandGraph()
{
    std::vector<tpp::Delaunay::Point> points;

    std::mt19937 gen(global_->cities_seed);
    std::uniform_int_distribution<uint32_t> dist(0, 6);
    PoissonDiscSampling(gen, 150.0f, glm::vec2(global_->world_size), {}, [&](const glm::vec2& pt) {
        if (dist(gen) != 0)
        {
            return true; // skip some points to create more irregularity
        }

        points.push_back(tpp::Delaunay::Point(pt.x, pt.y));
        return true;
    });

    tpp::Delaunay delaunay(points);
    // delaunay.Triangulate();
    delaunay.Tesselate();

    for (const auto& vert : delaunay.voronoiVertices())
    {
        auto& land_vert = global_->land_vertices.emplace_back();
        land_vert.pos = glm::vec2(vert.x(), vert.y());
    }

    for (const auto& edge : delaunay.voronoiEdges())
    {
        auto start = edge.startPointId();
        tpp::Delaunay::Point normvec{};
        auto end = edge.endPointId(normvec);

        if (start < 0 || end < 0)
        {
            continue; // ?
        }

        if (start > end)
        {
            std::swap(start, end);
        }

        auto& land_edge = global_->land_edges.emplace_back();
        land_edge.p0 = static_cast<uint32_t>(start);
        land_edge.p1 = static_cast<uint32_t>(end);

        land_edge.length =
            glm::length(global_->land_vertices[land_edge.p1].pos - global_->land_vertices[land_edge.p0].pos);
    }
}

void mg::GlobalLayoutGenerator::GenerateLandPolys()
{
    auto& global = *global_;

    // Clear existing data just in case
    global.lands.clear();
    global.land_poly_vertex_idxs.clear();

    if (global.land_vertices.empty() || global.land_edges.empty())
    {
        return;
    }

    // 1. Define directed Half-Edge
    struct HalfEdge
    {
        uint32_t target;
        float angle;
        bool visited;
    };

    // 2. Build Adjacency List
    std::vector<std::vector<HalfEdge>> adj(global.land_vertices.size());

    for (const auto& edge : global.land_edges)
    {
        glm::vec2 p0 = global.land_vertices[edge.p0].pos;
        glm::vec2 p1 = global.land_vertices[edge.p1].pos;

        // Calculate polar angle for both directions
        float ang0 = std::atan2(p1.y - p0.y, p1.x - p0.x);
        float ang1 = std::atan2(p0.y - p1.y, p0.x - p1.x);

        adj[edge.p0].push_back({edge.p1, ang0, false});
        adj[edge.p1].push_back({edge.p0, ang1, false});
    }

    // 3. Sort outgoing edges at every vertex counter-clockwise by angle
    for (auto& edges : adj)
    {
        std::sort(edges.begin(), edges.end(), [](const HalfEdge& a, const HalfEdge& b) { return a.angle < b.angle; });
    }

    struct CandidatePoly
    {
        std::vector<uint32_t> vertices;
        float area;
    };
    std::vector<CandidatePoly> candidates;

    // 4. Traverse Planar Graph
    for (uint32_t start_u = 0; start_u < adj.size(); ++start_u)
    {
        for (size_t start_idx = 0; start_idx < adj[start_u].size(); ++start_idx)
        {

            // Skip already visited edges
            if (adj[start_u][start_idx].visited)
                continue;

            CandidatePoly poly;
            uint32_t curr = start_u;
            uint32_t next = adj[start_u][start_idx].target;

            // Trace the polygon
            while (true)
            {
                poly.vertices.push_back(curr);

                // Find curr->next in curr's adjacency list and mark visited
                auto it_curr = std::find_if(adj[curr].begin(), adj[curr].end(),
                                            [next](const HalfEdge& e) { return e.target == next; });
                it_curr->visited = true;

                // Find the incoming edge (next->curr) in next's adjacency list
                auto it_next = std::find_if(adj[next].begin(), adj[next].end(),
                                            [curr](const HalfEdge& e) { return e.target == curr; });

                // Turn to the NEXT edge counter-clockwise around `next`
                // This traces polygons tightly making the sharpest "right" turns
                size_t next_idx = std::distance(adj[next].begin(), it_next);
                next_idx = (next_idx + 1) % adj[next].size();

                uint32_t next_next = adj[next][next_idx].target;

                // Advance
                curr = next;
                next = next_next;

                // Stop if we looped back to the starting directed edge
                if (curr == start_u && next == adj[start_u][start_idx].target)
                {
                    break;
                }
            }

            // Calculate Polygon Signed Area using Shoelace formula
            poly.area = 0.0f;
            for (size_t i = 0; i < poly.vertices.size(); ++i)
            {
                glm::vec2 p1 = global.land_vertices[poly.vertices[i]].pos;
                glm::vec2 p2 = global.land_vertices[poly.vertices[(i + 1) % poly.vertices.size()]].pos;
                poly.area += (p1.x * p2.y - p2.x * p1.y);
            }
            poly.area *= 0.5f;

            // Discard completely degenerate dead-end branches (zero area)
            if (std::abs(poly.area) > 1e-5f)
            {
                candidates.push_back(poly);
            }
        }
    }

    if (candidates.empty())
        return;

    // 5. Filter out outer bounding hulls
    // Interior polygons will all have the same area sign (e.g., negative).
    // The outer hull will enclose everything but be traced backwards (positive).
    // We count them and keep only the faces matching the majority sign.
    int pos_count = 0;
    int neg_count = 0;
    for (const auto& poly : candidates)
    {
        if (poly.area > 0.0f)
            pos_count++;
        else
            neg_count++;
    }

    bool keep_positive = (pos_count > neg_count);

    // 6. Write to Global Layout structs
    for (const auto& poly : candidates)
    {
        bool is_positive = poly.area > 0.0f;

        // Skip the exterior bounding hull(s)
        if (is_positive != keep_positive)
            continue;

        // check if not in water
        uint32_t num_dry_vertices = 0;
        for (uint32_t v_idx : poly.vertices)
        {
            const auto& pos = global.land_vertices[v_idx].pos;
            if (pos.x < 0.0f || pos.y < 0.0f || pos.x >= global.world_size || pos.y >= global.world_size)
            {
                num_dry_vertices = 0;
                break;
            }

            uint32_t area_x = glm::min(static_cast<uint32_t>(pos.x / cfg_.chunk_size_m), cfg_.chunks - 1);
            uint32_t area_y = glm::min(static_cast<uint32_t>(pos.y / cfg_.chunk_size_m), cfg_.chunks - 1);
            const auto& area = global.areas[area_y * cfg_.chunks + area_x];
            if (area.min_terrain_z > 0.0f)
            {
                num_dry_vertices++;
            }
        }

        if (num_dry_vertices < poly.vertices.size() / 2)
        {
            continue;
        }

        Land land{};
        land.vertex_start = static_cast<uint32_t>(global.land_poly_vertex_idxs.size());
        land.vertex_count = static_cast<uint32_t>(poly.vertices.size());

        // Push indices into the flat array
        for (uint32_t v_idx : poly.vertices)
        {
            global.land_poly_vertex_idxs.push_back(v_idx);
        }

        global.lands.push_back(land);
    }
}

void mg::GlobalLayoutGenerator::RemoveUselessRegionVerticesAndEdges()
{
    auto& global = *global_;

    if (global.land_vertices.empty())
        return;

    const size_t old_count = global.land_vertices.size();

    // Mark used vertices
    std::vector<bool> is_used(old_count, false);

    // Check references from valid polygons
    for (uint32_t v_idx : global.land_poly_vertex_idxs)
    {
        is_used[v_idx] = true;
    }

    // Compact vertex list & create old->new mapping
    std::vector<LandVertex> compacted_vertices;
    compacted_vertices.reserve(old_count);

    std::vector<uint32_t> old_to_new(old_count, UINT32_MAX);

    for (uint32_t old_idx = 0; old_idx < old_count; ++old_idx)
    {
        if (is_used[old_idx])
        {
            old_to_new[old_idx] = static_cast<uint32_t>(compacted_vertices.size());
            compacted_vertices.push_back(global.land_vertices[old_idx]);
        }
    }

    // Swap old vertex buffer with the compacted one
    global.land_vertices = std::move(compacted_vertices);

    // Update Polygon Index References
    for (uint32_t& v_idx : global.land_poly_vertex_idxs)
    {
        v_idx = old_to_new[v_idx];
    }

    // Update Edge References (and remove orphan edges if desired)
    std::vector<LandEdge> active_edges;
    active_edges.reserve(global.land_edges.size());

    for (const auto& edge : global.land_edges)
    {
        auto new_edge = edge;
        new_edge.p0 = old_to_new[edge.p0];
        new_edge.p1 = old_to_new[edge.p1];

        // Keep edge only if BOTH vertices are part of valid polygons
        if (new_edge.p0 != UINT32_MAX && new_edge.p1 != UINT32_MAX)
        {
            active_edges.push_back(new_edge);

            // add to map
            auto edge_key = MakeEdgeKey(new_edge.p0, new_edge.p1);
            global.land_edge_map[edge_key] = static_cast<uint32_t>(active_edges.size() - 1);
        }
    }

    global.land_edges = std::move(active_edges);
}

void mg::GlobalLayoutGenerator::GenerateLandVertexEdgeInfo()
{
    auto& global = *global_;

    for (uint32_t i = 0; i < global.land_edges.size(); ++i)
    {
        const auto& land_edge = global.land_edges[i];
        auto& vert_a = global.land_vertices[land_edge.p0];
        auto& vert_b = global.land_vertices[land_edge.p1];

        if (vert_a.num_edges >= MAX_LAND_VERTEX_EDGES || vert_b.num_edges >= MAX_LAND_VERTEX_EDGES)
        {
            continue; // skip if max edges reached
        }

        vert_a.edge_idxs[vert_a.num_edges++] = i;
        vert_b.edge_idxs[vert_b.num_edges++] = i;
    }
}

void mg::GlobalLayoutGenerator::GenerateCities()
{
    float radius = 70000.0f;
    float population = 100000.0f;

    std::mt19937 gen(global_->cities_seed);

    std::vector<glm::vec2> positions;
    std::vector<float> populations;

    // generate random municipalities using Poisson Disc Sampling
    while (radius > 2000.0f)
    {
        PoissonDiscSampling(gen, radius, glm::vec2(global_->world_size), positions, [&](const glm::vec2& pt) {
            uint32_t area_x = glm::min(static_cast<uint32_t>(pt.x / cfg_.chunk_size_m), cfg_.chunks - 1);
            uint32_t area_y = glm::min(static_cast<uint32_t>(pt.y / cfg_.chunk_size_m), cfg_.chunks - 1);
            const auto& area = global_->areas[area_y * cfg_.chunks + area_x];

            if (area.min_terrain_z < 0.0f)
            {
                return true; // continue
            }

            positions.push_back(pt);
            populations.push_back(population);
            return true;
        });

        radius *= 0.6f;
        population *= 0.7f;
    }

    // assign municipalities to land vertices
    for (size_t i = 0; i < positions.size(); ++i)
    {
        const auto& pos = positions[i];
        float pop = populations[i];
        // Find nearest land vertex
        uint32_t nearest_idx = INVALID_IDX;
        float nearest_dist2 = std::numeric_limits<float>::max();
        for (uint32_t v_idx = 0; v_idx < global_->land_vertices.size(); ++v_idx)
        {
            const auto& vert_pos = global_->land_vertices[v_idx].pos;
            float dist2 = glm::distance2(vert_pos, pos);
            if (dist2 < nearest_dist2)
            {
                nearest_dist2 = dist2;
                nearest_idx = v_idx;
            }
        }

        if (nearest_idx == INVALID_IDX)
        {
            continue; // no valid land vertex found
        }

        auto& land_vert = global_->land_vertices[nearest_idx];

        // check if there is already a city at this vertex and if so, merge populations
        if (land_vert.city_idx != INVALID_IDX)
        {
            auto& existing_city = global_->cities[land_vert.city_idx];
            existing_city.population += pop;
            continue;
        }

        uint32_t city_idx = static_cast<uint32_t>(global_->cities.size());
        auto& city = global_->cities.emplace_back();
        city.population = pop;
        city.land_vertex_idx = nearest_idx;

        land_vert.city_idx = city_idx;
    }
}

static std::set<std::pair<uint32_t, uint32_t>> GetRelativeNeighborhoodEdges(
    const std::vector<glm::vec2>& points, const std::set<std::pair<uint32_t, uint32_t>>& edges)
{
    std::set<std::pair<uint32_t, uint32_t>> rng_edges;

    for (const auto& edge : edges)
    {
        uint32_t u = edge.first;
        uint32_t v = edge.second;

        // Safety check to ensure indices exist
        if (u >= points.size() || v >= points.size())
        {
            continue;
        }

        const glm::vec2& p_u = points[u];
        const glm::vec2& p_v = points[v];

        // Calculate squared distance between u and v
        glm::vec2 diff_uv = p_u - p_v;
        float dist2_uv = diff_uv.x * diff_uv.x + diff_uv.y * diff_uv.y;

        bool is_rng_edge = true;

        // Check if any other point w lies within the lune of u and v
        for (uint32_t w = 0; w < points.size(); ++w)
        {
            if (w == u || w == v)
            {
                continue;
            }

            const glm::vec2& p_w = points[w];

            // Calculate squared distance from u to w
            glm::vec2 diff_uw = p_u - p_w;
            float dist2_uw = diff_uw.x * diff_uw.x + diff_uw.y * diff_uw.y;

            // Optimization: If w is further from u than v is, it cannot be in the lune.
            // We can skip calculating the distance from v to w.
            if (dist2_uw >= dist2_uv)
            {
                continue;
            }

            // Calculate squared distance from v to w
            glm::vec2 diff_vw = p_v - p_w;
            float dist2_vw = diff_vw.x * diff_vw.x + diff_vw.y * diff_vw.y;

            // If w is strictly closer to BOTH u and v than the distance between
            // u and v, then the lune is not empty, and this is not an RNG edge.
            if (dist2_vw < dist2_uv)
            {
                is_rng_edge = false;
                break; // No need to check other points for this edge
            }
        }

        // If the lune was empty, add it to our final set, ensuring the lower index is first
        if (is_rng_edge)
        {
            uint32_t lower = std::min(u, v);
            uint32_t upper = std::max(u, v);
            rng_edges.insert({lower, upper});
        }
    }

    return rng_edges;
}

static void LandAStar(const mg::GlobalLayout& global, uint32_t start_vertex_idx, uint32_t goal_vertex_idx,
                      std::vector<uint32_t>& out_path)
{
    out_path.clear();

    // Safety check for valid indices
    if (start_vertex_idx >= global.land_vertices.size() || goal_vertex_idx >= global.land_vertices.size())
    {
        return;
    }

    if (start_vertex_idx == goal_vertex_idx)
    {
        out_path.push_back(start_vertex_idx);
        return;
    }

    size_t num_vertices = global.land_vertices.size();

    // Track best costs to a node and the path tree
    std::vector<float> g_score(num_vertices, std::numeric_limits<float>::infinity());
    std::vector<uint32_t> came_from(num_vertices, UINT32_MAX);

    // Min-priority queue stores pairs of {f_score, vertex_idx}
    using Node = std::pair<float, uint32_t>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open_set;

    // Euclidean distance heuristic
    auto heuristic = [&global](uint32_t a, uint32_t b) {
        return glm::distance(global.land_vertices[a].pos, global.land_vertices[b].pos);
    };

    g_score[start_vertex_idx] = 0.0f;
    open_set.push({heuristic(start_vertex_idx, goal_vertex_idx), start_vertex_idx});

    while (!open_set.empty())
    {
        float current_f = open_set.top().first;
        uint32_t current = open_set.top().second;
        open_set.pop();

        // Goal reached: reconstruct the path
        if (current == goal_vertex_idx)
        {
            uint32_t curr = goal_vertex_idx;
            while (curr != UINT32_MAX)
            {
                out_path.push_back(curr);
                curr = came_from[curr];
            }
            std::reverse(out_path.begin(), out_path.end());
            return;
        }

        // Stale node check: skip if we've already found a better path to 'current'
        // since this specific node was added to the priority queue.
        if (current_f > g_score[current] + heuristic(current, goal_vertex_idx))
        {
            continue;
        }

        const auto& vertex = global.land_vertices[current];

        for (uint32_t i = 0; i < vertex.num_edges; ++i)
        {
            uint32_t edge_idx = vertex.edge_idxs[i];
            const auto& edge = global.land_edges[edge_idx];

            // Resolve which end of the edge is the neighbor
            uint32_t neighbor = (edge.p0 == current) ? edge.p1 : edge.p0;

            float tentative_g_score = g_score[current] + edge.cost;

            // Relaxation step
            if (tentative_g_score < g_score[neighbor])
            {
                came_from[neighbor] = current;
                g_score[neighbor] = tentative_g_score;

                float f_score = tentative_g_score + heuristic(neighbor, goal_vertex_idx);
                open_set.push({f_score, neighbor});
            }
        }
    }
}

static void GenerateRoadsLevel(mg::GlobalLayout& global, float min_population, float traffic_score_mult,
                               bool use_relative_neighborhood, float max_edge_length = 250000.0f)
{
    // determine which cities should be connected
    std::vector<tpp::Delaunay::Point> points;
    std::vector<glm::vec2> point_positions;
    std::vector<uint32_t> city_indices;
    for (uint32_t i = 0; i < global.cities.size(); ++i)
    {
        const auto& city = global.cities[i];
        if (city.population < min_population)
        {
            continue; // skip cities below the population threshold
        }

        const auto& vert = global.land_vertices[city.land_vertex_idx];
        points.push_back(tpp::Delaunay::Point(vert.pos.x, vert.pos.y));
        point_positions.push_back(vert.pos);
        city_indices.push_back(i);
    }


    std::set<std::pair<uint32_t, uint32_t>> connection_edges;

    if (points.size() < 2)
    {
        return;
    }

    if (points.size() > 2)
    {
        tpp::Delaunay delaunay(points);
        delaunay.Triangulate();

        for (const auto& face : delaunay.faces())
        {
            int v0 = face.Org();
            int v1 = face.Dest();
            int v2 = face.Apex();

            if (v0 < 0 || v1 < 0 || v2 < 0)
            {
                continue; // skip invalid faces
            }

            std::array<uint32_t, 3> vertex_indices;
            vertex_indices[0] = static_cast<uint32_t>(v0);
            vertex_indices[1] = static_cast<uint32_t>(v1);
            vertex_indices[2] = static_cast<uint32_t>(v2);

            for (int i = 0; i < 3; ++i)
            {
                auto idx0 = vertex_indices[i];
                auto idx1 = vertex_indices[(i + 1) % 3];

                if (idx0 > idx1)
                {
                    std::swap(idx0, idx1);
                }

                connection_edges.insert({idx0, idx1});
            }
        }

        // filter connection edges using Relative Neighborhood Graph
        if (use_relative_neighborhood)
        {
            connection_edges = GetRelativeNeighborhoodEdges(point_positions, connection_edges);
        }

        // filter edges by max length
        {
            std::set<std::pair<uint32_t, uint32_t>> filtered_edges;
            for (const auto& edge : connection_edges)
            {
                const auto& pos_a = point_positions[edge.first];
                const auto& pos_b = point_positions[edge.second];
                float dist2 = glm::distance2(pos_a, pos_b);
                if (dist2 <= max_edge_length * max_edge_length)
                {
                    filtered_edges.insert(edge);
                }
            }
            connection_edges = std::move(filtered_edges);
        }
    }
    else
    {
        // If there are exactly two points, connect them directly
        connection_edges.insert({0, 1});
    }


    // init land edge costs based on distance and current traffic factor
    for (auto& land_edge : global.land_edges)
    {
        land_edge.cost = land_edge.length * (1.0f + glm::clamp(1.0f - land_edge.traffic_score, 0.0f, 1.0f) * 0.5f);
    }

    std::vector<uint32_t> path;

    // find connection paths
    for (const auto& edge : connection_edges)
    {
        // global.roads.push_back({city_indices[edge.first], city_indices[edge.second]});

        const auto& city0 = global.cities[city_indices[edge.first]];
        const auto& city1 = global.cities[city_indices[edge.second]];

        auto v0 = city0.land_vertex_idx;
        auto v1 = city1.land_vertex_idx;

        auto& pos0 = global.land_vertices[v0].pos;
        auto& pos1 = global.land_vertices[v1].pos;

        auto distance = glm::distance(pos0, pos1);
        float traffic_score = glm::min(city0.population, city1.population) *
                              glm::log(1.0f + glm::max(city0.population, city1.population)) * traffic_score_mult /
                              glm::pow(distance, 0.5f);

        LandAStar(global, v0, v1, path);

        if (path.size() < 2)
        {
            continue; // no valid path found
        }

        for (size_t i = 0; i < path.size() - 1; ++i)
        {
            uint32_t p0 = path[i];
            uint32_t p1 = path[i + 1];

            auto it = global.land_edge_map.find(MakeEdgeKey(p0, p1));
            if (it == global.land_edge_map.end())
            {
                continue; // edge not found
            }

            uint32_t edge_idx = it->second;
            global.land_edges[edge_idx].traffic_score =
                glm::max(global.land_edges[edge_idx].traffic_score, traffic_score);
        }
    }
}

void mg::GlobalLayoutGenerator::GenerateRoads()
{
    
    float traffic_score_mult = 0.00015f;

    GenerateRoadsLevel(*global_, 70000.0f, traffic_score_mult * 2.0f, false);
    GenerateRoadsLevel(*global_, 30000.0f, traffic_score_mult * 1.3f, true, 50000.0f);
    GenerateRoadsLevel(*global_, 15000.0f, traffic_score_mult * 1.1f, true, 10000.0f);
    GenerateRoadsLevel(*global_, 0.0f, traffic_score_mult * 1.0f, false, 10000.0f);
}
