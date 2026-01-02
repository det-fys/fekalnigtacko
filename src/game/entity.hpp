#pragma once

#include <vector>

#include "net/msg_producer.hpp"
#include "transform_node.hpp"

namespace game
{

class World;
class Player;

class Entity : public net::MsgProducer
{
public:
    Entity(World& world, net::EntType viewtype);

    net::EntNum GetEntNum() const { return entnum_; }
    net::EntType GetViewType() const { return viewtype_; }

    virtual void Update() { ResetMsg(); }
    virtual void SendInitData(Player& player, net::OutMessage& msg) const {}

protected:
    World& world_;
    const net::EntNum entnum_;
    const net::EntType viewtype_;

    TransformNode root_;
};

}