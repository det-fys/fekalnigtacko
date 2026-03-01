#include "entity.hpp"

#include "world.hpp"

game::Entity::Entity(World& world, net::EntType viewtype) : Scheduler(world.GetTime()), world_(world), entnum_(world.GetNewEntnum()), viewtype_(viewtype) {}

void game::Entity::SendInitData(Player& player, net::OutMessage& msg) const
{
    WriteNametag(msg);
    WriteAttach(msg);
}

void game::Entity::Update()
{
    upd_time_ = world_.GetTime();

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
    int64_t time = world_.GetTime();
    if (time == upd_time_)
        return false;

    Update();
    return true;
}

void game::Entity::FinalizeFrame()
{
    ResetMsg();
    DiscardUpdateMsg();
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
