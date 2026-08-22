#pragma once

#include <vector>
#include <memory>
#include <random>
#include <array>
#include <map>

#include <glm/glm.hpp>

#include "map_config.hpp"

namespace mg
{

constexpr uint32_t INVALID_IDX = UINT32_MAX;

struct GlobalCity
{
    uint32_t land_vertex_idx = INVALID_IDX;
    float population;
};

struct Road
{
    uint32_t city_a = INVALID_IDX;
    uint32_t city_b = INVALID_IDX;

};

struct Area
{
    float min_terrain_z = 0.0f;
    float max_terrain_z = 0.0f;
};

constexpr uint32_t MAX_LAND_VERTEX_EDGES = 6;

struct LandVertex
{
    glm::vec2 pos;
    uint32_t city_idx = INVALID_IDX;

    uint32_t num_edges = 0;
    std::array<uint32_t, MAX_LAND_VERTEX_EDGES> edge_idxs;
};

struct LandEdge
{
    uint32_t p0 = INVALID_IDX;
    uint32_t p1 = INVALID_IDX;

    // road
    float length = 0.0f;
    float cost = 0.0f;
    float traffic_score = 0.0f;

    // TODO: add road suitability info
};

struct Land
{
    uint32_t vertex_start = INVALID_IDX;
    uint32_t vertex_count = 0;
};

struct GlobalLayout
{
    uint32_t heightmap_seed = 0;
    uint32_t cities_seed = 0;

    float world_size = 0.0f;

    //std::vector<float> heightmap;
    float min_terrain_z = 0.0f;
    float max_terrain_z = 0.0f;
    std::vector<Area> areas;
    std::vector<GlobalCity> cities;
    std::vector<Road> roads;

    std::vector<LandVertex> land_vertices;
    std::vector<LandEdge> land_edges;
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> land_edge_map;
    std::vector<uint32_t> land_poly_vertex_idxs;
    std::vector<Land> lands;
};

class GlobalLayoutGenerator
{
public:
    GlobalLayoutGenerator();

    void Setup(const MapConfig& cfg);
    void Generate();

    const std::shared_ptr<GlobalLayout>& GetGlobalLayout() const { return global_; }

private:
    void InitGlobalLayout();
    void GenerateHeightmap();
    
    void GenerateLandGraph();
    void GenerateLandPolys();
    void RemoveUselessRegionVerticesAndEdges();
    void GenerateLandVertexEdgeInfo();

    void GenerateCities();
    void GenerateRoads();

private:
    MapConfig cfg_{};
    std::mt19937 rng_;
    std::shared_ptr<GlobalLayout> global_;


};

} // namespace mg