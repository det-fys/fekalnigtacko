#include "player.hpp"

#include "world.hpp"

#include <iostream>

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
        SyncEntities();
}

void game::Player::SetWorld(std::shared_ptr<World> world)
{
    if (world == world_)
        return;

    if (world_)
        world_->PlayerLeft(*this);

    world_ = std::move(world);

    if (world_)
        world_->PlayerJoined(*this);
}

void game::Player::SetCamera(net::EntNum entnum)
{
    auto msg = BeginMsg(net::MSG_CAM);
    msg.Write(entnum);
}

void game::Player::SendChat(const std::string text)
{
    auto msg = BeginMsg(net::MSG_CHAT);
    net::ChatMessage chatm = text;
    msg.Write(chatm);
}

game::Player::~Player()
{
    SetWorld(nullptr);
    game_.PlayerLeft(*this);
}

void game::Player::SendWorldMsg()
{
    MSGDEBUG(std::cout << "seding CHWORLD" << std::endl;)
    auto msg = BeginMsg(net::MSG_CHWORLD);
    msg.Write(net::MapName(world_->GetMapName()));
}

void game::Player::SyncEntities()
{
    const auto& ents = world_->GetEntities();

    auto ent_it = ents.begin();
    auto know_it = known_ents_.begin();

    while (ent_it != ents.end() || know_it != known_ents_.end())
    {
        const net::EntNum entnum = (ent_it != ents.end() ? ent_it->first : std::numeric_limits<net::EntNum>::max());
        const net::EntNum knownum = (know_it != known_ents_.end() ? *know_it : std::numeric_limits<net::EntNum>::max());

        if (entnum == knownum) // ----- entity exists and is currently known -----
        {
            const Entity& e = *ent_it->second;
            if (ShouldSeeEntity(e)) // still visible?
            {
                SendUpdateEntity(e); // 2) update
                ++ent_it;
                ++know_it;
            }
            else // vanished for player
            {
                SendDestroyEntity(knownum);           // 3) destroy
                know_it = known_ents_.erase(know_it); // remove from known
                ++ent_it;
            }
        }
        else if (entnum < knownum) // ----- entity exists, player does NOT know it -----
        {
            const Entity& e = *ent_it->second;
            if (ShouldSeeEntity(e)) // 1) become visible
            {
                SendInitEntity(e);
                known_ents_.insert(entnum); // add to known
            } // else: stays invisible, nothing to do
            ++ent_it;
        }
        else // ----- player knows it, but it no longer exists -----
        {
            SendDestroyEntity(knownum);           // 3) destroy
            know_it = known_ents_.erase(know_it); // remove from known
        }
    }
}

bool game::Player::ShouldSeeEntity(const Entity& entity) const
{
    return true; // TODO: check distance?
}

void game::Player::SendInitEntity(const Entity& entity)
{
    MSGDEBUG(std::cout << "seding ENTSPAWN " << entity.GetEntNum() << std::endl;)
    auto msg = BeginMsg(net::MSG_ENTSPAWN);
    msg.Write(entity.GetEntNum());
    msg.Write(entity.GetViewType());
    entity.SendInitData(*this, msg);
}

void game::Player::SendUpdateEntity(const Entity& entity)
{
    MSGDEBUG(std::cout << "seding update ent " << entity.GetEntNum() << std::endl;)
    auto msg = BeginMsg(); // no CMD here, these are already included in entity message payload!
    msg.Write(entity.GetMsg());
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

void game::Player::Input(PlayerInputType type, bool enabled)
{
    if (enabled)
        in_ |= (1 << type);
    else
        in_ &= ~(1 << type);

    if (!game_.PlayerInput(*this, type, enabled))
    {
        if (world_)
            world_->PlayerInput(*this, type, enabled);
    }
}
