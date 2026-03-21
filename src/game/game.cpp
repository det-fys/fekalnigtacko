#include "game.hpp"

#include "player.hpp"
#include "openworld.hpp"

game::Game::Game()
{
    openworld_ = std::make_shared<OpenWorld>();
    all_worlds_.push_back(openworld_.get());
}

void game::Game::Update()
{
    for (auto world : all_worlds_)
    {
        world->Update(40);
    }
}

void game::Game::FinishFrame()
{
    for (auto world : all_worlds_)
    {
        world->FinishFrame();
    }
}

void game::Game::PlayerJoined(Player& player)
{
    BroadcastChat(player.GetName() + "^r se připoojil jupí jupí jupííí");

    players_.insert({ &player, PlayerGameInfo(player) });
    auto& player_info = players_.at(&player);
    player_info.world = openworld_.get();
    player.SetWorld(openworld_);

    openworld_->InsertPlayer(player, glm::vec3(100.0f, 100.0f, 5.0f), 0.0f);
}

void game::Game::PlayerViewAnglesChanged(Player& player, float yaw, float pitch)
{
    auto world = FindPlayerWorld(player);
    if (world)
        world->PlayerViewAnglesChanged(player, yaw, pitch);
}

void game::Game::PlayerInput(Player& player, PlayerInputType type, bool enabled)
{
    auto world = FindPlayerWorld(player);
    if (world)
        world->PlayerInput(player, type, enabled);
}

void game::Game::PlayerLeft(Player& player)
{
    auto world = FindPlayerWorld(player);
    if (world)
        world->RemovePlayer(player);

    players_.erase(&player);

    BroadcastChat(player.GetName() + "^r se vodpojil zmrd");
}

void game::Game::BroadcastChat(const std::string& text)
{
    for (auto& [player, info] : players_)
    {
        player->SendChat(text);
    }
}

game::EnterableWorld* game::Game::FindPlayerWorld(Player& player) const
{
    auto it = players_.find(&player);
    if (it == players_.end())
        return nullptr;

    return it->second.world;
}
