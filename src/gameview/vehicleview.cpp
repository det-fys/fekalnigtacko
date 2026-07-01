#include "vehicleview.hpp"

#include "assets/asset_manager.hpp"
#include "net/utils.hpp"
#include "worldview.hpp"
#include "utils/random.hpp"
#include "utils/math.hpp"

#include <iostream>
#include <ranges>

game::view::VehicleView::VehicleView(WorldView& world, net::InMessage& msg)
    : EntityView(world, msg)
{
    net::ModelName modelname;
    if (!msg.Read(modelname))
        throw EntityInitError();

    model_ = assets::AssetManager::GetInstance().Get<assets::VehicleModel>(std::string(modelname));
    mesh_ = *model_->GetModel()->GetMesh();
    InitMesh();
    InitHeadlights();
    
    auto& modelwheels = model_->GetWheels();
    wheels_.resize(modelwheels.size());
    
    for (size_t i = 0; i < wheels_.size(); ++i)
    {
        wheels_[i].node.parent = &root_;
    }
    
    if (!ReadTuning(msg) || !ReadState(&msg) || !ReadDeformSync(msg))
        throw EntityInitError();
    
    // init the other transform to identical
    root_trans_[0] = root_trans_[1];
    root_.local = root_trans_[0];

    snd_accel_ = assets::AssetManager::GetInstance().Get<audio::Sound>("auto");

    radius_ = 3.0f;
    
    colors_[VCS_OTHER] = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
}

bool game::view::VehicleView::ProcessMsg(net::EntMsgType type, net::InMessage& msg)
{
    switch (type)
    {
    case net::EMSG_DEFORM:
        return ProcessDeformMsg(msg);
    case net::EMSG_DEFORM_SYNC:
        return ProcessDeformSyncMsg(msg);
    case net::EMSG_TUNING:
        return ReadTuning(msg);
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

    UpdateSounds();
    UpdateWindows();
    UpdateLights(info.delta_time);
}

void game::view::VehicleView::Draw(const DrawArgs& args)
{
    Super::Draw(args);

    bool exploded = (flags_ & VF_EXPLODED) > 0;
    
    // base model
    const glm::vec4* colors = exploded ? &destroyed_colors_[0] : &colors_[0]; 
    for (const auto& surface : mesh_.surfaces)
    {
        gfx::DrawSurfaceCmd cmd;
        cmd.surface = &surface;
        cmd.matrices = &root_.matrix;
        cmd.color = colors;
        cmd.num_colors = SD_MAX_COLORS;
        args.dlist.AddSurface(cmd);
    }

    // wheels
    if ((flags_ & VF_NO_WHEELS) == 0)
    {
        const auto& wheels = model_->GetWheels();
        for (size_t i = 0; i < wheels.size(); ++i)
        {
            const auto& mesh = *wheels_[i].model->GetMesh();
    
            for (const auto& surface : mesh.surfaces)
            {
                gfx::DrawSurfaceCmd cmd;
                cmd.surface = &surface;
                cmd.matrices = &wheels_[i].node.matrix;
                cmd.color = &wheels_[i].color;
                cmd.num_colors = 1;
                args.dlist.AddSurface(cmd);
            }
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

    // headlights
    if (!exploded && headlights_factor_ >= 0.01f)
    {
        // light
        auto light_pos = world_.CameraSweep(root_.GetGlobalPosition(), root_.matrix * glm::vec4(0.0f, 7.0f, 0.0f, 1.0f));
        args.dlist.AddLight(light_pos, headlight_color_ * headlights_factor_, 5.0f);

        // cones
        for (size_t i = 0; i < num_headlights; ++i)
        {
            auto& cone_surfaces = light_cone_mdl_->GetMesh()->surfaces;
            
            for (const auto& surface : cone_surfaces)
            {
                gfx::DrawSurfaceCmd cmd;
                cmd.surface = &surface;
                cmd.matrices = &light_cone_node_[i].matrix;
                cmd.color = &headlight_cone_color_;
                args.dlist.AddSurface(cmd);
            }
        }
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

bool game::view::VehicleView::ReadTuning(net::InMessage& msg)
{
    glm::vec4 recv_colors[4];

    // read colors
    for (size_t i = 0; i < 4; ++i)
    {
        uint32_t color;

        if (!net::ReadRGB(msg, color))
            return false;

        recv_colors[i] = glm::unpackUnorm4x8(color);
    }

    // read wheel models
    for (size_t i = 0; i < wheels_.size(); ++i)
    {
        net::ModelName wheelmodel_fixed;

        if (!msg.Read(wheelmodel_fixed))
            return false;

        std::string wheel_model_name = wheelmodel_fixed;

        wheels_[i].model = !wheel_model_name.empty()
                               ? assets::AssetManager::GetInstance().Get<assets::Model>(wheel_model_name)
                               : model_->GetWheels()[i].model;
        wheels_[i].color = recv_colors[1]; // TODO: dynamic?;
        wheels_[i].color.a = 0.0f;
    }

    colors_[VCS_PRIMARY] = recv_colors[0]; // primary
    colors_[VCS_SECONDARY] =  recv_colors[2]; // secondary
    headlight_color_ = recv_colors[3];

    UpdateDestroyedColors();

    return true;
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

bool game::view::VehicleView::ProcessDeformSyncMsg(net::InMessage& msg)
{
    return ReadDeformSync(msg);
}

void game::view::VehicleView::InitHeadlights()
{
    for (size_t i = 0; i < 2; ++i)
    {
        std::string loc_name = std::string("headlight") + static_cast<char>('0' + i);
        auto loc = model_->GetLocation(loc_name);

        if (!loc)
            break;

        if (!light_cone_mdl_)
        {
            light_cone_mdl_ = assets::AssetManager::GetInstance().Get<assets::Model>("headlightcone");
        }

        light_cone_node_[i].parent = &root_;
        light_cone_node_[i].local.position = loc->position;

        ++num_headlights;
    }
}

void game::view::VehicleView::UpdateSounds()
{
    if (!world_.IsLoaded())
        return;

    bool accel = flags_ & VF_ACCELERATING;

    if (accel && !snd_accel_src_)
    {
        snd_accel_src_ = audioplayer_.PlaySound(snd_accel_, &root_);
        snd_accel_src_->SetLooping(true);
    }
    else if (!accel && snd_accel_src_)
    {
        snd_accel_src_->Delete();
        snd_accel_src_ = nullptr;
    }
}

void game::view::VehicleView::UpdateWindows()
{
    if ((flags_ & VF_BROKENWINDOWS) && !windows_broken_)
    {
        windows_broken_ = true;

        auto it = mesh_.surface_names.find("carwindows"); 
        if (it != mesh_.surface_names.end())
        {
            size_t idx = it->second;
            mesh_.surfaces[idx].texture =
                assets::AssetManager::GetInstance().Get<gfx::Texture>("carbrokenwindows");
        }
    }
}

void game::view::VehicleView::UpdateLights(float delta_t)
{
    float max_delta = delta_t * 10.0f;

    MoveToward(headlights_factor_, (flags_ & VF_LIGHTS_ON) ? 1.0f : 0.0f, max_delta);
    MoveToward(braking_lights_factor_, (flags_ & VF_BRAKING) ? 1.0f : 0.0f, max_delta);
    MoveToward(orange_lights_factor_, (flags_ & VF_ORANGE_LIGHTS_ON) ? 1.0f : 0.0f, max_delta);
    MoveToward(reverse_light_factor_, (flags_ & VF_REVERSING) ? 1.0f : 0.0f, max_delta);

    colors_[VCS_HEADLIGHTS] = glm::vec4(headlight_color_, headlights_factor_);
    colors_[VCS_REAR_LIGHTS] = glm::vec4(1.0f, 1.0f, 1.0f, headlights_factor_ * 0.5f + braking_lights_factor_ * 1.0f);
    colors_[VCS_BRAKING_LIGHTS] = glm::vec4(1.0f, 1.0f, 1.0f, braking_lights_factor_);
    colors_[VCS_ORANGE_LIGHTS] = glm::vec4(1.0f, 1.0f, 1.0f, orange_lights_factor_);
    colors_[VCS_REVERSE_LIGHT] = glm::vec4(1.0f, 1.0f, 1.0f, reverse_light_factor_);

    if (headlights_factor_ < 0.01f)
        return;

    float intensity = headlights_factor_ * 0.3f;
    headlight_cone_color_ = glm::vec4(headlight_color_ * intensity, 1.0f);

    for (size_t i = 0; i < num_headlights; ++i)
    {
        light_cone_node_[i].UpdateMatrix();
    }
}

void game::view::VehicleView::UpdateDestroyedColors()
{
    destroyed_colors_[VCS_PRIMARY] = glm::mix(colors_[VCS_PRIMARY], glm::vec4(0.1f, 0.1f, 0.1f, 0.0f), 0.8f);
    destroyed_colors_[VCS_SECONDARY] = glm::mix(colors_[VCS_SECONDARY], glm::vec4(0.1f, 0.1f, 0.1f, 0.0f), 0.8f);

    destroyed_colors_[VCS_HEADLIGHTS] = glm::vec4(0.1f, 0.1f, 0.1f, 0.0f);
    destroyed_colors_[VCS_REAR_LIGHTS] = glm::vec4(0.1f, 0.1f, 0.1f, 0.0f);
    destroyed_colors_[VCS_BRAKING_LIGHTS] = glm::vec4(0.1f, 0.1f, 0.1f, 0.0f);
    destroyed_colors_[VCS_ORANGE_LIGHTS] = glm::vec4(0.1f, 0.1f, 0.1f, 0.0f);
    destroyed_colors_[VCS_REVERSE_LIGHT] = glm::vec4(0.1f, 0.1f, 0.1f, 0.0f);

    destroyed_colors_[VCS_OTHER] = glm::vec4(0.2f, 0.2f, 0.2f, 0.0f);

}
