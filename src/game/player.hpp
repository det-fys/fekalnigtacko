#pragma once

#include <set>
#include <memory>

#include <glm/glm.hpp>

#include "net/defs.hpp"
#include "net/inmessage.hpp"
#include "net/msg_producer.hpp"
#include "utils/defs.hpp"

#include "player_input.hpp"

#include "remote_menu.hpp"

namespace game
{

class Game;
class World;
class Entity;

class Player : public net::MsgProducer
{
public:
    Player(Game& game, std::string name);
    DELETE_COPY_MOVE(Player)

    bool ProcessMsg(net::MessageType type, net::InMessage& msg);
    void Update();

    void SetWorld(World* world);

    void SetCamera(net::EntNum entnum);
    void SendChat(const std::string& text);
    void SetUseTarget(const std::string& text, const std::string& error_text, float delay);

    RemoteMenu& DisplayMenu(std::string title);
    void CloseMenu(const RemoteMenu& menu);
    bool HasOpenMenu() const { return (bool)remote_menu_; }

    const std::string& GetName() const { return name_; }

    PlayerInputFlags GetInput() const { return in_; }
    float GetViewYaw() const { return view_yaw_; }
    float GetViewPitch() const { return view_pitch_; }

    const glm::vec3 GetCullPos() const { return cull_pos_; }

    ~Player();

private:
    // world sync
    void SyncWorld();
    void SendWorldMsg();
    void SendWorldUpdateMsg();
    void SendEnv();

    // entities sync
    void SyncEntities();
    bool ShouldSeeEntity(const Entity& entity) const;
    void SendInitEntity(const Entity& entity);
    void SendDestroyEntity(net::EntNum entnum);

    // msg handlers
    bool ProcessInputMsg(net::InMessage& msg);
    bool ProcessViewAnglesMsg(net::InMessage& msg);
    bool ProcessMenuActionMsg(net::InMessage& msg);

    // events
    void Input(PlayerInputType type, bool enabled);

    // menu sync
    void SendMenuMsgs();

private:
    Game& game_;
    std::string name_;

    World* world_ = nullptr;
    World* known_world_ = nullptr;
    std::set<net::EntNum> known_ents_;
    int64_t last_env_time_ = 0;

    PlayerInputFlags in_ = 0;
    float view_yaw_ = 0.0f, view_pitch_ = 0.0f;

    net::EntNum cam_ent_ = 0;
    glm::vec3 cull_pos_ = glm::vec3(0.0f);

    // menus
    // TODO: allow more menus
    net::MenuId menu_id_ = 0;
    std::unique_ptr<RemoteMenu> remote_menu_;
};

}