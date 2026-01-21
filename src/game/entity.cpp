#include "entity.hpp"

#include "world.hpp"

game::Entity::Entity(World& world, net::EntType viewtype) : Scheduler(world.GetTime()), world_(world), entnum_(world.GetNewEntnum()), viewtype_(viewtype) {}

void game::Entity::Update()
{
    ResetMsg();
    RunTasks();
}

net::OutMessage game::Entity::BeginEntMsg(net::EntMsgType type)
{
    auto msg = BeginMsg(net::MSG_ENTMSG);
    msg.Write(entnum_);
    msg.Write(type);
    return msg;
}
