#include "openworld.hpp"

#include "vehicle.hpp"
#include "player.hpp"

game::OpenWorld::OpenWorld() : World("openworld") {}

void game::OpenWorld::PlayerJoined(Player& player)
{
    // spawn him car
    auto& vehicle = Spawn<Vehicle>("pickup");
    player.Control(&vehicle);

    player_vehicles_[&player] = vehicle.GetEntNum();
}

void game::OpenWorld::PlayerLeft(Player& player)
{
    auto it = player_vehicles_.find(&player);
    
    Entity* ent = GetEntity(it->second);
    if (ent)
        ent->Remove();
    
    player_vehicles_.erase(it);


}
