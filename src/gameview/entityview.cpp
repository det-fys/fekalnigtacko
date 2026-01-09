#include "entityview.hpp"

#include "worldview.hpp"

game::view::EntityView::EntityView(WorldView& world) : world_(world), audioplayer_(world_.GetAudioMaster()) {}

bool game::view::EntityView::ProcessMsg(net::EntMsgType type, net::InMessage& msg)
{
    return false;
}

void game::view::EntityView::Update(const UpdateInfo& info)
{
    audioplayer_.Update();
}

void game::view::EntityView::Draw(gfx::DrawList& dlist) {}
