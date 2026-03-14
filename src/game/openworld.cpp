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
    const char* vehicles[] = {"pickup_hd", "passat", "twingo", "polskifiat", "cow_static"};
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


game::OpenWorld::OpenWorld() : World("openworld")
{
    srand(time(NULL));

    // spawn bots
    for (size_t i = 0; i < 100; ++i)
    {
        SpawnBot();
    }

    auto& veh = Spawn<game::DrivableVehicle>("twingo", glm::vec3{1.0f, 0.8f, 0.1f});
    veh.SetPosition({110.0f, 100.0f, 5.0f});

    constexpr size_t in_row = 20;

    for (size_t i = 0; i < 100; ++i)
    {
        Schedule(i * 40, [this, i] {
            size_t col = i % in_row;
            size_t row = i / in_row;
            glm::vec3 pos(62.0f + static_cast<float>(col) * 4.0f, 165.0f + static_cast<float>(row) * 7.0f, 7.0f);

            auto& veh = Spawn<game::DrivableVehicle>(GetRandomCarModel(), GetRandomColor());
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

    Schedule(10000, [this, num] {
        RespawnObj(num);
    });
}

std::optional<std::pair<game::Usable&, const game::UseTarget&>> game::OpenWorld::GetBestUseTarget(const glm::vec3& pos) const
{
    std::optional<std::pair<Usable*, const UseTarget*>> best_target;
    float best_dist = std::numeric_limits<float>::max();

    // TODO: spatial query
    for (const auto& [entnum, ent] : GetEntities())
    {
        auto usable = dynamic_cast<Usable*>(ent.get());
        if (!usable)
            continue;

        for (const auto& target : usable->GetUseTargets())
        {
            glm::vec3 pos_world = ent->GetRoot().matrix * glm::vec4(target.position, 1.0f);

            float dist = glm::distance(pos, pos_world);
            if (dist < 3.0f && dist < best_dist)
            {
                best_dist = dist;
                best_target = std::make_pair(usable, &target);
            }
        }
    }

    if (best_target)
        return std::make_pair(std::ref(*best_target->first), *best_target->second);
    else
        return std::nullopt;

}

template <class T, typename... TArgs>
static T& SpawnRandomCharacter(game::OpenWorld& world, TArgs&&... args)
{
    auto& character = world.Spawn<T>(world, std::forward<TArgs>(args)...);

    // add clothes
    character.AddClothes("tshirt", GetRandomColor());
    character.AddClothes("shorts", GetRandomColor());

    return character;
}

void game::OpenWorld::CreatePlayerCharacter(Player& player)
{
    RemovePlayerCharacter(player);

    auto& character = SpawnRandomCharacter<PlayerCharacter>(*this, player);
    character.SetNametag("player (" + std::to_string(character.GetEntNum()) + ")");
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

static game::DrivableVehicle& SpawnRandomVehicle(game::World& world)
{
    auto roads = world.GetMap().GetGraph("roads");

    if (!roads)
    {
        throw std::runtime_error("SpawnRandomVehicle: no roads graph in map");
    }

    size_t start_node = rand() % roads->nodes.size();
    // auto color = glm::vec3{0.3f, 0.3f, 0.3f};
    auto color = GetRandomColor();
    auto& vehicle = world.Spawn<game::DrivableVehicle>(GetRandomCarModel(), color);
    // vehicle.SetNametag("bot (" + std::to_string(vehicle.GetEntNum()) + ")");
    vehicle.SetPosition(roads->nodes[start_node].position + glm::vec3{0.0f, 0.0f, 5.0f});

    return vehicle;
}

void game::OpenWorld::SpawnBot()
{
    auto& vehicle = SpawnRandomVehicle(*this);
    auto& driver = SpawnRandomCharacter<NpcCharacter>(*this);

    driver.SetVehicle(&vehicle, 0);
}
