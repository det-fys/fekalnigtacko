#include "characterview.hpp"
#include "assets/asset_manager.hpp"
#include "assets/model.hpp"
#include "net/utils.hpp"
#include "worldview.hpp"

game::view::CharacterView::CharacterView(WorldView& world, net::InMessage& msg) : EntityView(world, msg), ubo_(sk_)
{
    // read model name
    net::ModelName model_name;
    if (!msg.Read(model_name))
        throw EntityInitError();;

    basemodel_ = assets::AssetManager::GetInstance().Get<assets::Model>(std::string(model_name));
    sk_ = SkeletonInstance(basemodel_->GetSkeleton(), &root_);
    ubo_.Update();
    ubo_valid_ = true;

    // read clothes
    net::NumClothes num_clothes = 0;
    if (!msg.Read(num_clothes))
        throw EntityInitError();

    for (net::NumClothes i = 0; i < num_clothes; ++i)
    {
        net::ClothesName name;
        glm::vec3 color;

        if (!msg.Read(name) || !net::ReadRGB(msg, color))
            throw EntityInitError();

        AddClothes(name, color);
    }

    UpdateSurfaceMask();

    // read item
    net::ModelName item_name;
    if (!msg.Read(item_name))
        throw EntityInitError();

    SetItem(item_name);

    // read initial state
    if (!ReadState(&msg))
        throw EntityInitError();

    OnAttach();

    radius_ = 2.0f;
}

bool game::view::CharacterView::ProcessMsg(net::EntMsgType type, net::InMessage& msg)
{
    switch (type)
    {
    case net::EMSG_EQUIP:
        return ProcessEquipMsg(msg);
    case net::EMSG_FIRE:
        return ProcessFireMsg(msg);
    default:
        return Super::ProcessMsg(type, msg);
    }
}

bool game::view::CharacterView::ProcessUpdateMsg(net::InMessage* msg)
{
    return ReadState(msg);
}

void game::view::CharacterView::Update(const UpdateInfo& info)
{
    Super::Update(info);

    // interpolate states
    float tps = 25.0f;
    float t = (info.time - update_time_) * tps * 0.8f; // assume some jitter, interpolate for longer;
    t = glm::clamp(t, 0.0f, 2.0f);
    float t_sane = glm::clamp(t, 0.0f, 1.0f);

    root_.local = Transform::Lerp(states_[0].trans, states_[1].trans, t);

    // loco
    animstate_.loco_blend = glm::mix(states_[0].loco_blend, states_[1].loco_blend, t);
           
    float loco_phase0 = states_[0].loco_phase;
    float loco_phase1 = states_[1].loco_phase;

    // interpolate across 0-1 wrap using the shortest direction
    // compute signed shortest delta in range [-0.5,0.5)
    float delta = glm::mod(loco_phase1 - loco_phase0 + 0.5f, 1.0f) - 0.5f;
    float interp = loco_phase0 + delta * t;
    animstate_.loco_phase = glm::mod(interp, 1.0f);

    // action
    animstate_.action_time = glm::mix(states_[0].action_time, states_[1].action_time, t_sane);
    // if (animstate_.action_anim_idx != assets::NO_ANIM)
    // {
    //     std::cout <<"phase: " << animstate_.action_phase << std::endl;
    // }

    // aim
    animstate_.yaw = glm::mix(states_[0].aim_yaw, states_[1].aim_yaw, t_sane);
    animstate_.pitch = glm::mix(states_[0].aim_pitch, states_[1].aim_pitch, t_sane);

    animstate_.ApplyToSkeleton(sk_);

    root_.UpdateMatrix();
    sk_.UpdateBoneMatrices();
    ubo_valid_ = false;

    if (item_)
    {
        item_node_.UpdateMatrix();
        fire_snd_node_.UpdateMatrix();
    }
}

void game::view::CharacterView::Draw(const DrawArgs& args)
{
    Super::Draw(args);

    //glm::vec3 start = root_.local.position;
    //glm::vec3 end = start + glm::vec3(0.0f, 0.0f, 1.5f);
    //args.dlist.AddBeam(start, end, 0xFF777777, 0.1f);

    //start = root_.local.position;
    //end = start + glm::vec3(glm::cos(yaw_), glm::sin(yaw_), 0.0f) * 0.5f;
    //args.dlist.AddBeam(start, end, 0xFF007700, 0.05f);

    //// draw bones debug
    // const auto& bone_nodes = sk_.GetBoneNodes();
    // for (const auto& bone_node : bone_nodes)
    // {
    //     if (!bone_node.parent)
    //         continue;

    //    glm::vec3 p0 = bone_node.parent->matrix[3];
    //    glm::vec3 p1 = bone_node.matrix[3];

    //    args.dlist.AddBeam(p0, p1, 0xFF00EEEE, 0.01f);
    // }

    // update skinning matrices
    if (!ubo_valid_)
    {
        ubo_.Update();
        ubo_valid_ = true;
    }

    // draw clothes
    for (const auto& clothes : clothes_)
    {
        const auto& mesh = *clothes.model->GetMesh();
        for (const auto& surface : mesh.surfaces)
        {
            gfx::DrawSurfaceCmd cmd;
            cmd.surface = &surface;
            cmd.matrices = &root_.matrix;
            cmd.skinning = &ubo_;
            cmd.color = &clothes.color;
            args.dlist.AddSurface(cmd);
        }
    }

    // draw basemodel
    const auto& mesh = *basemodel_->GetMesh();
    for (size_t i = 0; i < mesh.surfaces.size(); ++i)
    {
        if (!(surfacemask_ & (1 << i))) // hidden by clothes?
            continue;

        gfx::DrawSurfaceCmd cmd;
        cmd.surface = &mesh.surfaces[i];
        cmd.matrices = &root_.matrix;
        cmd.skinning = &ubo_;
        args.dlist.AddSurface(cmd);
    }

    DrawItem(args);
}

void game::view::CharacterView::OnAttach()
{
    states_[0] = states_[1]; // lerp from the read state to avoid jump

    // init view state
    root_.local = states_[0].trans;
    animstate_.loco_blend = states_[0].loco_blend;
    animstate_.loco_phase = states_[0].loco_phase;
}

bool game::view::CharacterView::ReadState(net::InMessage* msg)
{
    update_time_ = world_.GetTime();
    
    auto& old_state = states_[0];
    auto& new_state = states_[1];

    // init lerp start state
    old_state.trans = root_.local;
    old_state.loco_blend = animstate_.loco_blend;
    old_state.loco_phase = animstate_.loco_phase;
    old_state.action_time = animstate_.action_time;
    old_state.aim_yaw = animstate_.yaw;
    old_state.aim_pitch = animstate_.pitch;

    if (msg)
    {
        // parse state delta
        CharacterSyncFieldFlags fields;
        if (!msg->Read(fields))
            return false;

        // transform
        if (fields & CSF_TRANSFORM)
        {
            if (!net::ReadDelta(*msg, sync_.pos.x) || !net::ReadDelta(*msg, sync_.pos.y) ||
                !net::ReadDelta(*msg, sync_.pos.z) || !net::ReadDelta(*msg, sync_.yaw))
                return false;

            net::DecodePosition(sync_.pos, new_state.trans.position);
            new_state.trans.rotation =
                glm::rotate(glm::quat(1.0f, 0.0f, 0.0f, 0.0f), sync_.yaw.Decode(), glm::vec3(0, 0, 1));
        }

        if (fields & CSF_IDLE_ANIM)
        {
            if (!msg->Read(sync_.idle_anim))
                return false;

            animstate_.idle_anim_idx = sync_.idle_anim;
        }

        if (fields & CSF_LOCO_ANIMS)
        {
            if (!msg->Read(sync_.walk_anim) || !msg->Read(sync_.run_anim))
                return false;

            animstate_.walk_anim_idx = sync_.walk_anim;
            animstate_.run_anim_idx = sync_.run_anim;
        }

        if (fields & CSF_LOCO_VALS)
        {
            if (!net::ReadDelta(*msg, sync_.loco_blend) || !net::ReadDelta(*msg, sync_.loco_phase))
                return false;

            new_state.loco_blend = sync_.loco_blend.Decode();
            new_state.loco_phase = sync_.loco_phase.Decode();
        }

        // action anim
        if (fields & CSF_ACTION_ANIM)
        {
            if (!msg->Read(sync_.action_anim))
                return false;

            animstate_.action_anim_idx = sync_.action_anim;
        }

        // action time
        if (fields & CSF_ACTION_TIME)
        {
            if (!net::ReadDelta(*msg, sync_.action_time))
                return false;

            new_state.action_time = sync_.action_time.Decode();

            if (fields & CSF_ACTION_ANIM)
            {
                // anim just changed, dont blend time
                old_state.action_time = new_state.action_time;
                animstate_.action_time = new_state.action_time;
            }
        }

        // aim
        if (fields & CSF_AIM)
        {
            if (!net::ReadDelta(*msg, sync_.aim_yaw) || !net::ReadDelta(*msg, sync_.aim_pitch))
                return false; 

            new_state.aim_yaw = sync_.aim_yaw.Decode();
            new_state.aim_pitch = sync_.aim_pitch.Decode();
        }

    }

    return true;
}

game::view::CharacterView::SurfaceMask game::view::CharacterView::GetSurfaceMask(const std::string& name)
{
    const auto& surface_names = basemodel_->GetMesh()->surface_names;
    auto it = surface_names.find(name);
    if (it != surface_names.end())
    {
        return 1 << it->second;
    }

    return 0;
}

void game::view::CharacterView::UpdateSurfaceMask()
{
    surfacemask_ = 0xFFFFFFFF;
    for (const auto& clothes : clothes_)
    {
        surfacemask_ &= ~clothes.surfacemask;
    }

}

void game::view::CharacterView::AddClothes(const std::string& name, const glm::vec3& color)
{
    CharacterViewClothes c;
    c.color = glm::vec4(color, 1.0f);

    if (name == "tshirt")
    {
        c.model = assets::AssetManager::GetInstance().Get<assets::Model>("tshirt");
        c.surfacemask = GetSurfaceMask("upperbody");
    }
    else if (name == "shorts")
    {
        c.model = assets::AssetManager::GetInstance().Get<assets::Model>("shorts");
        c.surfacemask = GetSurfaceMask("upperlegs");
    }
    else
    {
        return; // unknown??
    }

    clothes_.emplace_back(std::move(c));
}

bool game::view::CharacterView::ProcessEquipMsg(net::InMessage& msg)
{
    net::ModelName item_name;
    if (!msg.Read(item_name))
        return false;

    SetItem(item_name);

    return true;
}

bool game::view::CharacterView::ProcessFireMsg(net::InMessage& msg)
{
    FireItem();
    return true;
}

void game::view::CharacterView::SetItem(const std::string& item_name)
{
    if (item_name == item_name_)
        return;

    item_name_ = item_name;

    fire_snd_.reset();
    fire_fx_.reset();

    if (item_name.empty())
    {
        item_.reset();
        return;
    }

    item_ = assets::AssetManager::GetInstance().Get<assets::Item>(item_name);

    auto bone_node = sk_.GetBoneNodeByName(item_->bone);
    item_node_.parent = bone_node ? bone_node : &root_;
    item_node_.local = item_->bone_offset;

    fire_snd_node_.parent = &item_node_;
    fire_snd_node_.local.position = glm::vec3(0.0f, 0.1f, 0.0f);

    // snd
    if (!item_->fire_snd.empty())
    {
        fire_snd_ = assets::AssetManager::GetInstance().Get<audio::Sound>(item_->fire_snd);
    }

    // fx
    if (!item_->fire_fx.empty())
    {
        fire_fx_ = assets::AssetManager::GetInstance().Get<assets::Effect>(item_->fire_fx);
        auto loc = item_->model->GetLocation(item_->fire_fx_loc);
        fire_fx_offset_ = loc ? loc->position : glm::vec3(0.0f);
    }
}

void game::view::CharacterView::DrawItem(const DrawArgs& args)
{
    if (!item_ || !item_->model)
        return;

    const auto& mesh = *item_->model->GetMesh();
    for (const auto& surface : mesh.surfaces)
    {
        gfx::DrawSurfaceCmd cmd;
        cmd.surface = &surface;
        cmd.matrices = &item_node_.matrix;
        args.dlist.AddSurface(cmd);
    }
}

void game::view::CharacterView::FireItem()
{
    if (!item_)
        return;

    if (fire_snd_)
    {
        auto snd = audioplayer_.PlaySound(fire_snd_, &fire_snd_node_);
        // snd->SetPosition(item_node_.GetGlobalPosition());
    }

    if (fire_fx_)
    {
        glm::vec3 pos = item_node_.matrix * glm::vec4(fire_fx_offset_, 1.0f);
        glm::vec3 dir = item_node_.matrix * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        world_.GetEmitter().Emit(fire_fx_, pos, glm::normalize(dir));
    }


}
