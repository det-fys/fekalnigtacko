#pragma once

#include <stdexcept>

#include "game/transform_node.hpp"
#include "gfx/draw_list.hpp"

#include "net/defs.hpp"
#include "net/inmessage.hpp"

#include "utils/defs.hpp"

namespace game::view
{

class WorldView;

struct UpdateInfo
{
    float time;
    float delta_time;
};

class EntityView
{
public:
    EntityView(WorldView& world) : world_(world) {}
    DELETE_COPY_MOVE(EntityView)

    virtual bool ProcessMsg(net::EntMsgType type, net::InMessage& msg) { return false; }
    virtual void Update(const UpdateInfo& info) {}
    virtual void Draw(gfx::DrawList& dlist) {}

    const TransformNode& GetRoot() const { return root_; }

    virtual ~EntityView() = default;

protected:
    WorldView& world_;
    TransformNode root_;
    bool visible_ = false;

};

} // namespace game::view