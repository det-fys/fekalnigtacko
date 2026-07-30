#pragma once

#include "assets/map.hpp"
#include "assets/map_navmesh.hpp"
#include "collision/dynamicsworld.hpp"
#include "net/defs.hpp"
#include "collision/object_info.hpp"

namespace game
{

using MapObjectCollisionFlags = uint32_t;

enum MapObjectCollisionFlag : MapObjectCollisionFlags
{
    MAPOBJ_DESTRUCTIBLE = 0x01,
};

struct MapObjectBreakInfo
{
    float impulse;
    glm::vec3 from_pos;
    glm::vec3 hit_pos;
};

class MapObjectCollision : public collision::ObjectCallback
{
public:
    MapObjectCollision(collision::DynamicsWorld& world, std::shared_ptr<const assets::Model> model,
                       net::ObjNum num, const Transform& trans, MapObjectCollisionFlags flags);
    DELETE_COPY_MOVE(MapObjectCollision)

    void Break(const MapObjectBreakInfo& info);

    void GetModelTransform(Transform& trans) const;
    
    const std::shared_ptr<const assets::Model>& GetModel() const { return model_; }
    btRigidBody& GetBtBody() { return *body_; }
    net::ObjNum GetNum() const { return num_; }
    float GetDestroyThreshold() const { return destr_th_; }

    bool no_projectile_collision_ = false;
    
    virtual ~MapObjectCollision() override;

private:
    collision::DynamicsWorld& world_;
    std::shared_ptr<const assets::Model> model_;
    net::ObjNum num_;
    std::unique_ptr<btRigidBody> body_;
    float destr_th_ = 1.0f;
};

class MapInstance
{
public:
    MapInstance(collision::DynamicsWorld& world, std::string mapname);

    const assets::Map& GetMap() const { return *map_; }
    const assets::MapNavMeshSet& GetNavMeshSet() const { return *navmesh_set_; }

    const std::string& GetName() const { return mapname_; }

    void SpawnObj(net::ObjNum objnum);
    std::unique_ptr<MapObjectCollision> DestroyObj(net::ObjNum objnum, const MapObjectBreakInfo& info);

private:

private:
    collision::DynamicsWorld& world_;
    std::string mapname_;
    std::shared_ptr<const assets::Map> map_;
    std::shared_ptr<const assets::MapNavMeshSet> navmesh_set_;

    std::unique_ptr<MapObjectCollision> basemodel_col_;
    std::vector<std::unique_ptr<MapObjectCollision>> obj_cols_;

};


}