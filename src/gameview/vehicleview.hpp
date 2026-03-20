#pragma once

#include "entityview.hpp"

#include "assets/vehiclemdl.hpp"
#include "game/vehicle_sync.hpp"
#include "game/deform_grid.hpp"

#include <chrono>

namespace game::view
{

struct VehicleWheelViewInfo
{
    std::shared_ptr<const assets::Model> model;
    glm::vec4 color;

    TransformNode node;
    float steering = 0.0f;
    float z_offset = 0.0f;
    float speed = 0.0f;
    float rotation = 0.0f;
};

struct VehicleDeformView
{
    DeformGrid grid;
    std::shared_ptr<gfx::DeformTexture> tex;

    VehicleDeformView(const gfx::DeformGridInfo& info) : grid(info), tex(std::make_shared<gfx::DeformTexture>(info)) {}
};

class VehicleView : public EntityView
{
    using Super = EntityView;
public:
    VehicleView(WorldView& world, net::InMessage& msg);
    DELETE_COPY_MOVE(VehicleView)

    virtual bool ProcessMsg(net::EntMsgType type, net::InMessage& msg) override;
    virtual bool ProcessUpdateMsg(net::InMessage* msg) override;
    virtual void Update(const UpdateInfo& info) override;
    virtual void Draw(const DrawArgs& args) override;

private:
    void InitMesh();

    bool ReadTuning(net::InMessage& msg);
    bool ReadState(net::InMessage* msg);

    bool ReadDeformSync(net::InMessage& msg);
    bool ProcessDeformMsg(net::InMessage& msg);

private:
    std::shared_ptr<const assets::VehicleModel> model_;
    assets::Mesh mesh_;
    glm::vec4 color_;

    game::VehicleSyncState sync_;
    std::vector<VehicleWheelViewInfo> wheels_;

    float update_time_ = 0.0f;
    Transform root_trans_[2];

    VehicleFlags flags_ = 0;

    std::shared_ptr<const audio::Sound> snd_accel_;
    audio::SoundSource* snd_accel_src_ = nullptr;

    bool windows_broken_ = false;
    std::unique_ptr<VehicleDeformView> deform_;
    std::vector<std::tuple<glm::vec3, glm::vec3>> debug_deforms_;
};

}