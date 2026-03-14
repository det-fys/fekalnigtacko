#include "game.hpp"

#include <format>

#include "player.hpp"
#include "openworld.hpp"

game::Game::Game()
{
    default_world_ = std::make_shared<OpenWorld>();

}

void game::Game::Update()
{
    default_world_->Update(40);
}

void game::Game::FinishFrame()
{
    default_world_->FinishFrame();
}

void game::Game::PlayerJoined(Player& player)
{
    player.SetWorld(default_world_);

    players_.insert(&player);
    BroadcastChat(std::format("{}^r se připoojil jupí jupí jupííí", player.GetName()));
}

void game::Game::PlayerLeft(Player& player)
{
    players_.erase(&player);
    BroadcastChat(std::format("{}^r se vodpojil zmrd", player.GetName()));
}

bool game::Game::PlayerInput(Player& player, PlayerInputType type, bool enabled)
{
    return false; // not handled here
}

void game::Game::BroadcastChat(const std::string& text)
{
    for (auto player : players_)
    {
        player->SendChat(text);
    }
}
