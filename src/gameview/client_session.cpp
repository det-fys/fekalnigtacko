#include "client_session.hpp"

#include "client/app.hpp"
#include <iostream>
// #include <glm/gtx/common.hpp>

#include "utils/version.hpp"
#include "utils.hpp"
#include "vehicleview.hpp"
#include "assets/asset_manager.hpp"
#include "assets/item.hpp"
#include "game/player_hud_data.hpp"

game::view::ClientSession::ClientSession(App& app) : app_(app), hud_(app.GetTime())
{
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

    case net::MSG_HUD:
        return ProcessHudMsg(msg);

    case net::MSG_DAMAGE:
        return ProcessDamageMsg(msg);

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
    if (camera_controller_.ShouldDrawScope())
    {
        sens_mult = 0.1f;
    }

    float yaw = glm::mod(camera_controller_.GetYaw() + delta_yaw * sens_mult, glm::two_pi<float>());

    float pitch = camera_controller_.GetPitch() + delta_pitch * sens_mult;
    pitch = glm::clamp(pitch, glm::radians(-89.0f), glm::radians(89.0f)); // Clamp pitch to avoid gimbal lock
    
    camera_controller_.SetViewAngles(yaw, pitch);
}

void game::view::ClientSession::ChatInput(std::string_view line)
{
    auto msg = BeginMsg(net::MSG_CHAT);
    msg.Write(net::ChatMessage(line));
}

void game::view::ClientSession::Update(const UpdateInfo& info)
{
    if (world_)
    {
        world_->Update(info);
        SendViewAngles(info.time);
        UpdateCamera(info);
    }

    hud_.Update(info.delta_time);
}

void game::view::ClientSession::Draw(gfx::DrawList& dlist, gfx::DrawListParams& params, gui::Context& gui)
{
    if (world_)
    {
        DrawWorld(dlist, params, gui);

        if (world_->IsLoaded())
        {
            hud_.Draw(gui);
        }
    }

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

    // app_.AddChatMessagePrefix("Server", chatm);
    app_.AddChatMessage(chatm);
    return true;
}

bool game::view::ClientSession::ProcessHudMsg(net::InMessage& msg)
{
    PlayerHudFields fields{};
    if (!msg.Read(fields))
        return false;

    PlayerHudData hud_data{};

    if (fields & PHUD_HEALTH)
    {
        if (!msg.Read(hud_data.health))
            return false;

        hud_.SetHealth(static_cast<float>(hud_data.health));
    }

    if (fields & PHUD_WEAPON_SLOTS)
    {
        if (!msg.Read(hud_data.weapon_slots))
            return false;
        
        hud_.SetWeaponSlots(hud_data.weapon_slots);
    }

    if (fields & PHUD_ITEM)
    {
        net::ModelName item_name;
        if (!msg.Read(item_name))
            return false;

        hud_data.held_item = item_name;

        // determine clip size
        std::string displayname = hud_data.held_item;
        size_t clip_size = 0;
        size_t item_slot = 0;
        if (!hud_data.held_item.empty())
        {
            auto item = assets::AssetManager::GetInstance().Get<assets::Item>(hud_data.held_item);
            displayname = item->displayname;
            clip_size = item->clip_size;
            item_slot = item->slot;
        }

        hud_.SetItemInfo(displayname, item_slot, clip_size);
    }

    if (fields & PHUD_AMMO_LOADED)
    {
        if (!msg.Read(hud_data.ammo_loaded))
            return false;

        hud_.SetLoadedAmmo(static_cast<size_t>(hud_data.ammo_loaded));
    }

    if (fields & PHUD_AMMO_TOTAL)
    {
        if (!msg.Read(hud_data.ammo_total))
            return false;

        hud_.SetTotalAmmo(static_cast<size_t>(hud_data.ammo_total));
    }

    if (fields & PHUD_DEATH)
    {
        if (!msg.Read(hud_data.dead))
            return false;

        hud_.SetDead(hud_data.dead > 0);
    }

    return true;
}

bool game::view::ClientSession::ProcessDamageMsg(net::InMessage& msg)
{
    DamageEventType type;
    if (!msg.Read(type))
        return false;

    if (type == DAMAGE_EVENT_RECEIVED)
    {
        hud_.ShowDamageReceived();
    }
    else
    {
        hud_.ShowDamageDealt(type == DAMAGE_EVENT_DEALT_KILL);
    }

    return true;
}

bool game::view::ClientSession::ProcessUseTargetMsg(net::InMessage& msg)
{
    net::UseTargetName text, error_text;
    float delay;

    if (!msg.Read(text) || !msg.Read(error_text) || !msg.Read<net::UseDelayQ>(delay))
        return false;

    hud_.SetUseTargetData(text, error_text, delay);
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
    camera_controller_.SetAimType(camera_info_.flags & CAM_AIM_CROSSHAIR, camera_info_.flags & CAM_AIM_SCOPE);

    camera_controller_.Update(info.delta_time);
    camera_controller_.Recalculate(world_.get());

    hud_.SetDisplayCrosshair(camera_controller_.ShouldDrawCrosshair());
    hud_.SetDisplayScope(camera_controller_.ShouldDrawScope());

    // hide character if scope
    if (character)
    {
        character->SetVisible(!camera_controller_.ShouldDrawScope());
    }
}

void game::view::ClientSession::DrawWorld(gfx::DrawList& dlist, gfx::DrawListParams& params, gui::Context& gui)
{
    // glm::mat4 view = glm::lookAt(glm::vec3(15.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -13.0f), glm::vec3(0.0f,
    // 0.0f, 1.0f));
    float aspect = static_cast<float>(params.screen_width) / static_cast<float>(params.screen_height);

    const float farplane = 3000.0f;

    glm::mat4 proj = glm::perspective(glm::radians(camera_controller_.GetFov() * 0.5f), aspect, 0.1f, farplane);
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

