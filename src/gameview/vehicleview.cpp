#include "vehicleview.hpp"

#include "assets/cache.hpp"
#include "net/utils.hpp"
#include "worldview.hpp"

#include <iostream>

game::view::VehicleView::VehicleView(WorldView& world, net::InMessage& msg)
    : EntityView(world, msg)
{
    net::ModelName modelname;
    glm::vec3 color;
    if (!msg.Read(modelname) || !net::ReadRGB(msg, color))
        throw EntityInitError();

    model_ = assets::CacheManager::GetVehicleModel("data/" + std::string(modelname) + ".veh");
    mesh_ = *model_->GetModel()->GetMesh();
    
    auto& modelwheels = model_->GetWheels();
    wheels_.resize(modelwheels.size());
    
    for (size_t i = 0; i < wheels_.size(); ++i)
    {
        wheels_[i].node.parent = &root_;
    }
    
    color_ = glm::vec4(color, 1.0f);
    
    if (!ReadState(msg))
        throw EntityInitError();
    
    // init the other transform to identical
    root_trans_[0] = root_trans_[1];
    
    snd_accel_ = assets::CacheManager::GetSound("data/auto.snd");
    
    // sync state
    net::DecodePosition(sync_.pos, root_.local.position);
    net::DecodeRotation(sync_.rot, root_.local.rotation);


    radius_ = 3.0f;
}

bool game::view::VehicleView::ProcessMsg(net::EntMsgType type, net::InMessage& msg)
{
    switch (type)
    {
    case net::EMSG_UPDATE:
        return ProcessUpdateMsg(msg);

    default:
        return Super::ProcessMsg(type, msg);
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

    // update windows
    if ((flags_ & VF_BROKENWINDOWS) && !windows_broken_)
    {
        windows_broken_ = true;

        auto it = mesh_.surface_names.find("carwindows"); 
        if (it != mesh_.surface_names.end())
        {
            size_t idx = it->second;
            mesh_.surfaces[idx].texture = assets::CacheManager::GetTexture("data/carbrokenwindows.png");
        }
    }
}

void game::view::VehicleView::Draw(const DrawArgs& args)
{
    Super::Draw(args);

    for (const auto& surface : mesh_.surfaces)
    {
        gfx::DrawSurfaceCmd cmd;
        cmd.surface = &surface;
        cmd.matrices = &root_.matrix;
        cmd.color = &color_;
        args.dlist.AddSurface(cmd);
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
            args.dlist.AddSurface(cmd);
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
