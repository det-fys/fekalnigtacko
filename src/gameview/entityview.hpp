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

    bool TryUpdate(const UpdateInfo& info); // if not updated already
    virtual void Update(const UpdateInfo& info);
    
    virtual void Draw(const DrawArgs& args);

    Sphere GetBoundingSphere() const { return Sphere{root_.GetGlobalPosition(), radius_}; }
    const TransformNode& GetRoot() const { return root_; }

    virtual ~EntityView() = default;

private:
    bool ReadNametag(net::InMessage& msg);
    bool ReadAttach(net::InMessage& msg);

    void DrawNametag(const DrawArgs& args);
    void DrawAxes(const DrawArgs& args);

protected:
    WorldView& world_;

    TransformNode root_;

    EntityView* parent_ = nullptr;

    float radius_ = 1.0f;

    audio::Player audioplayer_;

private:
    std::string nametag_;
    gfx::Text nametag_text_;
    gfx::HudPosition nametag_pos_;

    net::EntNum parentnum_ = 0;
    float upd_time_ = 0.0f;
};

} // namespace game::view