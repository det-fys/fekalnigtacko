#include "entityview.hpp"

#include <iostream>

#include "worldview.hpp"
#include "assets/asset_manager.hpp"

game::view::EntityView::EntityView(WorldView& world, net::InMessage& msg) : 
    world_(world),
    audioplayer_(world_.GetAudioMaster())
{
    // read nametag and attachment info
    if (!ReadNametag(msg) || !ReadAttach(msg))
        throw EntityInitError();
}

bool game::view::EntityView::ProcessMsg(net::EntMsgType type, net::InMessage& msg)
{
    switch (type)
    {
    case net::EMSG_NAMETAG:
        return ReadNametag(msg);
    case net::EMSG_ATTACH:
        return ReadAttach(msg);
    case net::EMSG_PLAYSOUND:
        return ProcessPlaySoundMsg(msg);
    default:
        return false;
    }
}

bool game::view::EntityView::ProcessUpdateMsg(net::InMessage* msg)
{
    return true;
}

bool game::view::EntityView::TryUpdate(const UpdateInfo& info)
{
    float time = world_.GetTime();
    if (time == upd_time_)
        return false;

    Update(info);
    return true;
}

void game::view::EntityView::Update(const UpdateInfo& info)
{
    upd_time_ = world_.GetTime();

    // ensure parent is updated
    parent_ = nullptr;
    if (parentnum_)
    {
        parent_ = world_.GetEntity(parentnum_);

        if (parent_)
            parent_->TryUpdate(info);
    }

    // update transform parent
    root_.parent = parent_ ? &parent_->GetRoot() : nullptr;

    audioplayer_.Update();
}

void game::view::EntityView::Draw(const DrawArgs& args)
{
    // std::cout << "TODO draw entity nametag: " << nametag_ << std::endl;
    DrawNametag(args);
    //DrawAxes(args);
}

bool game::view::EntityView::ReadNametag(net::InMessage& msg)
{
    // read nametag
    net::NameTag nametag;
    if (!msg.Read(nametag))
        return false;

    nametag_ = nametag;
    return true;
}

bool game::view::EntityView::ReadAttach(net::InMessage& msg)
{
    if (!msg.Read(parentnum_))
        return false;

    OnAttach();

    return true;
}

bool game::view::EntityView::ProcessPlaySoundMsg(net::InMessage& msg)
{
    net::SoundName name;
    float volume, pitch;
    if (!msg.Read(name) || !msg.Read<net::SoundVolumeQ>(volume) || !msg.Read<net::SoundPitchQ>(pitch))
        return false;

    if (!world_.IsLoaded())
        return true; // dont play if not loaded yet

    auto sound = assets::AssetManager::GetInstance().Get<audio::Sound>(std::string(name));
    auto snd = audioplayer_.PlaySound(sound, &root_);
    snd->SetVolume(volume);
    snd->SetPitch(pitch);

    return true;
}

void game::view::EntityView::DrawNametag(const DrawArgs& args)
{
    if (args.ctx.pass != gfx::DRAW_PASS_MAIN || nametag_.empty())
        return;

    // calc screen position
    glm::vec4 world_pos = GetRoot().matrix * glm::vec4(glm::vec3(0.0f, 0.0f, 2.0f), 1.0f);
    glm::vec4 clip_pos = args.ctx.view_proj * world_pos;
    if (clip_pos.w == 0.0f)
        return;

    glm::vec3 ndc_pos = glm::vec3(clip_pos) / clip_pos.w;

    if (ndc_pos.z < -1.0f || ndc_pos.z > 1.0f)
        return; // behind camera

    glm::vec2 anchor = ndc_pos * 0.5f + 0.5f;
    anchor.y = 1.0f - anchor.y;
    anchor *= args.gui.GetViewportSize();
    
    float scale = 0.7f;
    glm::vec2 pos = anchor + args.gui.MeasureText(nametag_) * glm::vec2(-0.5f, -1.0f) * scale;

    args.gui.DrawText(nametag_, pos, 0xFFFFFFFF, scale);
}

void game::view::EntityView::DrawAxes(const DrawArgs& args)
{
    if (args.ctx.pass != gfx::DRAW_PASS_MAIN)
        return;

    const float len = 5.0f;
    static const uint32_t colors[] = {0xFF0000FF, 0xFF00FF00, 0xFFFF0000};

    for (size_t i = 0; i < 3; i++)
    {
        glm::vec3 end(0.0f);
        end[i] = len;

        glm::vec3 beam_start = glm::vec3(root_.matrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        glm::vec3 beam_end = glm::vec3(root_.matrix * glm::vec4(end, 1.0f));

        args.ctx.dlist.AddBeam(beam_start, beam_end, colors[i], 0.05f);
    }
}
