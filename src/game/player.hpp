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
#include "camera_info.hpp"
#include "camera_controller.hpp"
#include "player_hud_data.hpp"

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

    void SetCamera(const CameraInfo& camera_info);
    void SendChat(const std::string& text);
    void SetUseTarget(const std::string& text, const std::string& error_text, float delay);

    RemoteMenu& DisplayMenu(std::string title);
    void CloseMenu(const RemoteMenu& menu);
    bool HasOpenMenu() const { return (bool)remote_menu_; }

    void SetHudData(const PlayerHudData& hud_data);
    void ResetHudData();

    const std::string& GetName() const { return name_; }

    PlayerInputFlags GetInput() const { return in_; }
    PlayerInputFlags GetNewInput() const { return in_new_; }
    float GetViewYaw() const { return camera_controller_.GetYaw(); }
    float GetViewPitch() const { return camera_controller_.GetPitch(); }
    bool GetView(glm::vec3& eye, glm::vec3& forward);

    const glm::vec3 GetCullPos() const { return cull_pos_; }

    ~Player();

private:
    // world sync
    void SyncWorld();
    void UpdateCullPos();
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

    void UpdateCamera();

private:
    Game& game_;
    std::string name_;

    World* world_ = nullptr;
    World* known_world_ = nullptr;
    std::set<net::EntNum> known_ents_;
    int64_t last_env_time_ = 0;

    PlayerInputFlags in_ = 0;
    PlayerInputFlags in_new_ = 0;

    CameraInfo camera_info_;
    CameraController camera_controller_;
    glm::vec3 cull_pos_ = glm::vec3(0.0f);

    // menus
    // TODO: allow more menus
    net::MenuId menu_id_ = 0;
    std::unique_ptr<RemoteMenu> remote_menu_;

    // hud
    PlayerHudData hud_data_;
};

}