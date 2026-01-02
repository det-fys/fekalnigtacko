#include "game.hpp"

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

void game::Game::PlayerJoined(Player& player)
{
    player.SetWorld(default_world_.get());
}

void game::Game::PlayerLeft(Player& player)
{
    
}
