#include "entity.hpp"

#include "world.hpp"
#include "player.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>


game::Entity::Entity(World& world, net::EntType viewtype) : Scheduler(world.GetTime()), world_(world), entnum_(world.GetNewEntnum()), viewtype_(viewtype)
{
    if (viewtype == net::ET_NONE)
    {
        visible_ = false;
    }

}

void game::Entity::SendInitData(Player& player, net::OutMessage& msg) const
{
    WriteNametag(msg);
    WriteAttach(msg);
}

void game::Entity::Update()
{
    up_to_date_ = true;

    // ensure parent is updated
    parent_ = nullptr;
    if (parentnum_)
    {
        parent_ = world_.GetEntity(parentnum_);

        if (parent_)
            parent_->TryUpdate();
    }

    // update transform parent
    root_.parent = parent_ ? &parent_->GetRoot() : nullptr;

    RunTasks();
}

bool game::Entity::TryUpdate()
{
    if (IsUpToDate())
        return false;

    Update();
    return true;
}

void game::Entity::FinalizeFrame()
{
    ResetMsg();
    DiscardUpdateMsg();
    up_to_date_ = false;
}

void game::Entity::SetNametag(const std::string& nametag)
{
    if (nametag_ == nametag)
        return;

    nametag_ = nametag;
    SendNametagMsg(); // notify viewers
}

void game::Entity::Attach(net::EntNum parentnum)
{
    parentnum_ = parentnum;
    SendAttachMsg();
}

void game::Entity::PlaySound(const std::string& name, float volume, float pitch)
{
    auto msg = BeginEntMsg(net::EMSG_PLAYSOUND);
    msg.Write(net::SoundName(name));
    msg.Write<net::SoundVolumeQ>(volume);
    msg.Write<net::SoundPitchQ>(pitch);
}

bool game::Entity::IsVisibleTo(const Player& player) const
{
    if (!visible_)
        return false;

    // max distance check
    if (glm::distance2(root_.GetGlobalPosition(), player.GetCullPos()) > (max_distance_ * max_distance_))
        return false;

    return true;
}

void game::Entity::WriteNametag(net::OutMessage& msg) const
{
    msg.Write(net::NameTag{nametag_});
}

void game::Entity::SendNametagMsg()
{
    auto msg = BeginEntMsg(net::EMSG_NAMETAG);
    WriteNametag(msg);
}

void game::Entity::WriteAttach(net::OutMessage& msg) const
{
    msg.Write(parentnum_);
}

void game::Entity::SendAttachMsg()
{
    auto msg = BeginEntMsg(net::EMSG_ATTACH);
    WriteAttach(msg);
}

net::OutMessage game::Entity::BeginEntMsg(net::EntMsgType type)
{
    auto msg = BeginMsg(net::MSG_ENTMSG);
    msg.Write(entnum_);
    msg.Write(type);
    return msg;
}

net::OutMessage game::Entity::BeginUpdateMsg()
{
    update_msg_buf_.clear(); // make sure
    return net::OutMessage(update_msg_buf_);
}

void game::Entity::DiscardUpdateMsg()
{
    update_msg_buf_.clear();
}
