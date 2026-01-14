#include "openworld.hpp"

#include "vehicle.hpp"
#include "player.hpp"

game::OpenWorld::OpenWorld() : World("openworld") {}

void game::OpenWorld::PlayerJoined(Player& player)
{
    // spawn him car
    // random model
    const char* vehicles[] = { "pickup", "passat" };
    auto vehicle_name = vehicles[rand() % (sizeof(vehicles) / sizeof(vehicles[0]))];

    // ranodm color
    glm::vec3 color;
    for (int i = 0; i < 3; ++i)
    {
        net::ColorQ qcol;
        qcol.value = rand() % 256;
        color[i] = qcol.Decode();
    }

    auto& vehicle = Spawn<Vehicle>(vehicle_name, color);
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
