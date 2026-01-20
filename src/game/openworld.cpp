#include "openworld.hpp"

#include "player.hpp"
#include "vehicle.hpp"

game::OpenWorld::OpenWorld() : World("openworld")
{
    srand(time(NULL));

    // spawn test vehicles
    for (size_t i = 0; i < 150; ++i)
    {
        auto& vehicle = Spawn<Vehicle>("pickup_hd", glm::vec3{1.0f, 0.0f, 0.0f});
        vehicle.SetPosition({ static_cast<float>(i * 3), 150.0f, 5.0f });
        vehicle.SetInput(VIN_FORWARD, true);    
        bots_.push_back(&vehicle);
    }
}

void game::OpenWorld::Update(int64_t delta_time)
{
    World::Update(delta_time);

    for (auto bot : bots_)
    {
        bot->SetInput(VIN_FORWARD, true);
    
        if (rand() % 1000 < 10)
        {
            bool turn_left = rand() % 2;
            bot->SetInput(VIN_LEFT, turn_left);
            bot->SetInput(VIN_RIGHT, !turn_left);
        }
        else
        {
            bot->SetInput(VIN_LEFT, false);
            bot->SetInput(VIN_RIGHT, false);
        }

        auto pos = bot->GetPosition();
        if (glm::distance(pos, glm::vec3(0.0f, 0.0f, 0.0f)) > 1000.0f || pos.z < -20.0f)
        {
            bot->SetPosition({ rand() % 30 * 3 + 100.0f, 200.0f, 10.0f });
        }
    
    }
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
