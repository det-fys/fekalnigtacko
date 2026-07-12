#pragma once

#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "assets/map.hpp"
#include "collision/dynamicsworld.hpp"
#include "draw_args.hpp"
#include "net/defs.hpp"
#include "utils/defs.hpp"
#include "modelview.hpp"

namespace game::view
{

class MapObjectCollisionView
{
public:
    MapObjectCollisionView(collision::DynamicsWorld& world, std::shared_ptr<const assets::Model> model, const Transform& trans);
    DELETE_COPY_MOVE(MapObjectCollisionView)

    void SetEnabled(bool enabled);

    ~MapObjectCollisionView();

private:
    collision::DynamicsWorld& world_;
    std::shared_ptr<const assets::Model> model_;
    std::unique_ptr<btRigidBody> body_;
    bool enabled_ = false;
};

class MapInstanceView
{
public:
    MapInstanceView(collision::DynamicsWorld& world, const std::string& map_name);

    void LoadNext();
    bool IsLoaded() const { return loader_.get() == nullptr; }
    int GetLoadingPercent() const;

    float GetChunkSize() const { return (IsLoaded() && map_) ? map_->GetChunkSize() : 1.0f; }

    void Draw(const game::view::DrawArgs& args) const;

    void EnableObj(net::ObjNum num, bool enable);

private:
    void InitModelViews();
    void InitObjsAndCollisions();

    void DrawChunk(const game::view::DrawArgs& args, const assets::Chunk& chunk) const;

private:
    collision::DynamicsWorld& world_;
    std::unique_ptr<assets::MapLoader> loader_;
    std::shared_ptr<const assets::Map> map_;

    std::shared_ptr<const ModelView> basemodel_view_;
    std::vector<std::shared_ptr<const ModelView>> obj_models_view_;

    std::unique_ptr<MapObjectCollisionView> basemodel_col_;

    std::vector<bool> objs_visible_;
    std::vector<std::unique_ptr<MapObjectCollisionView>> obj_cols_;
};



}