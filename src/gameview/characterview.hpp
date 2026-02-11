#pragma once

#include "entityview.hpp"
#include "assets/model.hpp"
#include "game/skeletoninstance.hpp"
#include "skinning_ubo.hpp"

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

    std::shared_ptr<const assets::Model> basemodel_;
    SkeletonInstance sk_;
    SkinningUBO ubo_;
    bool ubo_valid_ = false;

};

}
