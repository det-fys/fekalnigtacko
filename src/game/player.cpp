#include "player.hpp"

#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include "world.hpp"
#include "game.hpp"
#include "utils/cvars.hpp"
#include "utils/chatcolors.hpp"
#include "db/db.hpp"
#include "utils/format.hpp"

CVAR(uint8_t, pl_autoadmin, CV_NONE, 0, 0, 1);

game::Player::Player(Game& game, db::PlayerId id) : game_(game), id_(id)
{
    LoadName();
    LoadBalance();

    if (pl_autoadmin.Get() > 0)
    {
        is_admin_ = true;
        SendChat(COL_SUCCESS "jsi automaticky admin (pl_autoadmin = 1)");
    }

    game_.PlayerJoined(*this);
}

bool game::Player::ProcessMsg(net::MessageType type, net::InMessage& msg)
{
    switch (type)
    {
    case net::MSG_IN:
        return ProcessInputMsg(msg);

    case net::MSG_VIEWANGLES:
        return ProcessViewAnglesMsg(msg);

    case net::MSG_MENUACTION:
        return ProcessMenuActionMsg(msg);

    case net::MSG_CHAT:
        return ProcessChatMsg(msg);

    default:
        return false;
    }
}

void game::Player::Update()
{
    SyncWorld();
    SendMenuMsgs();
    UpdateCamera();
    SendHudUpdate();
    SendDamageEvents();

    // reset for next frame
    in_new_ = 0;
}

void game::Player::SetWorld(World* world)
{
    if (world == world_)
        return;

    world_ = world;
}

void game::Player::SetCamera(const CameraInfo& camera_info)
{
    camera_info_ = camera_info;

    auto msg = BeginMsg(net::MSG_CAM);
    msg.Write(camera_info.character_entnum);
    msg.Write(camera_info.rideable_entnum);
    msg.Write(camera_info.flags);
}

void game::Player::SendChat(const std::string& text)
{
    auto msg = BeginMsg(net::MSG_CHAT);
    msg.Write(net::ChatMessage(text));
}

void game::Player::SetUseTarget(const std::string& text, const std::string& error_text, float delay)
{
    auto msg = BeginMsg(net::MSG_USETARGET);
    msg.Write(net::UseTargetName(text));
    msg.Write(net::UseTargetName(error_text));
    msg.Write<net::UseDelayQ>(delay);
}

game::RemoteMenu& game::Player::DisplayMenu(std::string title)
{
    if (remote_menu_)
        throw std::runtime_error("cannot display multiple menus atm");

    net::MenuId id = ++menu_id_;
    remote_menu_ = std::make_unique<RemoteMenu>(id, std::move(title));
    
    // send msg
    auto msg = BeginMsg(net::MSG_REMOTEMENU);
    msg.Write(id);
    msg.Write(net::MMSG_CREATE);
    
    return *remote_menu_;
}

void game::Player::CloseMenu(const RemoteMenu& menu)
{
    if (&menu != remote_menu_.get())
        return;

    // send msg
    auto msg = BeginMsg(net::MSG_REMOTEMENU);
    msg.Write(menu.GetId());
    msg.Write(net::MMSG_CLOSE);

    remote_menu_.reset();
}

game::PlayerCharacterHudData& game::Player::GetCharacterHudData()
{
    return hud_data_[1].character;
}

void game::Player::ResetCharacterHudData()
{
    GetCharacterHudData() = PlayerCharacterHudData{};
}

void game::Player::DisplayDamageEvent(DamageEventType type)
{
    dmg_event_flags_ |= 1 << type;
}

bool game::Player::GetView(glm::vec3& eye, glm::vec3& forward)
{
    if (!world_)
        return false;

    auto character = world_->GetEntity(camera_info_.character_entnum);
    camera_controller_.SetCharacterTransform(character ? &character->GetRoot().matrix : nullptr);
    
    auto rideable = world_->GetEntity(camera_info_.rideable_entnum);
    camera_controller_.SetRideableTransform(rideable ? &rideable->GetRoot().matrix : nullptr);

    camera_controller_.Recalculate(world_);

    eye = camera_controller_.GetEye();
    forward = camera_controller_.GetForward();
    return true;
}

bool game::Player::ChangeBalance(int64_t delta, const std::string& desc)
{
    if (delta == 0)
        return true;
    
    auto res = game_.GetDb().ChangePlayerBalance(id_, delta);
    if (res != db::QR_OK)
    {
        SendChat(COL_ERROR "chyba: " + std::string(db::GetResultDescription(res)));
        return false;
    }

    if (delta < 0)
    {
        SendChat("výdaj " + FormatBalance(-delta) + ": " + desc);
    }
    else
    {
        SendChat("příjem " + FormatBalance(delta) + ": " + desc);
    }

    LoadBalance();
    return true;
}

game::Player::~Player()
{
    game_.PlayerLeft(*this);
}

void game::Player::SyncWorld()
{
    if (world_ != known_world_)
    {
        SendWorldMsg();
        known_world_ = world_;
        known_ents_.clear();

        return; // send updates next frame
    }

    if (world_)
    {
        UpdateCullPos();
        SendWorldUpdateMsg();
        SendEnv();
        SyncEntities();
    }
}

void game::Player::UpdateCullPos()
{
    auto cam_entnum = camera_info_.rideable_entnum ? camera_info_.rideable_entnum : camera_info_.character_entnum;
    if (cam_entnum)
    {
        auto cam_ent = world_->GetEntity(cam_entnum);
        if (cam_ent)
        {
            cull_pos_ = cam_ent->GetRoot().GetGlobalPosition();
        }
    }
}

void game::Player::SendWorldMsg()
{
    MSGDEBUG(std::cout << "seding CHWORLD" << std::endl;)
    auto msg = BeginMsg(net::MSG_CHWORLD);
    world_->SendInitData(*this, msg);

    last_env_time_ = 0; // reset after world changed
}

void game::Player::SendWorldUpdateMsg()
{
    if (!world_)
        return;

    auto msg = BeginMsg(); // no CMD here, included in world payload
    msg.Write(world_->GetMsg());

    // local msgs
    world_->PickLocalMsgs(*this, cull_pos_);
}

void game::Player::SendHudUpdate()
{
    PlayerHudFields fields = 0;

    auto msg = BeginMsg(net::MSG_HUD);
    auto fields_pos = msg.Reserve<PlayerHudFields>();

    auto& prev_data = hud_data_[0];
    auto& curr_data = hud_data_[1];

    if (curr_data.balance != prev_data.balance)
    {
        fields |= PHUD_BALANCE;
        prev_data.balance = curr_data.balance;
        msg.Write(curr_data.balance);
    }

    if (curr_data.character.health != prev_data.character.health)
    {
        fields |= PHUD_HEALTH;
        prev_data.character.health = curr_data.character.health;
        msg.Write(curr_data.character.health);
    }

    if (curr_data.character.weapon_slots != prev_data.character.weapon_slots)
    {
        fields |= PHUD_WEAPON_SLOTS;
        prev_data.character.weapon_slots = curr_data.character.weapon_slots;
        msg.Write(curr_data.character.weapon_slots);
    }

    if (curr_data.character.held_item != prev_data.character.held_item)
    {
        fields |= PHUD_ITEM;
        prev_data.character.held_item = curr_data.character.held_item;
        msg.Write(net::ModelName(curr_data.character.held_item));
    }

    if (curr_data.character.ammo_loaded != prev_data.character.ammo_loaded)
    {
        fields |= PHUD_AMMO_LOADED;
        prev_data.character.ammo_loaded = curr_data.character.ammo_loaded;
        msg.Write(curr_data.character.ammo_loaded);
    }

    if (curr_data.character.ammo_total != prev_data.character.ammo_total)
    {
        fields |= PHUD_AMMO_TOTAL;
        prev_data.character.ammo_total = curr_data.character.ammo_total;
        msg.Write(curr_data.character.ammo_total);
    }

    if (curr_data.character.dead != prev_data.character.dead)
    {
        fields |= PHUD_DEATH;
        prev_data.character.dead = curr_data.character.dead;
        msg.Write(curr_data.character.dead);
    }

    if (fields == 0)
    {
        DiscardMsg();
        return;
    }

    msg.WriteAt(fields_pos, fields);
}

void game::Player::SendDamageEvents()
{
    if (dmg_event_flags_ & (1 << DAMAGE_EVENT_RECEIVED))
        SendDamageEvent(DAMAGE_EVENT_RECEIVED);

    if (dmg_event_flags_ & (1 << DAMAGE_EVENT_DEALT))
        SendDamageEvent(DAMAGE_EVENT_DEALT);

    if (dmg_event_flags_ & (1 << DAMAGE_EVENT_DEALT_KILL))
        SendDamageEvent(DAMAGE_EVENT_DEALT_KILL);

    dmg_event_flags_ = 0;
}

void game::Player::SendDamageEvent(DamageEventType type)
{
    auto msg = BeginMsg(net::MSG_HUDEVENT);
    msg.Write(type);
}

void game::Player::SendEnv()
{
    if (!world_ || last_env_time_ + 1000 > world_->GetTime())
        return;

    last_env_time_ = world_->GetTime();

    auto msg = BeginMsg(net::MSG_ENV);
    msg.Write<net::DayTimeQ>(world_->GetDayTime());
}

void game::Player::SyncEntities()
{
    // list of entities to send update and messages of
    static std::vector<const Entity*> upd_ents;
    upd_ents.clear();

    const auto& ents = world_->GetEntities();

    auto ent_it = ents.begin();
    auto know_it = known_ents_.begin();

    while (ent_it != ents.end() || know_it != known_ents_.end())
    {
        const net::EntNum entnum = (ent_it != ents.end() ? ent_it->first : std::numeric_limits<net::EntNum>::max());
        const net::EntNum knownum = (know_it != known_ents_.end() ? *know_it : std::numeric_limits<net::EntNum>::max());

        if (entnum == knownum) // entity exists and is currently known
        {
            const Entity& e = *ent_it->second;
            if (ShouldSeeEntity(e)) // still visible?
            {
                upd_ents.push_back(&e);
                ++ent_it;
                ++know_it;
            }
            else // not longed visible for player
            {
                SendDestroyEntity(knownum);
                know_it = known_ents_.erase(know_it); 
                ++ent_it;
            }
        }
        else if (entnum < knownum) // entity exists, player does NOT know it
        {
            const Entity& e = *ent_it->second;
            if (ShouldSeeEntity(e))
            {
                SendInitEntity(e);
                known_ents_.insert(entnum);
            }
            ++ent_it;
        }
        else // player knows it, but it no longer exists
        {
            SendDestroyEntity(knownum);
            know_it = known_ents_.erase(know_it);
        }
    }

    // write update payload
    {
        auto msg = BeginMsg(net::MSG_UPDATEENTS);
        size_t count_pos = msg.Reserve<net::EntCount>();
    
        net::EntCount count = 0;
        net::EntNum lastnum = 0;
        
        for (auto ent : upd_ents)
        {
            auto ent_upd = ent->GetUpdateMsg();
            if (!ent_upd.empty())
            {
                auto numdiff = ent->GetEntNum() - lastnum;
                
                msg.WriteVarInt(numdiff);
                msg.Write(ent_upd);
                
                ++count;
                lastnum = ent->GetEntNum();
            }
        }

        msg.WriteAt(count_pos, count);
    }

    // write other entity msgs
    for (auto ent : upd_ents)
    {
        auto ent_msg = ent->GetMsg();
        if (!ent_msg.empty())
        {
            auto msg = BeginMsg();
            msg.Write(ent_msg);
        }
    }
}

bool game::Player::ShouldSeeEntity(const Entity& entity) const
{
    return entity.IsVisibleTo(*this);
}

void game::Player::SendInitEntity(const Entity& entity)
{
    MSGDEBUG(std::cout << "seding ENTSPAWN " << entity.GetEntNum() << std::endl;)
    auto msg = BeginMsg(net::MSG_ENTSPAWN);
    msg.Write(entity.GetEntNum());
    msg.Write(entity.GetViewType());
    entity.SendInitData(*this, msg);
}

void game::Player::SendDestroyEntity(net::EntNum entnum)
{
    MSGDEBUG(std::cout << "seding ENTDESTROY " << entnum << std::endl;)
    auto msg = BeginMsg(net::MSG_ENTDESTROY);
    msg.Write(entnum);
}

bool game::Player::ProcessInputMsg(net::InMessage& msg)
{
    uint8_t val;
    if (!msg.Read(val))
        return false;

    bool enabled = false;
    if (val & 128)
    {
        enabled = true;
        val &= ~128;
    }

    Input(static_cast<PlayerInputType>(val), enabled);
    return true;
}

bool game::Player::ProcessViewAnglesMsg(net::InMessage& msg)
{
    net::ViewYawQ yaw_q;
    net::ViewPitchQ pitch_q;

    if (!msg.Read(yaw_q.value) || !msg.Read(pitch_q.value))
        return false;

    camera_controller_.SetViewAngles(yaw_q.Decode(),  pitch_q.Decode());
    game_.PlayerViewAnglesChanged(*this, camera_controller_.GetYaw(), camera_controller_.GetPitch());    
    return true;
}

bool game::Player::ProcessMenuActionMsg(net::InMessage& msg)
{
    net::MenuId id;
    net::MenuActionType type;

    if (!msg.Read(id) || !msg.Read(type))
        return false;

    if (!remote_menu_ || remote_menu_->GetId() != id)
    {
        // not illegal, might be just a late message
        // need to skip specific amount of bytes :((((
        switch (type)
        {
        case net::MA_CLICK:
            return msg.Skip(sizeof(net::MenuItemId));
        case net::MA_SELECT:
            return msg.Skip(sizeof(net::MenuItemId) + sizeof(net::MenuSelectDir));
        case net::MA_HOVER:
            return msg.Skip(sizeof(net::MenuItemId));
        case net::MA_EXIT:
            return true;
        default:
            return false;
        }
    }

    return remote_menu_->ProcessActionMsg(msg, type);
}

bool game::Player::ProcessChatMsg(net::InMessage& msg)
{
    net::ChatMessage line;
    if (!msg.Read(line))
        return false;

    game_.PlayerChat(*this, line);

    return true;
}

void game::Player::Input(PlayerInputType type, bool enabled)
{
    PlayerInputFlags flag = 1 << type;

    if (enabled)
    {
        in_ |= flag;
        in_new_ |= flag;
    }
    else
    {
        in_ &= ~flag;
    }

    game_.PlayerInput(*this, type, enabled);
}

void game::Player::SendMenuMsgs()
{
    if (!remote_menu_)
        return;

    remote_menu_->Update();
    auto menu_msg = remote_menu_->GetMsg();

    if (menu_msg.empty())
        return;

    auto msg = BeginMsg();
    msg.Write(menu_msg);

    remote_menu_->ResetMsg();
}

void game::Player::UpdateCamera()
{
    camera_controller_.SetAiming(camera_info_.flags & CAM_AIMING);
    camera_controller_.SetAimType(camera_info_.flags & CAM_AIM_CROSSHAIR, camera_info_.flags & CAM_AIM_SCOPE);
    camera_controller_.Update(1.0f / 25.0f);
}

void game::Player::LoadName()
{
    auto res = game_.GetDb().GetPlayerName(id_, name_);
    if (res != db::QR_OK)
    {
        SendChat(COL_ERROR "chyba při načítání jména: " + std::string(db::GetResultDescription(res)));
        name_ = "<chyba při načítání jména>";
    }
}

void game::Player::LoadBalance()
{
    db::MoneyAmount balance;
    auto res = game_.GetDb().GetPlayerBalance(id_, balance);
    if (res != db::QR_OK)
    {
        SendChat(COL_ERROR "nepovedlo se načíst zůstatek: " + std::string(db::GetResultDescription(res)));
        return;
    }

    hud_data_[1].balance = balance;
}
