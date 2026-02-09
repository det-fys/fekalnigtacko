#pragma once

#include "entityview.hpp"
#include "game/skeletoninstance.hpp"

namespace game::view
{

class CharacterView : public EntityView
{
public:
    using Super = EntityView;

    CharacterView(WorldView& world, net::InMessage& msg);
    DELETE_COPY_MOVE(CharacterView)

    virtual bool ProcessMsg(net::EntMsgType type, net::InMessage& msg) override;
    virtual void Update(const UpdateInfo& info) override;
    virtual void Draw(const DrawArgs& args) override;

private:
    bool ProcessUpdateMsg(net::InMessage& msg);

private:
    float yaw_ = 0.0f;

    SkeletonInstance sk_;

};

}
