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

enum MapModelSpecial
{
    MMS_NONE,
    MMS_NIGHTLIGHT,
};

struct MapModel
{
    std::shared_ptr<const ModelView> model;

    // special effects
    MapModelSpecial special = MMS_NONE;
    size_t update_frame = 0;
    std::array<glm::vec4, 8> colors;

    // special - night light
    bool light_on = false;
    gfx::LightData light{};
};

class MapInstanceView
{
public:
    MapInstanceView(collision::DynamicsWorld& world, const std::string& map_name);

    void LoadNext();
    bool IsLoaded() const { return loader_.get() == nullptr; }
    int GetLoadingPercent() const;

    float GetChunkSize() const { return (IsLoaded() && map_) ? map_->GetChunkSize() : 1.0f; }

    void Draw(const game::view::DrawArgs& args);
    void Update();

    void EnableObj(net::ObjNum num, bool enable);

    void SetDayTime(float daytime) { daytime_ = daytime; }

private:
    void InitModels();
    void InitModel(MapModel& mapmodel);
    void InitObjsAndCollisions();

    void DrawChunk(const game::view::DrawArgs& args, const assets::Chunk& chunk);
    void DrawObj(const DrawArgs& args, MapModel& mapmodel, const glm::mat4& matrix);

    void UpdateModelSpecial(MapModel& mapmodel);

private:
    collision::DynamicsWorld& world_;
    std::unique_ptr<assets::MapLoader> loader_;
    std::shared_ptr<const assets::Map> map_;

    std::shared_ptr<const ModelView> basemodel_view_;
    std::vector<MapModel> obj_models_;

    std::unique_ptr<MapObjectCollisionView> basemodel_col_;

    std::vector<bool> objs_visible_;
    std::vector<std::unique_ptr<MapObjectCollisionView>> obj_cols_;

    size_t update_frame_ = 0;
    float daytime_ = 0.0f;
};



}