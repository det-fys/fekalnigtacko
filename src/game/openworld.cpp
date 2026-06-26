#include "openworld.hpp"

#include <iostream>

#include "assets/cache.hpp"
#include "player.hpp"
#include "vehicle.hpp"
#include "player_character.hpp"
#include "npc_character.hpp"
#include "drivable_vehicle.hpp"
#include "destroyed_object.hpp"
#include "marker.hpp"
#include "tuning_world.hpp"
#include "game.hpp"
#include "cow.hpp"
#include "utils/random.hpp"

namespace game
{

} // namespace game

static const char* GetRandomCarModel()
{
    const char* vehicles[] = {"pickup_hd", "passat", "twingo", "polskifiat", "avia"};
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

game::OpenWorld::OpenWorld(Game& game) : EnterableWorld("openworld"), game_(game)
{
    SetSpawnPoint(glm::vec3(100.0f, 100.0f, 1.0f));

    // initial twingo
    VehicleSpawnInfo twingo_info{};
    twingo_info.tuning.model = "twingo";
    twingo_info.tuning.parts["primarycolor"] = "orange";
    // twingo_tuning.primary_color = 0x0077FF;
    // twingo_tuning.wheels_idx = 1; // enkei
    // twingo_tuning.wheel_color = 0x00FF00;
    twingo_info.position = glm::vec3{110.0f, 100.0f, 5.0f};

    auto& veh = Spawn<game::DrivableVehicle>(twingo_info);

    constexpr size_t in_row = 20;

    for (size_t i = 0; i < 100; ++i)
    {
        Schedule(i * 160, [this, i] {
            size_t col = i % in_row;
            size_t row = i / in_row;
            glm::vec3 pos(62.0f + static_cast<float>(col) * 4.0f, 165.0f + static_cast<float>(row) * 7.0f, 7.0f);

            SpawnRandomVehicle(pos, 0.0f);
        });
    }

    daytime_offset_ = static_cast<float>(rand() % 24);

    for (auto locs = GetMap().GetLocations("tuning"); const auto& loc : locs)
    {
        CreateTuningGarage(loc.transform.position, glm::eulerAngles(loc.transform.rotation).x);
    }

    CreatePermaItemPickups("airrifle");
    CreatePermaItemPickups("airsniper");
    CreatePermaItemPickups("ak47");
    CreatePermaItemPickups("uzi");
    CreatePermaItemPickups("panzerschreck");

    SpawnNpcs();

    // cow
    auto& cow = Spawn<Cow>(glm::vec3(0.0f, 0.0f, 2.0f), 0.0f);
    cow.SetNametag("no ty krávo");

    // hit target npc
    auto& npc = SpawnRandomNpc();
    npc.SetPosition({90.0f, 100.0f, 5.0f});
    npc.SetWeapon(std::make_shared<ItemInstance>("ak47"));

    // hit target npc 2
    auto& npc2 = SpawnRandomNpc();
    npc2.SetPosition({80.0f, 100.0f, 5.0f});
    npc2.SetWeapon(std::make_shared<ItemInstance>("airsniper"));
}

void game::OpenWorld::Update(int64_t delta_time)
{
    Super::Update(delta_time);

    const float day_seconds_irl = 1800.0f;
    const float timespeed = 24.0f / day_seconds_irl;
    SetDayTime(static_cast<float>(GetTime()) * 0.001f * timespeed + daytime_offset_); 
}

void game::OpenWorld::PlayerInput(Player& player, PlayerInputType type, bool enabled)
{
    if (type == IN_DEBUG2 && enabled)
    {
        RecoverPlayer(player);
        return;
    }

    Super::PlayerInput(player, type, enabled);
}

void game::OpenWorld::SpawnNpcs()
{
    int64_t next_spawn_after = 10000;

    if (num_npcs_ < 120)
    {
        SpawnNpcVehicleWithPassengers();
        next_spawn_after = RandomInt(100, 1000);
    }

    Schedule(next_spawn_after, [this]{
        SpawnNpcs();
    });
}

static void CheckVehicleAbandonment(game::DrivableVehicle& vehicle)
{
    if (vehicle.IsAbandoned(60000 * 5)) // 5 min
    {
        vehicle.Remove();
    }
    else
    {
        vehicle.Schedule(RandomInt(0, 1000) + 10000, [&vehicle]{
            CheckVehicleAbandonment(vehicle);
        });
    }
}

game::DrivableVehicle& game::OpenWorld::SpawnRandomVehicle(const glm::vec3& pos, float yaw, bool auto_despawn)
{
    VehicleSpawnInfo vehicle_info;
    vehicle_info.tuning.model = GetRandomCarModel();
    vehicle_info.position = pos;
    vehicle_info.yaw = yaw;
    // tuning.primary_color = GetRandomColor24();

    auto& vehicle = Spawn<game::DrivableVehicle>(vehicle_info);
    // vehicle.SetNametag("bot (" + std::to_string(vehicle.GetEntNum()) + ")");

    auto& tuning = vehicle_info.tuning;
    auto& tuning_list = vehicle.GetTuningList();

    // make random tuning 
    std::vector<std::string> suitable_part_ids;
    for (const auto& group : tuning_list->groups)
    {
        suitable_part_ids.clear();

        bool add_nonstock = rand() % 100 < 3;

        for (const auto& part : group.parts)
        {
            if (part.second.stock || add_nonstock)
                suitable_part_ids.push_back(part.first);
        }
        
        if (suitable_part_ids.empty())
            continue;

        size_t random_part = rand() % suitable_part_ids.size();
        tuning.parts[group.id] = suitable_part_ids[random_part];
    }

    // auto& colors = tuning_list->groups[0].parts;

    // size_t random_color = rand() % colors.size();

    // auto item = colors.begin();
    // std::advance( item, random_color);

    // tuning.parts["primarycolor"] = item->second.id;

    vehicle.SetTuning(tuning);

    if (auto_despawn)
    {
        CheckVehicleAbandonment(vehicle);
    }

    return vehicle;
}

static void CheckNpcBoredom(game::NpcCharacter& npc)
{
    if (!npc.IsAlive())
        return;

    if (npc.IsBored(60000 * 5)) // 5 min doing nothing is insufferable
    {
        npc.Die();
    }
    else
    {
        npc.Schedule(RandomInt(0, 2000) + 10000, [&npc]{
            CheckNpcBoredom(npc);
        });
    }
}

game::NpcCharacter& game::OpenWorld::SpawnRandomNpc()
{
    HumanCharacterTuning npc_tuning;
    npc_tuning.clothes.push_back({ "tshirt", GetRandomColor24() });
    npc_tuning.clothes.push_back({ "shorts", GetRandomColor24() });

    auto& npc = Spawn<NpcCharacter>(npc_tuning);

    npc.SetOnDeath([this, &npc] {
        npc.Schedule(15000, [&npc]{
            npc.Remove();
        });

        if (num_npcs_ > 0)
        {
            --num_npcs_;
        }
    });

    npc.Schedule(1000, [&npc]{
        CheckNpcBoredom(npc);
    });

    ++num_npcs_;

    return npc;
}

static std::tuple<glm::vec3, float> GetRandomNodeAndRotation(const assets::MapGraph& graph)
{
    size_t node_idx = rand() % graph.nodes.size();
    auto& node = graph.nodes[node_idx];

    size_t nb_idx = graph.nbs[rand() % node.num_nbs];
    auto& nb = graph.nodes[nb_idx];

    auto dir = node.position - nb.position;
    float yaw = glm::atan(-dir.x, dir.y);

    return std::make_tuple(node.position, yaw);
}

void game::OpenWorld::SpawnNpcVehicleWithPassengers()
{
    auto roads = GetMap().GetGraph("roads");

    if (!roads)
    {
        throw std::runtime_error("SpawnNpcVehicleWithPassengers: no roads graph in map");
    }

    auto [pos, yaw] = GetRandomNodeAndRotation(*roads);
    auto& vehicle = SpawnRandomVehicle(pos, yaw, true);

    auto& driver = SpawnRandomNpc();
    driver.Ride(&vehicle, 0);

    if (Chance(0.5f))
    {
        driver.SetWeapon(std::make_shared<ItemInstance>("uzi"));
    }

    if (Chance(0.3f))
    {
        auto& passenger = SpawnRandomNpc();
        passenger.Ride(&vehicle, 1);

        if (Chance(0.5f))
        {
            passenger.SetWeapon(std::make_shared<ItemInstance>(Chance(0.4f) ? "ak47" : (Chance(0.5f) ? "uzi" : "airsniper")));
        }
    }
}

void game::OpenWorld::CreateTuningGarage(const glm::vec3& position, float yaw)
{
    auto garage = std::make_shared<TuningWorld>(game_, *this, position, yaw, "garage");
    game_.AddWorld(garage.get());

    MarkerInfo marker_info{};
    marker_info.position = position;
    marker_info.type = MARKER_VEHICLE;
    marker_info.color = 0x884400;
    marker_info.model = "marker_tuning";

    auto& marker = Spawn<Marker>(marker_info);
    marker.SetUseTarget("vject do tunírny", 
        [garage](PlayerCharacter& character, UseTargetQueryResult& res) {
            
            auto player = character.GetPlayer();          
            auto vehicle = character.GetVehicle();
            
            if (!vehicle)
            {
                res.enabled = false;
                res.error_text = "nemáš vehikl";
                return true;
            }

            if (vehicle->GetPassenger(0) != &character)
            {
                return false; // not driver
            }

            if (garage->IsOccupied())
            {
                res.enabled = false;
                res.error_text = "někdo tam už oxiduje";
                return true;
            }

            res.enabled = true;
            res.error_text = nullptr;
            res.delay = 0.2f;
            return true;
        },
        [this, garage, &marker](PlayerCharacter& character) {
            auto player = character.GetPlayer();
            game_.MovePlayerToWorld(*player, *garage, true, glm::vec3(0.0f), 0.0f);
            marker.SetNametag(player->GetName());
        }
    );

    garage->SetOnExit([&marker]() {
        marker.SetNametag(std::string());
    });
}

void game::OpenWorld::CreatePermaItemPickups(const std::string& item_name)
{
    for (auto locs = GetMap().GetLocations("pickup_" + item_name); const auto& loc : locs)
    {
        CreatePermaItemPickup(loc.transform.position, item_name);
    }

#ifndef NDEBUG
    for (auto locs = GetMap().GetLocations("pickup_" + item_name + "_debug"); const auto& loc : locs)
    {
        CreatePermaItemPickup(loc.transform.position, item_name);
    }
#endif
}

void game::OpenWorld::CreatePermaItemPickup(const glm::vec3& position, const std::string& item_name)
{
    auto item_def = assets::CacheManager::GetItem("data/" + item_name + ".item");
    CreateItemPickup(position, std::make_shared<ItemInstance>(item_def), 0, 5000, item_def->clip_size * 15);
}

void game::OpenWorld::RecoverPlayer(Player& player)
{
    auto character = GetPlayerCharacter(player);
    if (!character)
        return;

    auto vehicle = character->GetVehicle();

    if (!vehicle)
    {
        auto pos = character->GetRoot().GetGlobalPosition();
        glm::vec3 recovery;
        if (GetRecoveryPosition(pos, recovery))
        {
            character->SetPosition(recovery);
        }
        else
        {
            player.SendChat("nejsi pod zemí");
        }
        return;
    }

    if (vehicle->GetPassenger(0) != character)
        return; // not driver

    auto pos = vehicle->GetRoot().GetGlobalPosition();
    glm::vec3 recovery;
    if (GetRecoveryPosition(pos, recovery))
    {
        vehicle->SetPosition(recovery);
    }
    else
    {
        player.SendChat("nejsi pod zemí");
    }
}

static bool RecoveryRaycast(btCollisionWorld& bt_world, const glm::vec3& pos, glm::vec3& hit)
{
    btVector3 bt_from(pos.x, pos.y, 100.0f);
    btVector3 bt_to(pos.x, pos.y, -100.0f);
    btCollisionWorld::ClosestRayResultCallback cb(bt_from, bt_to);
    bt_world.rayTest(bt_from, bt_to, cb);

    if (!cb.hasHit())
        return false;

    hit = glm::vec3(cb.m_hitPointWorld.x(), cb.m_hitPointWorld.y(), cb.m_hitPointWorld.z());
    return true;
}

bool game::OpenWorld::GetRecoveryPosition(const glm::vec3& current, glm::vec3& recovery)
{
    glm::vec3 start = current;
    start = glm::max(start, glm::vec3(-2500.0f, -2500.0f, -1000.0f));
    start = glm::min(start, glm::vec3(3400.0f, 3100.0f, 1000.0f));

    if (!RecoveryRaycast(GetBtWorld(), start, recovery))
    {
        recovery = glm::vec3(0.0f, 0.0f, 5.0f);
        return true;

    }

    if (recovery.z - 5.0f < current.z)
    {
        return false; // already above ground
    }

    recovery.z += 5.0f;
    return true;
}
