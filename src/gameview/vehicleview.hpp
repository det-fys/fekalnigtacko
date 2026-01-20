#pragma once

#include "entityview.hpp"

#include "assets/vehiclemdl.hpp"
#include "game/vehicle_sync.hpp"

#include <chrono>

namespace game::view
{

struct VehicleWheelViewInfo
{
    TransformNode node;
    float steering = 0.0f;
    float z_offset = 0.0f;
    float speed = 0.0f;
    float rotation = 0.0f;
};

class VehicleView : public EntityView
{
    using Super = EntityView;
public:
    VehicleView(WorldView& world, std::shared_ptr<const assets::VehicleModel> model, const glm::vec3& color);
    static std::unique_ptr<VehicleView> InitFromMsg(WorldView& world, net::InMessage& msg);

    virtual bool ProcessMsg(net::EntMsgType type, net::InMessage& msg) override;
    virtual void Update(const UpdateInfo& info) override;
    virtual void Draw(gfx::DrawList& dlist) override;

private:
    bool ReadState(net::InMessage& msg);
    bool ProcessUpdateMsg(net::InMessage& msg);

private:
    std::shared_ptr<const assets::VehicleModel> model_;
    glm::vec4 color_;

    game::VehicleSyncState sync_;
    std::vector<VehicleWheelViewInfo> wheels_;

    float update_time_ = 0.0f;
    Transform root_trans_[2];

    VehicleFlags flags_ = 0;

    std::shared_ptr<const audio::Sound> snd_accel_;
    audio::SoundSource* snd_accel_src_ = nullptr;
};

}