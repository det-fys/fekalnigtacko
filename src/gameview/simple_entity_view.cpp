#include "simple_entity_view.hpp"
#include "net/defs.hpp"
#include "assets/asset_manager.hpp"
#include "worldview.hpp"
#include "net/utils.hpp"

game::view::SimpleEntityView::SimpleEntityView(WorldView& world, net::InMessage& msg) : Super(world, msg)
{
    net::ModelName modelname;
    if (!msg.Read(modelname))
        throw EntityInitError();

    if (modelname.len > 0)
    {
        model_ = assets::AssetManager::GetInstance().Get<ModelView>(std::string(modelname));
        if (!model_)
            throw EntityInitError();
    }

    if (!ReadState(&msg))
        throw EntityInitError();

    states_[0] = states_[1]; // lerp from the read state to avoid jump
    root_.local = states_[0].trans;

    radius_ = 20.0f;
}

bool game::view::SimpleEntityView::ProcessMsg(net::EntMsgType type, net::InMessage& msg)
{
    switch (type)
    {
    default:
        return Super::ProcessMsg(type, msg);
    }
}

bool game::view::SimpleEntityView::ProcessUpdateMsg(net::InMessage* msg)
{
    return ReadState(msg);
}

void game::view::SimpleEntityView::Update(const UpdateInfo& info)
{
    Super::Update(info);

    // interpolate states
    float tps = 25.0f;
    float t = (info.time - update_time_) * tps * 0.8f; // assume some jitter, interpolate for longer
    t = glm::clamp(t, 0.0f, 2.0f);

    root_.local = Transform::Lerp(states_[0].trans, states_[1].trans, t);
    root_.UpdateMatrix();
}

void game::view::SimpleEntityView::Draw(const DrawArgs& args)
{
    Super::Draw(args);

    if (!model_)
        return;

    model_->Draw(args.ctx, root_.matrix, {});
}

bool game::view::SimpleEntityView::ReadState(net::InMessage* msg)
{
    update_time_ = world_.GetTime();

    // init lerp start state
    states_[0].trans = root_.local;

    auto& new_state = states_[1];

    if (msg)
    {
        // parse state delta
        SimpleEntitySyncFieldFlags fields;
        if (!msg->Read(fields))
            return false;

        // pos
        if (fields & SESF_POSITION)
        {
            if (!net::ReadDelta(*msg, sync_.pos.x) || !net::ReadDelta(*msg, sync_.pos.y) || !net::ReadDelta(*msg, sync_.pos.z))
                return false;

            net::DecodePosition(sync_.pos, new_state.trans.position);
        }

        // rot
        if (fields & SESF_ROTATION)
        {
            if (!net::ReadDelta(*msg, sync_.rot.x) || !net::ReadDelta(*msg, sync_.rot.y) || !net::ReadDelta(*msg, sync_.rot.z))
                return false;

            net::DecodeRotation(sync_.rot, new_state.trans.rotation);
        }
    }

    return true;
}
