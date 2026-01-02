#include "entity.hpp"

#include "world.hpp"

game::Entity::Entity(World& world, net::EntType viewtype) : world_(world), entnum_(world.GetNewEntnum()), viewtype_(viewtype) {}

net::OutMessage game::Entity::BeginEntMsg(net::EntMsgType type)
{
    auto msg = BeginMsg(net::MSG_ENTMSG);
    msg.Write(entnum_);
    msg.Write(type);
    return msg;
}
