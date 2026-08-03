#include "map_navmesh.hpp"

#include <iostream>
#include <fstream>
#include <cstring>

#include <DetourNavMeshBuilder.h>

#include "utils/files.hpp"
#include <DetourCommon.h>

static glm::vec3 PointToRc(const glm::vec3& p)
{
    return glm::vec3(p.x, p.z, p.y);
}

static glm::vec3 PointFromRc(const glm::vec3& p)
{
    return glm::vec3(p.x, p.z, p.y);
}

template <typename T>
static void WriteBinary(std::string& data, const T& value)
{
    data.resize(data.size() + sizeof(T));
    std::memcpy(data.data() + data.size() - sizeof(T), &value, sizeof(T));
}

template <typename T>
static bool ReadBinary(std::string_view& data, T& value)
{
    if (sizeof(T) > data.size())
        return false;
    std::memcpy(&value, data.data(), sizeof(T));
    data = data.substr(sizeof(T));
    return true;
}


namespace
{
unsigned int nextPow2(unsigned int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

unsigned int ilog2(unsigned int v)
{
    unsigned int r = (v > 0xffff) << 4;
    v >>= r;
    unsigned int shift = (v > 0xff) << 3;
    v >>= shift;
    r |= shift;
    shift = (v > 0xf) << 2;
    v >>= shift;
    r |= shift;
    shift = (v > 0x3) << 1;
    v >>= shift;
    r |= shift;
    r |= (v >> 1);
    return r;
}
} // namespace

assets::MapNavMesh::MapNavMesh(std::shared_ptr<const Map> map, const MapNavMeshConfig& cfg)
    : map_(std::move(map)), cfg_(cfg)
{
    save_name_ = "data/" + map_->GetAssetName() + "_navmesh_" + cfg_.id + ".bin";
    
    if (!fs::FileExists(save_name_))
    {
        Build();
    }

    Load();
}

bool assets::MapNavMesh::FindPath(const glm::vec3& start, const glm::vec3& end,
                                  std::function<void(const glm::vec3&)> waypoint_cb) const
{
    dtQueryFilter filter;
    filter.setIncludeFlags(0xffff); // Include all walkable polys
    filter.setExcludeFlags(0);

    float extents[3] = {5.0f, 10.0f, 5.0f};

    auto rc_start = PointToRc(start);
    auto rc_end = PointToRc(end);

    // Snap start and end points to the nearest NavMesh polygons
    dtPolyRef start_ref = 0;
    dtPolyRef end_ref = 0;
    float start_pt[3];
    float end_pt[3];

    query_->findNearestPoly(&rc_start[0], extents, &filter, &start_ref, start_pt);
    query_->findNearestPoly(&rc_end[0], extents, &filter, &end_ref, end_pt);

    if (!start_ref || !end_ref)
    {
        return false; // Start or end point is too far from the navmesh
    }

    static const int MAX_POLYS = 1024;
    dtPolyRef poly_path[MAX_POLYS];
    int path_count = 0;

    query_->findPath(start_ref, end_ref, start_pt, end_pt, &filter, poly_path, &path_count, MAX_POLYS);

    if (path_count == 0)
    {
        return false; // No path possible
    }

    static const int MAX_WAYPOINTS = 1024;
    float waypoints[MAX_WAYPOINTS * 3];
    unsigned char waypoint_flags[MAX_WAYPOINTS]; 
    dtPolyRef waypoint_polys[MAX_WAYPOINTS];
    int waypoint_count = 0; 

    float end_pt_closest[3];
    dtVcopy(end_pt_closest, end_pt);
    if (poly_path[path_count - 1] != end_ref)
    {
        query_->closestPointOnPoly(poly_path[path_count - 1], end_pt, end_pt_closest, 0);
    }

    query_->findStraightPath(start_pt, end_pt_closest, poly_path, path_count, waypoints, waypoint_flags, waypoint_polys,
                             &waypoint_count, MAX_WAYPOINTS);

    for (int i = 0; i < waypoint_count; ++i)
    {
        waypoint_cb(PointFromRc(glm::vec3(waypoints[i * 3], waypoints[i * 3 + 1], waypoints[i * 3 + 2])));
    }

    return true;
}

void assets::MapNavMesh::DrawDebug(const glm::vec3& pos, std::vector<NavMeshDebugLine>& lines) const
{
    dtPolyRef poly_ref = 0;
    float extent[3] = {20.0f, 20.0f, 20.0f};

    dtQueryFilter filter{};
    filter.setIncludeFlags(0xffff);
    filter.setExcludeFlags(0);

    auto pos_rc = PointToRc(pos);

    auto status = query_->findNearestPoly(&pos_rc[0], extent, &filter, &poly_ref, nullptr);
    if (dtStatusFailed(status))
    {
        throw std::runtime_error("Failed to find nearest poly");
    }

    if (poly_ref == 0)
    {
        return; // no poly found in proximity
    }

    std::array<dtPolyRef, 256> polys{};
    int num_polys = 0;
    status = query_->findPolysAroundCircle(poly_ref, &pos_rc[0], 30.0f, &filter, polys.data(), nullptr, nullptr, &num_polys, polys.size());
    if (dtStatusFailed(status))
    {
        throw std::runtime_error("Failed to find polys around circle");
    }

    for (int i = 0; i < num_polys; ++i)
    {
        dtPolyRef poly_ref = polys[i];
        const dtMeshTile* tile = nullptr;
        const dtPoly* poly = nullptr;
        navmesh_->getTileAndPolyByRef(poly_ref, &tile, &poly);
        if (!tile || !poly)
        {
            continue;
        }
        for (int j = 0; j < poly->vertCount; ++j)
        {
            int v0_idx = poly->verts[j];
            int v1_idx = poly->verts[(j + 1) % poly->vertCount];
            glm::vec3 v0 = PointFromRc(
                glm::vec3(tile->verts[v0_idx * 3], tile->verts[v0_idx * 3 + 1], tile->verts[v0_idx * 3 + 2]));
            glm::vec3 v1 = PointFromRc(
                glm::vec3(tile->verts[v1_idx * 3], tile->verts[v1_idx * 3 + 1], tile->verts[v1_idx * 3 + 2]));
            lines.push_back({v0, v1});
        }
    }
}

assets::MapNavMesh::~MapNavMesh()
{
    if (query_)
    {
        dtFreeNavMeshQuery(query_);
        query_ = nullptr;
    }

    if (navmesh_)
    {
        dtFreeNavMesh(navmesh_);
        navmesh_ = nullptr;
    }


}

void assets::MapNavMesh::Build()
{
    std::cout << "Building navmesh: " << save_name_ << std::endl;

    std::string save_data;

    auto& map_aabb = map_->GetAABB();

    AABB3 aabb_rc{};
    aabb_rc.AddPoint(PointToRc(map_aabb.min));
    aabb_rc.AddPoint(PointToRc(map_aabb.max));

    int grid_width = 0;
    int grid_height = 0;
    rcCalcGridSize(&aabb_rc.min.x, &aabb_rc.max.x, cs_, &grid_width, &grid_height);

    int tile_size_cells = static_cast<int>(tile_size_ / cs_);

    int num_tiles_x = (grid_width + tile_size_cells - 1) / tile_size_cells;
    int num_tiles_y = (grid_height + tile_size_cells - 1) / tile_size_cells;

    WriteBinary(save_data, num_tiles_x);
    WriteBinary(save_data, num_tiles_y);

    for (int y = 0; y < num_tiles_y; ++y)
    {
        for (int x = 0; x < num_tiles_x; ++x)
        {
            if (x % 10 == 0)
            {
                std::cout << "Building navmesh tile: (" << x << ", " << y << ")" << std::endl;
            }
            auto tile_data = BuildTile(x, y);

            WriteBinary(save_data, static_cast<uint32_t>(tile_data.size()));
            if (!tile_data.empty())
            {
                save_data.insert(save_data.end(), tile_data.begin(), tile_data.end());
            }
        }
    }

    fs::WriteFile(save_name_, save_data);
}

void assets::MapNavMesh::Load()
{
    std::string load_data_str = fs::ReadFileAsString(save_name_);
    std::string_view load_data(load_data_str);
    
    int num_tiles_x = 0;
    int num_tiles_y = 0;

    if (!ReadBinary(load_data, num_tiles_x) || !ReadBinary(load_data, num_tiles_y))
    {
        throw std::runtime_error("Failed to read navmesh tile count");
    }

    int tile_bits = rcMin(static_cast<int>(ilog2(nextPow2(num_tiles_x * num_tiles_y))), 14);
    int poly_bits = 22 - tile_bits;

    dtNavMeshParams params{};
    auto orig_rc = PointToRc(map_->GetAABB().min);
    rcVcopy(params.orig, &orig_rc[0]);
    params.maxPolys = 1 << poly_bits;
    params.maxTiles = 1 << tile_bits;
    params.tileWidth = tile_size_;
    params.tileHeight = tile_size_;

    navmesh_ = dtAllocNavMesh();
    if (!navmesh_)
    {
        throw std::runtime_error("Failed to allocate navmesh");
    }

    auto status = navmesh_->init(&params);
    if (dtStatusFailed(status))
    {
        dtFreeNavMesh(navmesh_);
        navmesh_ = nullptr;
        throw std::runtime_error("Failed to initialize navmesh");
    }

    for (int y = 0; y < num_tiles_y; ++y)
    {
        for (int x = 0; x < num_tiles_x; ++x)
        {
            uint32_t tile_data_size = 0;
            if (!ReadBinary(load_data, tile_data_size))
            {
                dtFreeNavMesh(navmesh_);
                navmesh_ = nullptr;
                throw std::runtime_error("Failed to read navmesh tile data size");
            }
            
            if (tile_data_size == 0)
            {
                continue;
            }

            if (load_data.size() < tile_data_size)
            {
                dtFreeNavMesh(navmesh_);
                navmesh_ = nullptr;
                throw std::runtime_error("Insufficient data for navmesh tile");
            }

            auto tile_data_raw = reinterpret_cast<uint8_t*>(dtAlloc(tile_data_size, DT_ALLOC_PERM));
            std::memcpy(tile_data_raw, load_data.data(), tile_data_size);
            load_data.remove_prefix(tile_data_size);

            dtTileRef tile_ref = 0;
            auto status =
                navmesh_->addTile(tile_data_raw, static_cast<int>(tile_data_size), DT_TILE_FREE_DATA, 0, &tile_ref);
            if (dtStatusFailed(status))
            {
                dtFree(tile_data_raw);
                dtFreeNavMesh(navmesh_);
                navmesh_ = nullptr;
                throw std::runtime_error("Failed to add navmesh tile");
            }
        }
    }

    // create query
    query_ = dtAllocNavMeshQuery();
    if (!query_)
    {
        dtFreeNavMesh(navmesh_);
        navmesh_ = nullptr;
        throw std::runtime_error("Failed to allocate navmesh query");
    }

    status = query_->init(navmesh_, 2048);
    if (dtStatusFailed(status))
    {
        dtFreeNavMeshQuery(query_);
        query_ = nullptr;
        dtFreeNavMesh(navmesh_);
        navmesh_ = nullptr;
        throw std::runtime_error("Failed to initialize navmesh query");
    }
}

std::vector<uint8_t> assets::MapNavMesh::BuildTile(int x, int y)
{
    rcContext build_ctx;

    const auto& map_aabb = map_->GetAABB();
    auto tile_height = map_aabb.max.z - map_aabb.min.z;

    // tile aabb
    AABB3 aabb{};
    aabb.min = map_aabb.min + glm::vec3(x * tile_size_, y * tile_size_, 0.0f);
    aabb.max = map_aabb.min + glm::vec3((x + 1) * tile_size_, (y + 1) * tile_size_, tile_height);

    rcConfig cfg{};
    cfg.cs = cs_;
    cfg.ch = 0.1f;
    cfg.walkableSlopeAngle = 45.0f;
    cfg.walkableHeight = static_cast<int>(std::ceil(cfg_.agent_height / cfg.ch));
    cfg.walkableClimb = static_cast<int>(std::floor(cfg_.agent_climb/ cfg.ch));
    cfg.walkableRadius = static_cast<int>(std::ceil(cfg_.agent_radius / cfg.cs));
    cfg.maxEdgeLen = 24;
    cfg.maxSimplificationError = 1.3f;
    cfg.minRegionArea = 8;
    cfg.mergeRegionArea = 20;
    cfg.maxVertsPerPoly = 6;
    cfg.detailSampleDist = 6.0f;
    cfg.tileSize = static_cast<int>(glm::round(tile_size_ / cfg.cs));
    cfg.borderSize = cfg.walkableRadius + 3;
    cfg.width = cfg.tileSize + cfg.borderSize * 2;
    cfg.height = cfg.tileSize + cfg.borderSize * 2;
    cfg.detailSampleDist = 6.0f;
    cfg.detailSampleMaxError = 1.0f;

    auto border_ws = glm::vec3(cfg.borderSize * cfg.cs, cfg.borderSize * cfg.cs, 0.0f);

    AABB3 aabb_ext{};
    aabb_ext.min = aabb.min - border_ws;
    aabb_ext.max = aabb.max + border_ws;

    // swizzle as [x, z, y] for recast
    AABB3 aabb_rc{};
    aabb_rc.AddPoint(PointToRc(aabb_ext.min));
    aabb_rc.AddPoint(PointToRc(aabb_ext.max));

    rcVcopy(cfg.bmin, &aabb_rc.min[0]);
    rcVcopy(cfg.bmax, &aabb_rc.max[0]);

    auto heightfield = rcAllocHeightfield();
    if (!heightfield)
    {
        throw std::runtime_error("Failed to allocate heightfield");
    }

    if (!rcCreateHeightfield(&build_ctx, *heightfield, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch))
    {
        rcFreeHeightField(heightfield);
        throw std::runtime_error("Failed to create heightfield");
    }

    std::vector<float> verts;
    std::vector<int> tris;
    AddTileTris(aabb_ext, verts, tris);

    std::vector<uint8_t> tri_areas(tris.size() / 3, 0);
    rcMarkWalkableTriangles(&build_ctx, cfg.walkableSlopeAngle, verts.data(), verts.size(), tris.data(), tris.size() / 3, tri_areas.data());

    if (!rcRasterizeTriangles(&build_ctx, verts.data(), verts.size() / 3, tris.data(), tri_areas.data(), tris.size() / 3, *heightfield, cfg.walkableClimb))
    {
        rcFreeHeightField(heightfield);
        throw std::runtime_error("Failed to rasterize triangles");
    }

    rcFilterLowHangingWalkableObstacles(&build_ctx, cfg.walkableClimb, *heightfield);
    rcFilterLedgeSpans(&build_ctx, cfg.walkableHeight, cfg.walkableClimb, *heightfield);
    rcFilterWalkableLowHeightSpans(&build_ctx, cfg.walkableHeight, *heightfield);

    auto compact_heightfield = rcAllocCompactHeightfield();
    if (!compact_heightfield)
    {
        rcFreeHeightField(heightfield);
        throw std::runtime_error("Failed to allocate compact heightfield");
    }

    if (!rcBuildCompactHeightfield(&build_ctx, cfg.walkableHeight, cfg.walkableClimb, *heightfield, *compact_heightfield))
    {
        rcFreeCompactHeightfield(compact_heightfield);
        rcFreeHeightField(heightfield);
        throw std::runtime_error("Failed to build compact heightfield");
    }

    rcFreeHeightField(heightfield);

    if (!rcErodeWalkableArea(&build_ctx, cfg.walkableRadius, *compact_heightfield))
    {
        rcFreeCompactHeightfield(compact_heightfield);
        throw std::runtime_error("Failed to erode walkable area");
    }

    if (!rcBuildDistanceField(&build_ctx, *compact_heightfield))
    {
        rcFreeCompactHeightfield(compact_heightfield);
        throw std::runtime_error("Failed to build distance field");
    }

    if (!rcBuildRegions(&build_ctx, *compact_heightfield, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea))
    {
        rcFreeCompactHeightfield(compact_heightfield);
        throw std::runtime_error("Failed to build regions");
    }

    auto contour_set = rcAllocContourSet();
    if (!contour_set)
    {
        rcFreeCompactHeightfield(compact_heightfield);
        throw std::runtime_error("Failed to allocate contour set");
    }

    if (!rcBuildContours(&build_ctx, *compact_heightfield, cfg.maxSimplificationError, cfg.maxEdgeLen, *contour_set))
    {
        rcFreeContourSet(contour_set);
        rcFreeCompactHeightfield(compact_heightfield);
        throw std::runtime_error("Failed to build contours");
    }

    if (contour_set->nconts == 0)
    {
        rcFreeContourSet(contour_set);
        rcFreeCompactHeightfield(compact_heightfield);
        return {};
    }

    auto poly_mesh = rcAllocPolyMesh();
    if (!poly_mesh)
    {
        rcFreeContourSet(contour_set);
        rcFreeCompactHeightfield(compact_heightfield);
        throw std::runtime_error("Failed to allocate poly mesh");
    }

    if (!rcBuildPolyMesh(&build_ctx, *contour_set, cfg.maxVertsPerPoly, *poly_mesh))
    {
        rcFreePolyMesh(poly_mesh);
        rcFreeContourSet(contour_set);
        rcFreeCompactHeightfield(compact_heightfield);
        throw std::runtime_error("Failed to build poly mesh");
    }

    auto poly_mesh_detail = rcAllocPolyMeshDetail();
    if (!poly_mesh_detail)
    {
        rcFreePolyMesh(poly_mesh);
        rcFreeContourSet(contour_set);
        rcFreeCompactHeightfield(compact_heightfield);
        throw std::runtime_error("Failed to allocate poly mesh detail");
    }

    if (!rcBuildPolyMeshDetail(&build_ctx, *poly_mesh, *compact_heightfield, cfg.detailSampleDist, cfg.detailSampleMaxError, *poly_mesh_detail))
    {
        rcFreePolyMeshDetail(poly_mesh_detail);
        rcFreePolyMesh(poly_mesh);
        rcFreeContourSet(contour_set);
        rcFreeCompactHeightfield(compact_heightfield);
        throw std::runtime_error("Failed to build poly mesh detail");
    }

    for (int i = 0; i < poly_mesh->npolys; ++i)
    {
        poly_mesh->flags[i] = 1; // set all polys to walkable
    }

    uint8_t* navmesh_data = nullptr;
    int navmesh_data_size = 0;

    dtNavMeshCreateParams params{};
    params.verts = poly_mesh->verts;
    params.vertCount = poly_mesh->nverts;
    params.polys = poly_mesh->polys;
    params.polyAreas = poly_mesh->areas;
    params.polyFlags = poly_mesh->flags;
    params.polyCount = poly_mesh->npolys;
    params.nvp = poly_mesh->nvp;
    params.detailMeshes = poly_mesh_detail->meshes;
    params.detailVerts = poly_mesh_detail->verts;
    params.detailVertsCount = poly_mesh_detail->nverts;
    params.detailTris = poly_mesh_detail->tris;
    params.detailTriCount = poly_mesh_detail->ntris;
    params.walkableHeight = cfg.walkableHeight;
    params.walkableRadius = cfg.walkableRadius;
    params.walkableClimb = cfg.walkableClimb;
    rcVcopy(params.bmin, poly_mesh->bmin);
    rcVcopy(params.bmax, poly_mesh->bmax);
    params.cs = poly_mesh->cs;
    params.ch = poly_mesh->ch;
    params.buildBvTree = true;
    params.tileX = x;
    params.tileY = y;

    if (!dtCreateNavMeshData(&params, &navmesh_data, &navmesh_data_size))
    {
        rcFreePolyMeshDetail(poly_mesh_detail);
        rcFreePolyMesh(poly_mesh);
        rcFreeContourSet(contour_set);
        rcFreeCompactHeightfield(compact_heightfield);
        throw std::runtime_error("Failed to create navmesh data");
    }

    rcFreePolyMeshDetail(poly_mesh_detail);
    rcFreePolyMesh(poly_mesh);
    rcFreeContourSet(contour_set);
    rcFreeCompactHeightfield(compact_heightfield);

    std::vector<uint8_t> navmesh_data_vec(navmesh_data, navmesh_data + navmesh_data_size);
    dtFree(navmesh_data);

    return navmesh_data_vec;
}

void assets::MapNavMesh::AddTileTris(const AABB3& aabb, std::vector<float>& verts, std::vector<int>& tris)
{
    static std::vector<uint32_t> chunk_idxs;
    chunk_idxs.clear();

    map_->GetOverlappingChunks(aabb, chunk_idxs);

    const auto& chunks = map_->GetChunks();
    const auto& basemodel = *map_->GetBaseModel();
    const auto& basemodel_surfaces = basemodel.GetSurfaces();

    for (uint32_t chunk_idx : chunk_idxs)
    {
        const auto& chunk = chunks[chunk_idx];

        // add chunk basemodel surfaces
        for (const auto& surface : chunk.surfaces)
        {
            const auto& basemodel_surface = basemodel_surfaces[surface.idx];
            AddModelTris(basemodel, Transform(), basemodel_surface.tri_offset + surface.first, surface.count, verts, tris);
        }

        // add chunk static objects
        for (size_t i = 0; i < chunk.num_objs; ++i)
        {
            const auto& obj = map_->GetStaticObjects()[chunk.first_obj + i];

            if (!aabb.CollidesWith(obj.aabb))
                continue;

            AddModelTris(*map_->GetObjModels()[obj.model_idx], obj.node.local, 0, 0, verts, tris);
        }
    }

}

void assets::MapNavMesh::AddModelTris(const Model& model, const Transform& trans, uint32_t first, uint32_t count,
                                      std::vector<float>& verts, std::vector<int>& tris)
{
    std::map<uint32_t, uint32_t> vert_map;

    const auto& model_verts = model.GetVertices().positions;
    const auto& model_tris = model.GetTriangles();

    glm::mat4 trans_mat = trans.ToMatrix();

    if (count == 0)
    {
        count = model_tris.size() - first;
    }

    for (uint32_t i = first; i < (first + count); ++i)
    {
        const auto& idx = model_tris[i];

        uint32_t vert_indices[3];

        for (int j = 0; j < 3; ++j)
        {
            uint32_t vert_idx = idx.vertices[j];

            auto it = vert_map.find(vert_idx);
            if (it == vert_map.end())
            {
                uint32_t new_idx = verts.size() / 3;
                vert_map[vert_idx] = new_idx;

                glm::vec4 pos = trans_mat * glm::vec4(model_verts[vert_idx], 1.0f);
                auto pos_rc = PointToRc(pos);
                verts.push_back(pos_rc.x);
                verts.push_back(pos_rc.y);
                verts.push_back(pos_rc.z);

                vert_indices[j] = new_idx;
            }
            else
            {
                vert_indices[j] = it->second;
            }
        }

        // change winding
        tris.push_back(vert_indices[0]);
        tris.push_back(vert_indices[2]);
        tris.push_back(vert_indices[1]);
    }

}

static const assets::MapNavMeshConfig pawn_cfg{.id = "pawn", .agent_radius = 0.3f, .agent_climb = 0.2f, .agent_height = 1.8f};
static const assets::MapNavMeshConfig vehicle_cfg{.id = "vehicle", .agent_radius = 1.5f, .agent_climb = 0.3f, .agent_height = 2.0f};

assets::MapNavMeshSet::MapNavMeshSet(std::shared_ptr<const Map> map)
    : map_(std::move(map)), navmesh_pawn_(map_, pawn_cfg), navmesh_vehicle_(map_, vehicle_cfg)
{
}

std::shared_ptr<assets::MapNavMeshSet> assets::MapNavMeshSet::Load(const std::string& name)
{
    return std::make_shared<MapNavMeshSet>(assets::AssetManager::GetInstance().Get<Map>(name));
}
