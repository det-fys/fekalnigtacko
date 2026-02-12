#include "characterview.hpp"
#include "assets/cache.hpp"
#include "assets/model.hpp"
#include "net/utils.hpp"
#include "worldview.hpp"

game::view::CharacterView::CharacterView(WorldView& world, net::InMessage& msg) : EntityView(world, msg), ubo_(sk_)
{
    basemodel_ = assets::CacheManager::GetModel("data/human.mdl");
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

    // read initial state
    if (!ReadState(msg))
        throw EntityInitError();

    states_[0] = states_[1]; // lerp from the read state to avoid jump

}

bool game::view::CharacterView::ProcessMsg(net::EntMsgType type, net::InMessage& msg)
{
    switch (type)
    {
    case net::EMSG_UPDATE:
        return ProcessUpdateMsg(msg);

    default:
        return Super::ProcessMsg(type, msg);
    }
}

void game::view::CharacterView::Update(const UpdateInfo& info)
{
    // interpolate states
    float tps = 25.0f;
    float t = (info.time - update_time_) * tps * 0.8f; // assume some jitter, interpolate for longer
    t = glm::clamp(t, 0.0f, 2.0f);

    root_.local = Transform::Lerp(states_[0].trans, states_[1].trans, t);
    animstate_.loco_blend = glm::mix(states_[0].loco_blend, states_[1].loco_blend, t);
    animstate_.loco_phase = glm::mod(glm::mix(states_[0].loco_phase, states_[1].loco_phase, t), 1.0f);

    animstate_.ApplyToSkeleton(sk_);

    root_.UpdateMatrix();
    sk_.UpdateBoneMatrices();
    ubo_valid_ = false;
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
    //{
    //     if (!bone_node.parent)
    //         continue;

    //    glm::vec3 p0 = bone_node.parent->matrix[3];
    //    glm::vec3 p1 = bone_node.matrix[3];

    //    args.dlist.AddBeam(p0, p1, 0xFF00EEEE, 0.01f);
    //}

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
}

bool game::view::CharacterView::ReadState(net::InMessage& msg)
{
    update_time_ = world_.GetTime();

    // init lerp start state
    states_[0].trans = root_.local;
    states_[0].loco_blend = animstate_.loco_blend;
    states_[0].loco_phase = animstate_.loco_phase;

    auto& new_state = states_[1];

    // parse state delta
    CharacterSyncFieldFlags fields;
    if (!msg.Read(fields))
        return false;

    // transform
    if (fields & CSF_TRANSFORM)
    {
        if (!net::ReadDelta(msg, sync_.pos.x) || !net::ReadDelta(msg, sync_.pos.y) ||
            !net::ReadDelta(msg, sync_.pos.z) || !net::ReadDelta(msg, sync_.yaw))
            return false;

        net::DecodePosition(sync_.pos, new_state.trans.position);
        new_state.trans.rotation = glm::rotate(glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                               sync_.yaw.Decode() + glm::pi<float>() * 0.5f, glm::vec3(0, 0, 1));
    }

    if (fields & CSF_IDLE_ANIM)
    {
        if (!msg.Read(sync_.idle_anim))
            return false;

        animstate_.idle_anim_idx = sync_.idle_anim;
    }

    if (fields & CSF_LOCO_ANIMS)
    {
        if (!msg.Read(sync_.walk_anim) || !msg.Read(sync_.run_anim))
            return false;

        animstate_.walk_anim_idx = sync_.walk_anim;
        animstate_.run_anim_idx = sync_.run_anim;
    }

    if (fields & CSF_LOCO_VALS)
    {
        if (!net::ReadDelta(msg, sync_.loco_blend) || !net::ReadDelta(msg, sync_.loco_phase))
            return false;

        new_state.loco_blend = sync_.loco_blend.Decode();
        new_state.loco_phase = sync_.loco_phase.Decode();

        if (new_state.loco_phase < states_[0].loco_phase)
            states_[0].loco_phase -= 1.0f;
    }

    return true;
}

bool game::view::CharacterView::ProcessUpdateMsg(net::InMessage& msg)
{
    return ReadState(msg);
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
        c.model = assets::CacheManager::GetModel("data/tshirt.mdl");
        c.surfacemask = GetSurfaceMask("upperbody");
    }
    else if (name == "shorts")
    {
        c.model = assets::CacheManager::GetModel("data/shorts.mdl");
        c.surfacemask = GetSurfaceMask("upperlegs");
    }
    else
    {
        return; // unknown??
    }

    clothes_.emplace_back(std::move(c));
}
