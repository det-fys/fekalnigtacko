#pragma once

#include <vector>

#include "net/msg_producer.hpp"
#include "transform_node.hpp"
#include "utils/defs.hpp"
#include "utils/scheduler.hpp"

namespace game
{

class World;
class Player;

class Entity : public net::MsgProducer, public Scheduler
{
public:
    Entity(World& world, net::EntType viewtype);
    DELETE_COPY_MOVE(Entity)

    net::EntNum GetEntNum() const { return entnum_; }
    net::EntType GetViewType() const { return viewtype_; }

    virtual void Update();
    virtual void SendInitData(Player& player, net::OutMessage& msg) const;

    void SetNametag(const std::string& nametag);

    void Remove() { removed_ = true; }
    bool IsRemoved() const { return removed_; }

    const Transform& GetRootTransform() const { return root_.local; }
    float GetMaxDistance() const { return max_distance_; }

    virtual ~Entity() = default;

private:
    void WriteNametag(net::OutMessage& msg) const;
    
    void SendNametagMsg();

protected:
    net::OutMessage BeginEntMsg(net::EntMsgType type);

protected:
    World& world_;
    const net::EntNum entnum_;
    const net::EntType viewtype_;

    TransformNode root_;

    float max_distance_ = 700.0f;
    std::string nametag_;


    bool removed_ = false;

};

}