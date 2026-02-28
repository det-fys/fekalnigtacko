#pragma once

#include <vector>

#include "net/msg_producer.hpp"
#include "transform_node.hpp"
#include "utils/defs.hpp"
#include "utils/scheduler.hpp"
#include "collision/object_info.hpp"

namespace game
{

class World;
class Player;

class Entity : public net::MsgProducer, public Scheduler, public collision::ObjectCallback
{
public:
    Entity(World& world, net::EntType viewtype);
    DELETE_COPY_MOVE(Entity)

    net::EntNum GetEntNum() const { return entnum_; }
    net::EntType GetViewType() const { return viewtype_; }

    virtual void SendInitData(Player& player, net::OutMessage& msg) const;

    virtual void Update();
    bool TryUpdate(); // if not already updated
    int64_t GetUpdateTime() const { return upd_time_; }

    void SetNametag(const std::string& nametag);
     
    void Attach(net::EntNum parentnum);
    net::EntNum GetParentNum() const { return parentnum_; }

    void PlaySound(const std::string& name, float volume = 1.0f, float pitch = 1.0f);

    void Remove() { removed_ = true; }
    bool IsRemoved() const { return removed_; }

    const TransformNode& GetRoot() const { return root_; }
    const Transform& GetRootTransform() const { return root_.local; }

    float GetMaxDistance() const { return max_distance_; }

    virtual ~Entity() = default;

private:
    void WriteNametag(net::OutMessage& msg) const;
    void SendNametagMsg();

    void WriteAttach(net::OutMessage& msg) const;
    void SendAttachMsg();

protected:
    net::OutMessage BeginEntMsg(net::EntMsgType type);

protected:
    World& world_;
    const net::EntNum entnum_;
    const net::EntType viewtype_;

    TransformNode root_;
    Entity* parent_ = nullptr;

    float max_distance_ = 700.0f;

    bool removed_ = false;

private:
    int64_t upd_time_ = -1;
    std::string nametag_;
    net::EntNum parentnum_ = 0;
};

}