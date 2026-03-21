#include "player.hpp"

#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include "world.hpp"
#include "game.hpp"

game::Player::Player(Game& game, std::string name) : game_(game), name_(std::move(name))
{
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

    default:
        return false;
    }
}

void game::Player::Update()
{
    if (world_.get() != known_world_)
    {
        SendWorldMsg();
        known_world_ = world_.get();
        known_ents_.clear();
    }

    if (world_)
    {
        SendWorldUpdateMsg();
        SyncEntities();
    }
}

void game::Player::SetWorld(std::shared_ptr<World> world)
{
    if (world == world_)
        return;

    world_ = std::move(world);
}

void game::Player::SetCamera(net::EntNum entnum)
{
    cam_ent_ = entnum;

    auto msg = BeginMsg(net::MSG_CAM);
    msg.Write(entnum);
}

void game::Player::SendChat(const std::string& text)
{
    auto msg = BeginMsg(net::MSG_CHAT);
    net::ChatMessage chatm = text;
    msg.Write(chatm);
}

game::Player::~Player()
{
    game_.PlayerLeft(*this);
}

void game::Player::SendWorldMsg()
{
    MSGDEBUG(std::cout << "seding CHWORLD" << std::endl;)
    auto msg = BeginMsg(net::MSG_CHWORLD);
    world_->SendInitData(*this, msg);
}

void game::Player::SendWorldUpdateMsg()
{
    if (!world_)
        return;

    auto msg = BeginMsg(); // no CMD here, included in world payload
    msg.Write(world_->GetMsg());
}

void game::Player::SyncEntities()
{
    // update cull pos
    if (cam_ent_)
    {
        auto cam_ent = world_->GetEntity(cam_ent_);
        if (cam_ent)
        {
            cull_pos_ = cam_ent->GetRoot().GetGlobalPosition();
        }
    }

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
    // max distance check
    float max_dist = entity.GetMaxDistance();
    if (glm::distance2(entity.GetRoot().GetGlobalPosition(), cull_pos_) > (max_dist * max_dist))
        return false;

    // TODO: custom callback

    return true;
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

    view_yaw_ = yaw_q.Decode();
    view_pitch_ = pitch_q.Decode();
    
    game_.PlayerViewAnglesChanged(*this, view_yaw_, view_pitch_);    
    return true;
}

void game::Player::Input(PlayerInputType type, bool enabled)
{
    if (enabled)
        in_ |= (1 << type);
    else
        in_ &= ~(1 << type);

    game_.PlayerInput(*this, type, enabled);
}

