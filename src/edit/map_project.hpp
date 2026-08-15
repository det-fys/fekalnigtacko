#pragma once

#include "gfx/scene.hpp"

#include "gameview/modelview.hpp"
#include "gameview/mapinstanceview.hpp"
#include "gameview/worldenv.hpp"
#include "collision/dynamicsworld.hpp"

namespace edit
{

using namespace game::view;

class MapProject : public gfx::Scene
{
public:
    MapProject();

    void Update();

    // Scene
    virtual void Draw(const gfx::DrawContext& ctx) override;
    virtual gfx::Environment GetSceneEnvironment() override;
    virtual float GetMapChunkSize() override;

    void SetDayTime(float daytime) { daytime_ = daytime; }

private:
    collision::DynamicsWorld dynamics_world_;
    MapInstanceView map_;
    WorldEnv world_env_;

    float daytime_ = 12.0f; // 0-24

    //std::shared_ptr<const ModelView> test_model_;
};

} // namespace edit
