#pragma once

#include "entityview.hpp"

#include "assets/vehiclemdl.hpp"
#include "game/vehicle_sync.hpp"
#include "game/deform_grid.hpp"
#include "modelview.hpp"
#include "spz_texture.hpp"

#include <chrono>

namespace game::view
{

struct VehicleWheelViewInfo
{
    std::shared_ptr<const ModelView> model;
    glm::vec4 color{};

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

enum VehicleColorSlot
{
    VCS_PRIMARY,
    VCS_SECONDARY,

    VCS_HEADLIGHTS,
    VCS_REAR_LIGHTS,
    VCS_BRAKING_LIGHTS,
    VCS_ORANGE_LIGHTS,
    VCS_REVERSE_LIGHT,

    VCS_OTHER,
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
    bool ProcessDeformSyncMsg(net::InMessage& msg);

    void InitLights();

    void UpdateSounds();
    void UpdateWindows();
    void UpdateLights(float delta_t);

    void UpdateDestroyedColors();

    void DrawBaseModel(const DrawArgs& args) const;
    void DrawWheels(const DrawArgs& args) const;
    void DrawLights(const DrawArgs& args) const;

private:
    std::shared_ptr<const assets::VehicleModel> model_;
    std::shared_ptr<const ModelView> model_view_;
    std::vector<gfx::Surface> surfaces_;
    glm::vec4 colors_[SD_MAX_COLORS];
    glm::vec4 destroyed_colors_[SD_MAX_COLORS];
    glm::vec3 headlight_color_;

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

    // lights
    float headlights_factor_ = 0.0f;
    float braking_lights_factor_ = 0.0f;
    float orange_lights_factor_ = 0.0f;
    float reverse_light_factor_ = 0.0f;

    size_t num_headlights_ = 0;
    std::shared_ptr<const ModelView> light_cone_mdl_;
    TransformNode light_cone_node_[2];
    glm::vec3 headlight_pos_[2];
    glm::vec4 headlight_cone_color_;

    size_t num_rearlights_ = 0;
    glm::vec3 rearlight_pos_[2];

    size_t num_brakinglights_ = 0;
    glm::vec3 brakinglights_pos_[2];

    size_t num_reverselights_ = 0;
    glm::vec3 reverselights_pos_[2];

    // spz
    SpzTexture spz_;
};

}