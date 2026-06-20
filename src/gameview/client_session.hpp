#pragma once

#include <memory>

#include "worldview.hpp"
#include "gfx/draw_list.hpp"
#include "gfx/renderer.hpp"
#include "net/defs.hpp"
#include "net/inmessage.hpp"
#include "net/msg_producer.hpp"
#include "game/player_input.hpp"
#include "gui/player_hud.hpp"
#include "remote_menu_view.hpp"
#include "game/camera_info.hpp"
#include "game/camera_controller.hpp"

class App;

namespace game::view
{

class ClientSession : public net::MsgProducer
{
public:
    ClientSession(App& app);

    bool ProcessMessage(net::InMessage& msg);
    bool ProcessSingleMessage(net::MessageType type, net::InMessage& msg);

    void Input(game::PlayerInputType in, bool pressed, bool repeated);
    void ProcessMouseMove(float delta_yaw, float delta_pitch);

    void Update(const UpdateInfo& info);
    void Draw(gfx::DrawList& dlist, gfx::DrawListParams& params, gui::Context& gui);

    const WorldView* GetWorld() const { return world_.get(); } 
    audio::Master& GetAudioMaster() const;

private:
    // msg handlers
    bool ProcessWorldMsg(net::InMessage& msg);
    bool ProcessCameraMsg(net::InMessage& msg);
    bool ProcessChatMsg(net::InMessage& msg);
    bool ProcessHudMsg(net::InMessage& msg);
    bool ProcessDamageMsg(net::InMessage& msg);
    bool ProcessUseTargetMsg(net::InMessage& msg);
    bool ProcessMenuMsg(net::InMessage& msg);

    void UpdateCamera(const UpdateInfo& info);
    void DrawWorld(gfx::DrawList& dlist, gfx::DrawListParams& params, gui::Context& gui);
    
    void SendInput(game::PlayerInputType type, bool enable);
    void SendViewAngles(float time);

    void DrawMenus(gui::Context& gui) const;
    bool ProcessMenuInput(game::PlayerInputType in);
    RemoteMenuView* FindMenu(net::MenuId id) const;

private:
    App& app_;
    
    std::unique_ptr<WorldView> world_;

    CameraController camera_controller_;
    CameraInfo camera_info_;
    
    net::ViewYawQ view_yaw_q_;
    net::ViewPitchQ view_pitch_q_;
    float last_send_time_ = 0.0f;

    gui::PlayerHud hud_;

    std::vector<std::unique_ptr<RemoteMenuView>> remote_menus_;
};

} // namespace game::view