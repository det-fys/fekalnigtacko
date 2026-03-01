#pragma once

#include "assets/model.hpp"
#include "entityview.hpp"
#include "game/simple_entity_sync.hpp"

namespace game::view
{

struct SimpleEntityViewState
{
    Transform trans;
};

class SimpleEntityView : public EntityView
{
public:
    using Super = EntityView;

    SimpleEntityView(WorldView& world, net::InMessage& msg);

    virtual bool ProcessMsg(net::EntMsgType type, net::InMessage& msg) override;
    virtual bool ProcessUpdateMsg(net::InMessage* msg) override;
    virtual void Update(const UpdateInfo& info) override;
    virtual void Draw(const DrawArgs& args) override;

private:
    bool ReadState(net::InMessage* msg);

private:
    std::shared_ptr<const assets::Model> model_;

    SimpleEntitySyncState sync_;
    SimpleEntityViewState states_[2];
    float update_time_ = 0.0f;

};

    
}