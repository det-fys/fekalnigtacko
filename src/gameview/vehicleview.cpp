#include "vehicleview.hpp"

#include "assets/cache.hpp"
#include "net/utils.hpp"
#include "worldview.hpp"
#include "utils/random.hpp"

#include <iostream>
#include <ranges>

game::view::VehicleView::VehicleView(WorldView& world, net::InMessage& msg)
    : EntityView(world, msg)
{
    net::ModelName modelname;
    glm::vec3 color;
    if (!msg.Read(modelname) || !net::ReadRGB(msg, color))
        throw EntityInitError();

    model_ = assets::CacheManager::GetVehicleModel("data/" + std::string(modelname) + ".veh");
    mesh_ = *model_->GetModel()->GetMesh();
    InitMesh();
    
    auto& modelwheels = model_->GetWheels();
    wheels_.resize(modelwheels.size());
    
    for (size_t i = 0; i < wheels_.size(); ++i)
    {
        wheels_[i].node.parent = &root_;
    }
    
    color_ = glm::vec4(color, 1.0f);
    
    if (!ReadState(&msg))
        throw EntityInitError();

    if (!ReadDeformSync(msg))
        throw EntityInitError();

    // init the other transform to identical
    root_trans_[0] = root_trans_[1];
    root_.local = root_trans_[0];

    snd_accel_ = assets::CacheManager::GetSound("data/auto.snd");

    radius_ = 3.0f;
}

bool game::view::VehicleView::ProcessMsg(net::EntMsgType type, net::InMessage& msg)
{
    switch (type)
    {
    case net::EMSG_DEFORM:
        return ProcessDeformMsg(msg);
    default:
        return Super::ProcessMsg(type, msg);
    }
}

bool game::view::VehicleView::ProcessUpdateMsg(net::InMessage* msg)
{
    return ReadState(msg);
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

    // temp deforms
    for (const auto& [pos, deform] : debug_deforms_)
    {
        glm::vec3 start = root_.matrix * glm::vec4(pos, 1.0f);
        glm::vec3 end = root_.matrix * glm::vec4(pos + deform, 1.0f);
        glm::vec3 end2 = end + glm::vec3(0.0f, 0.0f, 0.1f);

        args.dlist.AddBeam(start, end, 0xFFFFFF00, 0.01f);
        args.dlist.AddBeam(end, end2, 0xFFFF00FF, 0.01f);
    }
}

void game::view::VehicleView::InitMesh()
{
    gfx::DeformGridInfo info{};
    info.min = glm::vec3(-1.0f, -2.5f, 0.10f);
    info.max = glm::vec3(1.0f, 2.0f, 1.8f);
    info.res = glm::ivec3(8, 16, 8);
    info.max_offset = 0.1f;
    deform_ = std::make_unique<VehicleDeformView>(info);

    for (auto& surface : mesh_.surfaces)
    {
        surface.deform_tex = deform_->tex;
        surface.sflags |= gfx::SF_DEFORM_GRID;
    }

    // for (size_t i = 0; i < 20; ++i)
    // {
    //     glm::vec3 pos(RandomFloat(-1.0f, 1.0f), RandomFloat(-2.0f, 2.0f), RandomFloat(0.0f, 2.0f));
    //     glm::vec3 impulse(RandomFloat(-1.0f, 1.0f), RandomFloat(-1.0f, 1.0f), RandomFloat(-1.0f, 1.0f));
    //     impulse *= 0.05f;

    //     deform_->grid.ApplyImpulse(pos, impulse, 1.0f);
    // }

    deform_->tex->SetData(deform_->grid.GetData());
}

bool game::view::VehicleView::ReadState(net::InMessage* msg)
{
    root_trans_[0] = root_.local;
    auto& root_trans = root_trans_[1];
    update_time_ = world_.GetTime();

    if (msg)
    {
        // parse state delta
        VehicleSyncFieldFlags fields;
        if (!msg->Read(fields))
            return false;

        // flags
        if (fields & VSF_FLAGS)
        {
            if (!msg->Read(flags_))
                return false;
        }

        // pos
        if (fields & VSF_POSITION)
        {
            if (!net::ReadDelta(*msg, sync_.pos.x) ||
                !net::ReadDelta(*msg, sync_.pos.y) ||
                !net::ReadDelta(*msg, sync_.pos.z))
                return false;

            net::DecodePosition(sync_.pos, root_trans.position);
        }

        // rot
        if (fields & VSF_ROTATION)
        {
            if (!net::ReadDelta(*msg, sync_.rot.x) ||
                !net::ReadDelta(*msg, sync_.rot.y) ||
                !net::ReadDelta(*msg, sync_.rot.z))
                return false;

            net::DecodeRotation(sync_.rot, root_trans.rotation);
        }

        // steering
        if (fields & VSF_STEERING)
        {
            if (!net::ReadDelta(*msg, sync_.steering))
                return false;

        }

        float steering = sync_.steering.Decode();

        // wheels
        if (fields & VSF_WHEELS)
        {
            for (size_t i = 0; i < wheels_.size(); ++i)
            {
                if (!net::ReadDelta(*msg, sync_.wheels[i].z_offset) ||
                    !net::ReadDelta(*msg, sync_.wheels[i].speed))
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
    }
    return true;
}

bool game::view::VehicleView::ReadDeformSync(net::InMessage& msg)
{
    net::NumTexels numtexels;
    if (!msg.Read(numtexels))
        return false;

    auto texels = deform_->grid.GetData();
    std::ranges::fill(texels, glm::i8vec3(0));

    size_t current = 0;
    for (size_t i = 0; i < numtexels; ++i)
    {
        int64_t diff;
        if (!msg.ReadVarInt(diff))
            return false;

        current += static_cast<size_t>(diff);

        if (current >= texels.size())
            return false;

        auto& texel = texels[current];
        for (size_t j = 0; j < 3; ++j)
        {
            if (!msg.Read(texel[j]))
                return false;
        }
    }

    deform_->tex->SetData(deform_->grid.GetData());

    return true;
}

bool game::view::VehicleView::ProcessDeformMsg(net::InMessage& msg)
{
    net::PositionQ pos_q, deform_q;
    if (!net::ReadPositionQ(msg, pos_q) || !net::ReadPositionQ(msg, deform_q))
        return false;


    glm::vec3 pos, deform;
    net::DecodePosition(pos_q, pos);
    net::DecodePosition(deform_q, deform);

    deform_->grid.ApplyImpulse(pos, deform, 0.3f);
    deform_->tex->SetData(deform_->grid.GetData());

    //debug_deforms_.emplace_back(std::make_tuple(pos, deform));

    return true;


}
