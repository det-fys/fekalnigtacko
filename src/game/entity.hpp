#pragma once

#include <vector>

#include "net/msg_producer.hpp"
#include "transform_node.hpp"
#include "utils/defs.hpp"

namespace game
{

class World;
class Player;

class Entity : public net::MsgProducer
{
public:
    Entity(World& world, net::EntType viewtype);
    DELETE_COPY_MOVE(Entity)

    net::EntNum GetEntNum() const { return entnum_; }
    net::EntType GetViewType() const { return viewtype_; }

    virtual void Update() { ResetMsg(); }
    virtual void SendInitData(Player& player, net::OutMessage& msg) const {}

    void Remove() { removed_ = true; }
    bool IsRemoved() const { return removed_; }

    virtual ~Entity() = default;

protected:
    net::OutMessage BeginEntMsg(net::EntMsgType type);

protected:
    World& world_;
    const net::EntNum entnum_;
    const net::EntType viewtype_;

    TransformNode root_;

    bool removed_ = false;
};

}