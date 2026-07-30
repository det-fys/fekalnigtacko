#pragma once

#include <string>
#include <functional>

#include "map.hpp"

#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>

namespace assets
{

struct MapNavMeshConfig
{
    std::string id;
    float agent_radius;
    float agent_climb;
    float agent_height;
};

struct NavMeshDebugLine
{
    glm::vec3 start;
    glm::vec3 end;
};

class MapNavMesh
{
public:
    MapNavMesh(std::shared_ptr<const Map> map, const MapNavMeshConfig& cfg);
    DELETE_COPY_MOVE(MapNavMesh);

    bool FindPath(const glm::vec3& start, const glm::vec3& end, std::function<void(const glm::vec3&)> waypoint_cb) const;

    void DrawDebug(const glm::vec3& pos, std::vector<NavMeshDebugLine>& lines) const;

    ~MapNavMesh();

private:
    void Build();
    void Load();
    std::vector<uint8_t> BuildTile(int x, int y);

    void AddTileTris(const AABB3& aabb, std::vector<float>& verts, std::vector<int>& tris);
    void AddModelTris(const Model& model, const Transform& trans, uint32_t first, uint32_t count,
                      std::vector<float>& verts, std::vector<int>& tris);

private:
    float cs_ = 1.0f;
    float tile_size_ = 64.0f;

    std::shared_ptr<const Map> map_;
    const MapNavMeshConfig cfg_;
    std::string save_name_;

    dtNavMesh* navmesh_ = nullptr;
    dtNavMeshQuery* query_ = nullptr;
    

};

class MapNavMeshSet : public Asset
{
public:
    MapNavMeshSet(std::shared_ptr<const Map> map);
    static std::shared_ptr<MapNavMeshSet> Load(const std::string& name);

    const MapNavMesh& GetPawnNavMesh() const { return navmesh_pawn_; }
    const MapNavMesh& GetVehicleNavMesh() const { return navmesh_vehicle_; }

private:
    std::shared_ptr<const Map> map_;
    MapNavMesh navmesh_pawn_;
    MapNavMesh navmesh_vehicle_;

};


}