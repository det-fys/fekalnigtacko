#include "openworld.hpp"

#include "player.hpp"
#include "vehicle.hpp"

game::OpenWorld::OpenWorld() : World("openworld")
{
    srand(time(NULL));
}

void game::OpenWorld::PlayerJoined(Player& player)
{
    SpawnVehicle(player);
}

void game::OpenWorld::PlayerInput(Player& player, PlayerInputType type, bool enabled)
{
    auto vehicle = player_vehicles_.at(&player);
    // player.SendChat("input zmenen: " + std::to_string(static_cast<int>(type)) + "=" + (enabled ? "1" : "0"));

    switch (type)
    {
    case IN_FORWARD:
        vehicle->SetInput(VIN_FORWARD, enabled);
        break;

    case IN_BACKWARD:
        vehicle->SetInput(VIN_BACKWARD, enabled);
        break;

    case IN_LEFT:
        vehicle->SetInput(VIN_LEFT, enabled);
        break;

    case IN_RIGHT:
        vehicle->SetInput(VIN_RIGHT, enabled);
        break;

    case IN_DEBUG1:
        if (enabled)
            vehicle->SetPosition({ 100.0f, 100.0f, 5.0f });
        break;
    
    case IN_DEBUG2:
        if (enabled)
            SpawnVehicle(player);
        break;

    default:
        break;
    }
}

void game::OpenWorld::PlayerLeft(Player& player)
{
    RemoveVehicle(player);
}

void game::OpenWorld::RemoveVehicle(Player& player)
{
    auto it = player_vehicles_.find(&player);
    if (it != player_vehicles_.end())
    {
        it->second->Remove();
        player_vehicles_.erase(it);
    }
}

void game::OpenWorld::SpawnVehicle(Player& player)
{
    RemoveVehicle(player);

    // spawn him car
    // random model
    const char* vehicles[] = {"pickup_hd", "passat", "twingo", "polskifiat"};
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
    vehicle.SetPosition({ 100.0f, 100.0f, 5.0f });

    player.SetCamera(vehicle.GetEntNum());

    player_vehicles_[&player] = &vehicle;

    player.SendChat("dostals " + std::string(vehicle_name));
}
