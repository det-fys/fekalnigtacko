#pragma once

#include <stdexcept>

#include "audio/player.hpp"
#include "draw_args.hpp"
#include "game/transform_node.hpp"
#include "gfx/text.hpp"

#include "net/defs.hpp"
#include "net/inmessage.hpp"

#include "utils/defs.hpp"

namespace game::view
{

class EntityInitError : public std::runtime_error
{
public:
    explicit EntityInitError() : std::runtime_error("Could not initialize entity from message") {}
};

class WorldView;

struct UpdateInfo
{
    float time;
    float delta_time;
};

class EntityView
{
public:
    EntityView(WorldView& world, net::InMessage& msg);
    DELETE_COPY_MOVE(EntityView)

    virtual bool ProcessMsg(net::EntMsgType type, net::InMessage& msg);
    virtual void Update(const UpdateInfo& info);
    virtual void Draw(const DrawArgs& args);

    Sphere GetBoundingSphere() const { return Sphere{root_.local.position, radius_}; }

    const TransformNode& GetRoot() const { return root_; }

    virtual ~EntityView() = default;

private:
    bool ReadNametag(net::InMessage& msg);

protected:
    WorldView& world_;

    TransformNode root_;
    float radius_ = 1.0f;

    audio::Player audioplayer_;

    std::string nametag_;
    gfx::Text nametag_text_;
    gfx::HudPosition nametag_pos_;
};

} // namespace game::view