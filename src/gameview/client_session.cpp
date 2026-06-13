#include "client_session.hpp"

#include "client/app.hpp"
#include <iostream>
// #include <glm/gtx/common.hpp>

#include "utils/version.hpp"
#include "utils.hpp"
#include "vehicleview.hpp"
#include "assets/cache.hpp"

game::view::ClientSession::ClientSession(App& app) : app_(app), use_target_hud_(app.GetTime())
{
    crosshair_texture_ = assets::CacheManager::GetTexture("data/crosshair.png");

    // send login
    auto msg = BeginMsg(net::MSG_ID);
    msg.Write<net::Version>(FEKAL_VERSION);
    msg.Write(net::PlayerName(app.GetUserName()));
}

bool game::view::ClientSession::ProcessMessage(net::InMessage& msg)
{
    while (true)
    {
        net::MessageType type = net::MSG_NONE;
        if (!msg.Read(type))
            return true;

        if (type == net::MSG_NONE || type >= net::MSG_COUNT)
            return false;

        if (!ProcessSingleMessage(type, msg))
            return false;
    }
}

bool game::view::ClientSession::ProcessSingleMessage(net::MessageType type, net::InMessage& msg)
{
    MSGDEBUG(std::cout << "[MSG] received " << (uint32_t)type << std::endl;)

    switch (type)
    {
    case net::MSG_CHWORLD:
        return ProcessWorldMsg(msg);

    case net::MSG_CAM:
        return ProcessCameraMsg(msg);

    case net::MSG_CHAT:
        return ProcessChatMsg(msg);

    case net::MSG_USETARGET:
        return ProcessUseTargetMsg(msg);

    case net::MSG_REMOTEMENU:
        return ProcessMenuMsg(msg);

    default:
        // try pass the msg to world
        if (world_ && world_->ProcessMsg(type, msg))
            return true;

        return false;
    }
}

void game::view::ClientSession::Input(game::PlayerInputType in, bool pressed, bool repeated)
{
    if (pressed && ProcessMenuInput(in))
        return;

    if (repeated)
        return;

    SendInput(in, pressed);
}

void game::view::ClientSession::ProcessMouseMove(float delta_yaw, float delta_pitch)
{
    auto sens_mult = glm::mix(1.0f, 0.3f, camera_controller_.GetAimFactor());

    float yaw = glm::mod(camera_controller_.GetYaw() + delta_yaw * sens_mult, glm::two_pi<float>());

    float pitch = camera_controller_.GetPitch() + delta_pitch * sens_mult;
    pitch = glm::clamp(pitch, glm::radians(-89.0f), glm::radians(89.0f)); // Clamp pitch to avoid gimbal lock
    
    camera_controller_.SetViewAngles(yaw, pitch);
}

void game::view::ClientSession::Update(const UpdateInfo& info)
{
    if (world_)
    {
        world_->Update(info);
        SendViewAngles(info.time);
        UpdateCamera(info);
    }
}

void game::view::ClientSession::Draw(gfx::DrawList& dlist, gfx::DrawListParams& params, gui::Context& gui)
{
    if (world_)
    {
        DrawWorld(dlist, params, gui);
    }

    DrawCrosshair(gui);
    use_target_hud_.Draw(gui);

    DrawMenus(gui);
}

audio::Master& game::view::ClientSession::GetAudioMaster() const
{
    return app_.GetAudioMaster();
}

bool game::view::ClientSession::ProcessWorldMsg(net::InMessage& msg)
{
    try
    {
        world_ = std::make_unique<WorldView>(*this, msg);
    }
    catch (const EntityInitError&)
    {
        return false;
    }

    return true;
}

bool game::view::ClientSession::ProcessCameraMsg(net::InMessage& msg)
{
    if (!msg.Read(camera_info_.character_entnum) || !msg.Read(camera_info_.rideable_entnum) || !msg.Read(camera_info_.flags))
        return false;

    return true;
}

bool game::view::ClientSession::ProcessChatMsg(net::InMessage& msg)
{
    net::ChatMessage chatm;
    if (!msg.Read(chatm))
        return false;

    app_.AddChatMessagePrefix("Server", chatm);
    return true;
}

bool game::view::ClientSession::ProcessUseTargetMsg(net::InMessage& msg)
{
    net::UseTargetName text, error_text;
    float delay;

    if (!msg.Read(text) || !msg.Read(error_text) || !msg.Read<net::UseDelayQ>(delay))
        return false;

    use_target_hud_.SetData(text, error_text, delay);
    return true;
}

bool game::view::ClientSession::ProcessMenuMsg(net::InMessage& msg)
{
    net::MenuId id;
    net::MenuMessageType type;

    if (!msg.Read(id) || !msg.Read(type))
        return false;

    switch (type)
    {
    case net::MMSG_CREATE: {
        if (FindMenu(id))
            return false;

        remote_menus_.push_back(std::make_unique<RemoteMenuView>(*this, id));
        return true;
    }

    case net::MMSG_CLOSE: {
        std::erase_if(remote_menus_, [id](auto& menu) { return menu->GetId() == id; });
        return true;
    }

    default: {
        auto menu = FindMenu(id);
        if (!menu)
            return false;

        return menu->ProcessMessage(type, msg);
    }
    }
}

void game::view::ClientSession::UpdateCamera(const UpdateInfo& info)
{
    auto character = world_->GetEntity(camera_info_.character_entnum);
    camera_controller_.SetCharacterTransform(character ? &character->GetRoot().matrix : nullptr);
    
    auto rideable = world_->GetEntity(camera_info_.rideable_entnum);
    camera_controller_.SetRideableTransform(rideable ? &rideable->GetRoot().matrix : nullptr);
    
    camera_controller_.SetAiming(camera_info_.flags & CAM_AIMING);
    camera_controller_.Update(info.delta_time);
    camera_controller_.Recalculate(world_.get());
}

void game::view::ClientSession::DrawWorld(gfx::DrawList& dlist, gfx::DrawListParams& params, gui::Context& gui)
{
    // glm::mat4 view = glm::lookAt(glm::vec3(15.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -13.0f), glm::vec3(0.0f,
    // 0.0f, 1.0f));
    float aspect = static_cast<float>(params.screen_width) / static_cast<float>(params.screen_height);

    const float farplane = 3000.0f;

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, farplane);
    glm::mat4 view = camera_controller_.GetViewMatrix();

    params.view_proj = proj * view;
    params.cam_pos = camera_controller_.GetEye();

    // glm::mat4 fake_view_proj = glm::perspective(glm::radians(30.0f), aspect, 0.1f, 3000.0f) * view;

    game::view::DrawArgs draw_args(dlist, params.env, gui, params.view_proj, params.cam_pos,
                                   glm::ivec2(params.screen_width, params.screen_height), farplane, 500.0f);
    world_->Draw(draw_args);

    glm::mat4 camera_world = glm::inverse(view);
    GetAudioMaster().SetListenerOrientation(camera_world);
}

void game::view::ClientSession::SendInput(game::PlayerInputType type, bool enable)
{
    auto msg = BeginMsg(net::MSG_IN);
    uint8_t val = type;
    if (enable)
        val |= 128;
    msg.Write(val);
}

void game::view::ClientSession::SendViewAngles(float time)
{
    if (time - last_send_time_ < 0.040f)
        return;

    net::ViewYawQ yaw_q;
    net::ViewPitchQ pitch_q;
    yaw_q.Encode(camera_controller_.GetYaw());
    pitch_q.Encode(camera_controller_.GetPitch());

    if (yaw_q.value == view_yaw_q_.value && pitch_q.value == view_pitch_q_.value)
        return;

    auto msg = BeginMsg(net::MSG_VIEWANGLES);
    msg.Write(yaw_q.value);
    msg.Write(pitch_q.value);

    view_yaw_q_.value = yaw_q.value;
    view_pitch_q_.value = pitch_q.value;
    last_send_time_ = time;
}

void game::view::ClientSession::DrawMenus(gui::Context& gui) const
{
    if (remote_menus_.empty())
        return;

    auto& top_menu = *remote_menus_.back();
    top_menu.Draw(gui, glm::vec2(20.0f, 100.0f));

}

bool game::view::ClientSession::ProcessMenuInput(game::PlayerInputType in)
{
    if (remote_menus_.empty())
        return false;

    gui::MenuInput mi;
    if (!InputToMenuInput(in, mi))
        return false;

    remote_menus_.back()->Input(mi);
    return true;
}

game::view::RemoteMenuView* game::view::ClientSession::FindMenu(net::MenuId id) const
{
    for (auto& menu : remote_menus_)
    {
        if (menu->GetId() == id)
            return menu.get();
    }

    return nullptr;
}

void game::view::ClientSession::DrawCrosshair(gui::Context& gui) const
{
    if (camera_controller_.GetAimFactor() < 0.5f)
        return; // no aiming no crosshair

    const float crosshair_size = 32.0f;

    auto& viewport_size = gui.GetViewportSize();

    auto p0 = viewport_size * 0.5f - crosshair_size * 0.5f;
    auto p1 = p0 + crosshair_size;

    gui.DrawRect(p0, p1, 0xFFFFFFFF, crosshair_texture_.get());
}
