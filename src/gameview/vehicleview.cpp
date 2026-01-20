#include "vehicleview.hpp"

#include "assets/cache.hpp"
#include "net/utils.hpp"
#include "worldview.hpp"

#include <iostream>

game::view::VehicleView::VehicleView(WorldView& world, std::shared_ptr<const assets::VehicleModel> model, const glm::vec3& color)
    : EntityView(world), model_(std::move(model))
{
    auto& modelwheels = model_->GetWheels();
    wheels_.resize(modelwheels.size());

    for (size_t i = 0; i < wheels_.size(); ++i)
    {
        wheels_[i].node.parent = &root_;
    }

    color_ = glm::vec4(color, 1.0f);

    snd_accel_ = assets::CacheManager::GetSound("data/auto.snd");

    // sync state
    net::DecodePosition(sync_.pos, root_.local.position);
    net::DecodeRotation(sync_.rot, root_.local.rotation);

}

std::unique_ptr<game::view::VehicleView> game::view::VehicleView::InitFromMsg(WorldView& world, net::InMessage& msg)
{
    net::ModelName modelname;
    glm::vec3 color;
    if (!msg.Read(modelname) || !net::ReadRGB(msg, color))
        return nullptr;

    auto model = assets::CacheManager::GetVehicleModel("data/" + std::string(modelname) + ".veh");

    auto vehicle = std::make_unique<VehicleView>(world, std::move(model), color);
    if (!vehicle->ReadState(msg))
        return nullptr;

    vehicle->root_trans_[0] = vehicle->root_trans_[1];

    return vehicle;
}

bool game::view::VehicleView::ProcessMsg(net::EntMsgType type, net::InMessage& msg)
{
    switch (type)
    {
    case net::EMSG_UPDATE:
        return ProcessUpdateMsg(msg);

    default:
        return false;
    }
}

void game::view::VehicleView::Update(const UpdateInfo& info)
{
    Super::Update(info);

    float tps = 25.0f;
    float t = (info.time - update_time_) * tps * 0.8f; // assume some jitter, interpolate for longer
    t = glm::clamp(t, 0.0f, 2.0f);
    root_.local = Transform::Lerp(root_trans_[0], root_trans_[1], t);

    root_.UpdateMatrix();

    const auto& wheels = model_->GetWheels();
    for (size_t i = 0; i < wheels.size(); ++i)
    {
        // update wheel transform
        auto& wheelstate = wheels_[i];
        auto& wheeltrans = wheelstate.node.local;
        wheeltrans.position = wheels[i].position;
        wheeltrans.position.z += wheelstate.z_offset;
        
        // rotate
        wheelstate.rotation += info.delta_time * wheelstate.speed;

        wheeltrans.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);        
        wheeltrans.rotation = glm::rotate(wheeltrans.rotation, wheelstate.steering, glm::vec3(0, 0, 1));
        wheeltrans.rotation = glm::rotate(wheeltrans.rotation, wheelstate.rotation, glm::vec3(1, 0, 0));
        
        const auto& type = wheels[i].type;
        if (type == assets::WHEEL_FL || type == assets::WHEEL_RL)
            wheeltrans.rotation = glm::rotate(wheeltrans.rotation, glm::radians(180.0f), glm::vec3(0, 1, 0));
        
        wheels_[i].node.UpdateMatrix();
    }

    // update snds
    bool accel = flags_ & VF_ACCELERATING;

    if (accel && !snd_accel_src_)
    {
        snd_accel_src_ = audioplayer_.PlaySound(snd_accel_, &root_.local.position);
        snd_accel_src_->SetLooping(true);
    }
    else if (!accel && snd_accel_src_)
    {
        snd_accel_src_->Delete();
        snd_accel_src_ = nullptr;
    }
}

void game::view::VehicleView::Draw(gfx::DrawList& dlist)
{
    // TOOD: chceck and fix
    const auto& model = *model_->GetModel();
    const auto& mesh = *model.GetMesh();

    for (const auto& surface : mesh.surfaces)
    {
        gfx::DrawSurfaceCmd cmd;
        cmd.surface = &surface;
        cmd.matrices = &root_.matrix;
        cmd.color = &color_;
        dlist.AddSurface(cmd);
    }

    const auto& wheels = model_->GetWheels();
    for (size_t i = 0; i < wheels.size(); ++i)
    {
        const auto& mesh = *wheels[i].model->GetMesh();

        for (const auto& surface : mesh.surfaces)
        {
            gfx::DrawSurfaceCmd cmd;
            cmd.surface = &surface;
            cmd.matrices = &wheels_[i].node.matrix;
            dlist.AddSurface(cmd);
        }
    }
}

bool game::view::VehicleView::ReadState(net::InMessage& msg)
{
    root_trans_[0] = root_.local;
    auto& root_trans = root_trans_[1];
    update_time_ = world_.GetTime();

    // parse state delta
    VehicleSyncFieldFlags fields;
    if (!msg.Read(fields))
        return false;

    // flags
    if (fields & VSF_FLAGS)
    {
        if (!msg.Read(flags_))
            return false;
    }

    // pos
    if (fields & VSF_POSITION)
    {
        if (!net::ReadDelta(msg, sync_.pos.x) ||
            !net::ReadDelta(msg, sync_.pos.y) ||
            !net::ReadDelta(msg, sync_.pos.z))
            return false;

        net::DecodePosition(sync_.pos, root_trans.position);
    }

    // rot
    if (fields & VSF_ROTATION)
    {
        if (!net::ReadDelta(msg, sync_.rot.x) ||
            !net::ReadDelta(msg, sync_.rot.y) ||
            !net::ReadDelta(msg, sync_.rot.z))
            return false;

        net::DecodeRotation(sync_.rot, root_trans.rotation);
    }

    // steering
    if (fields & VSF_STEERING)
    {
        if (!net::ReadDelta(msg, sync_.steering))
            return false;

    }
        
    float steering = sync_.steering.Decode();

    // wheels
    if (fields & VSF_WHEELS)
    {
        for (size_t i = 0; i < wheels_.size(); ++i)
        {
            if (!net::ReadDelta(msg, sync_.wheels[i].z_offset) ||
                !net::ReadDelta(msg, sync_.wheels[i].speed))
                return false;
        }
    }

    const auto& wheels = model_->GetWheels();
    for (size_t i = 0; i < wheels_.size(); ++i)
    {
        auto& wheel = wheels_[i];
        wheel.z_offset = sync_.wheels[i].z_offset.Decode();
        wheel.speed = sync_.wheels[i].speed.Decode();

        wheel.steering = i < 2 ? steering : 0.0f;
    }

    return true;
}

bool game::view::VehicleView::ProcessUpdateMsg(net::InMessage& msg)
{
    return ReadState(msg);
}
