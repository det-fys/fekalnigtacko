#include "entity.hpp"

#include "world.hpp"

game::Entity::Entity(World& world, net::EntType viewtype) : Scheduler(world.GetTime()), world_(world), entnum_(world.GetNewEntnum()), viewtype_(viewtype) {}

void game::Entity::Update()
{
    ResetMsg();
    RunTasks();
}

void game::Entity::SendInitData(Player& player, net::OutMessage& msg) const
{
    WriteNametag(msg);
}

void game::Entity::SetNametag(const std::string& nametag)
{
    nametag_ = nametag;
    SendNametagMsg(); // notify viewers
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

net::OutMessage game::Entity::BeginEntMsg(net::EntMsgType type)
{
    auto msg = BeginMsg(net::MSG_ENTMSG);
    msg.Write(entnum_);
    msg.Write(type);
    return msg;
}
