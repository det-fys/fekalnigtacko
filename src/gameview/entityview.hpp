#pragma once

#include "game/transform_node.hpp"
#include "gfx/draw_list.hpp"

class World;

namespace game::view
{

class EntityView
{
public:
    EntityView(World& world) : world_(world) {}

    virtual void Draw(gfx::DrawList& dlist) {}
    virtual void Update() {}

protected:
    World& world_;
    TransformNode root_;
    bool visible_ = false;

};

} // namespace game::view