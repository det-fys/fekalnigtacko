#include "entityview.hpp"

#include <iostream>

#include "worldview.hpp"
#include "assets/cache.hpp"

game::view::EntityView::EntityView(WorldView& world, net::InMessage& msg) : 
    world_(world),
    audioplayer_(world_.GetAudioMaster()),
    nametag_text_(assets::CacheManager::GetFont("data/comic32.font"), 0xFFFFFFFF)
{
    if (!ReadNametag(msg))
        throw EntityInitError();
}

bool game::view::EntityView::ProcessMsg(net::EntMsgType type, net::InMessage& msg)
{
    switch (type)
    {
    case net::EMSG_NAMETAG:
        return ReadNametag(msg);
    }

    return false;
}

void game::view::EntityView::Update(const UpdateInfo& info)
{
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
    nametag_text_.SetText(nametag);
    return true;
}

void game::view::EntityView::DrawNametag(const DrawArgs& args)
{
    if (nametag_.empty())
        return;

    // calc screen position
    glm::vec4 world_pos = glm::vec4(root_.local.position + glm::vec3(0.0f, 0.0f, 2.0f), 1.0f);
    glm::vec4 clip_pos = args.view_proj * world_pos;
    if (clip_pos.w == 0.0f)
        return;

    glm::vec3 ndc_pos = glm::vec3(clip_pos) / clip_pos.w;

    if (ndc_pos.z < -1.0f || ndc_pos.z > 1.0f)
        return; // behind camera

    nametag_pos_.anchor.x = ndc_pos.x * 0.5f + 0.5f;
    nametag_pos_.anchor.y = 1.0f - (ndc_pos.y * 0.5f + 0.5f); // flip y
    
    // center
    float scale = 0.7f;
    nametag_pos_.scale = glm::vec2(scale, -scale);
    nametag_pos_.pos = nametag_text_.GetSize() * glm::vec2(-0.5f, -1.0f) * scale;

    // static const glm::vec4 nametag_color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    
    gfx::DrawHudCmd hudcmd;
    hudcmd.va = &nametag_text_.GetVA();
    hudcmd.texture = nametag_text_.GetFont()->GetTexture().get();
    hudcmd.pos = &nametag_pos_;
    // hudcmd.color = &nametag_color;
    args.dlist.AddHUD(hudcmd);
    // std::cout << "at position: " << root_.local.position.x << ", " << root_.local.position.y << ", " << root_.local.position.z << std::endl;

}

void game::view::EntityView::DrawAxes(const DrawArgs& args)
{
    const float len = 5.0f;
    static const uint32_t colors[] = {0xFF0000FF, 0xFF00FF00, 0xFFFF0000};

    for (size_t i = 0; i < 3; i++)
    {
        glm::vec3 end(0.0f);
        end[i] = len;

        gfx::DrawBeamCmd cmd;
        cmd.start = glm::vec3(root_.matrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        cmd.end = glm::vec3(root_.matrix * glm::vec4(end, 1.0f));
        cmd.color = colors[i];
        cmd.radius = 0.05f;
        //cmd.num_segments = 10;
        //cmd.max_offset = 0.1f;
        args.dlist.AddBeam(cmd);
    }
}
