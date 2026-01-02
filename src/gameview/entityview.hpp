#pragma once

#include <stdexcept>

#include "game/transform_node.hpp"
#include "gfx/draw_list.hpp"

#include "net/defs.hpp"
#include "net/inmessage.hpp"

class World;

namespace game::view
{

class EntityView
{
public:
    EntityView(World& world) : world_(world) {}

    virtual bool ProcessMsg( net::InMessage& msg) { return false; }

    virtual void Update() {}

    virtual void Draw(gfx::DrawList& dlist) {}

protected:
    World& world_;
    TransformNode root_;
    bool visible_ = false;

};

} // namespace game::view