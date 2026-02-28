#pragma once

#include "assets/map.hpp"
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

class MapObjectCollision : public collision::ObjectCallback
{
public:
    MapObjectCollision(collision::DynamicsWorld& world, std::shared_ptr<const assets::Model> model,
                       net::ObjNum num, const Transform& trans, MapObjectCollisionFlags flags);
    DELETE_COPY_MOVE(MapObjectCollision)

    void Break();

    void GetModelTransform(Transform& trans) const;
    
    const std::shared_ptr<const assets::Model>& GetModel() const { return model_; }
    btRigidBody& GetBtBody() { return *body_; }
    net::ObjNum GetNum() const { return num_; }

    virtual ~MapObjectCollision() override;

private:
    collision::DynamicsWorld& world_;
    std::shared_ptr<const assets::Model> model_;
    net::ObjNum num_;
    std::unique_ptr<btRigidBody> body_;
};

class MapInstance
{
public:
    MapInstance(collision::DynamicsWorld& world, std::string mapname);

    const assets::Map& GetMap() const { return *map_; }
    const std::string& GetName() const { return mapname_; }

    void SpawnObj(net::ObjNum objnum);
    std::unique_ptr<MapObjectCollision> DestroyObj(net::ObjNum objnum);

private:

private:
    collision::DynamicsWorld& world_;
    std::string mapname_;
    std::shared_ptr<const assets::Map> map_;

    std::unique_ptr<MapObjectCollision> basemodel_col_;
    std::vector<std::unique_ptr<MapObjectCollision>> obj_cols_;

};


}