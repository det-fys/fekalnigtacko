#include "openworld.hpp"

#include <iostream>

#include "player.hpp"
#include "vehicle.hpp"

#include "player_character.hpp"
#include "npc_character.hpp"
#include "drivable_vehicle.hpp"
#include "destroyed_object.hpp"

namespace game
{

} // namespace game


static const char* GetRandomCarModel()
{
    const char* vehicles[] = {"pickup_hd", "passat", "twingo", "polskifiat", "cow_static", "pig_static"};
    return vehicles[rand() % (sizeof(vehicles) / sizeof(vehicles[0]))];
}

static glm::vec3 GetRandomColor()
{
    glm::vec3 color;
    // shittiest way to do it
    for (int i = 0; i < 3; ++i)
    {
        net::ColorQ qcol;
        qcol.value = rand() % 256;
        color[i] = qcol.Decode();
    }

    return color;
}

static uint32_t GetRandomColor24()
{
    uint8_t r,g,b;
    r = rand() % 256;
    g = rand() % 256;
    b = rand() % 256;
    return (b << 16) | (g << 8) | r;
}

game::OpenWorld::OpenWorld() : World("openworld")
{
    srand(time(NULL));

    // spawn bots
    for (size_t i = 0; i < 100; ++i)
    {
        SpawnBot();
    }

    // initial twingo
    VehicleTuning twingo_tuning;
    twingo_tuning.model = "twingo";
    twingo_tuning.primary_color = 0x0077FF;
    twingo_tuning.wheels_idx = 1; // enkei
    twingo_tuning.wheel_color = 0x00FF00;

    auto& veh = Spawn<game::DrivableVehicle>(twingo_tuning);
    veh.SetPosition({110.0f, 100.0f, 5.0f});

    constexpr size_t in_row = 20;

    for (size_t i = 0; i < 100; ++i)
    {
        Schedule(i * 40, [this, i] {
            size_t col = i % in_row;
            size_t row = i / in_row;
            glm::vec3 pos(62.0f + static_cast<float>(col) * 4.0f, 165.0f + static_cast<float>(row) * 7.0f, 7.0f);

            auto& veh = SpawnRandomVehicle();
            veh.SetPosition(pos);
        });
    }

}

void game::OpenWorld::Update(int64_t delta_time)
{
    World::Update(delta_time);

}

void game::OpenWorld::PlayerJoined(Player& player)
{
    CreatePlayerCharacter(player);
}

void game::OpenWorld::PlayerInput(Player& player, PlayerInputType type, bool enabled)
{
    auto character = player_characters_.at(&player);

    switch (type)
    {
    case IN_DEBUG1:
        if (enabled)
        {
            if (character->GetVehicle())
                character->GetVehicle()->SetPosition({100.0f, 100.0f, 5.0f});
            else
                character->SetPosition({100.0f, 100.0f, 5.0f});
        }
        break;

    case IN_DEBUG2:
        if (enabled)
            CreatePlayerCharacter(player);
        break;

    default:
        character->ProcessInput(type, enabled);
        break;
    }
}

void game::OpenWorld::PlayerViewAnglesChanged(Player& player, float yaw, float pitch)
{
    auto character = player_characters_.at(&player);
    character->SetForwardYaw(yaw);
}

void game::OpenWorld::PlayerLeft(Player& player)
{
    RemovePlayerCharacter(player);
}

void game::OpenWorld::DestructibleDestroyed(net::ObjNum num, std::unique_ptr<MapObjectCollision> col)
{
    auto& destroyed_obj = Spawn<DestroyedObject>(std::move(col));

    Schedule(120000, [this, num] {
        RespawnObj(num);
    });
}

template <class T, typename... TArgs>
static T& SpawnRandomCharacter(game::OpenWorld& world, TArgs&&... args)
{
    auto& character = world.Spawn<T>(std::forward<TArgs>(args)...);

    // add clothes
    character.AddClothes("tshirt", GetRandomColor());
    character.AddClothes("shorts", GetRandomColor());

    return character;
}

void game::OpenWorld::CreatePlayerCharacter(Player& player)
{
    RemovePlayerCharacter(player);

    auto& character = SpawnRandomCharacter<PlayerCharacter>(*this, player);
    // character.SetNametag("player (" + std::to_string(character.GetEntNum()) + ")");
    character.SetPosition({100.0f, 100.0f, 5.0f});

    player_characters_[&player] = &character;
}

void game::OpenWorld::RemovePlayerCharacter(Player& player)
{
    auto it = player_characters_.find(&player);
    if (it != player_characters_.end())
    {
        it->second->Remove();
        player_characters_.erase(it);
    }
}

game::DrivableVehicle& game::OpenWorld::SpawnRandomVehicle()
{
    game::VehicleTuning tuning;
    tuning.model = GetRandomCarModel();
    tuning.primary_color = GetRandomColor24();

    auto& vehicle = Spawn<game::DrivableVehicle>(tuning);
    // vehicle.SetNametag("bot (" + std::to_string(vehicle.GetEntNum()) + ")");

    return vehicle;
}

void game::OpenWorld::SpawnBot()
{
    auto roads = GetMap().GetGraph("roads");

    if (!roads)
    {
        throw std::runtime_error("SpawnBot: no roads graph in map");
    }

    size_t start_node = rand() % roads->nodes.size();

    auto& vehicle = SpawnRandomVehicle();
    vehicle.SetPosition(roads->nodes[start_node].position + glm::vec3{0.0f, 0.0f, 5.0f});

    auto& driver = SpawnRandomCharacter<NpcCharacter>(*this);
    driver.SetVehicle(&vehicle, 0);
}
