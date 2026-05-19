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
    const char* vehicles[] = {"pickup_hd", "passat", "twingo", "polskifiat", "cow_static", "pig_static", "avia"};
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

game::OpenWorld::OpenWorld() : EnterableWorld("openworld")
{
    // spawn bots
    for (size_t i = 0; i < 100; ++i)
    {
        SpawnBot();
    }

    // initial twingo
    VehicleTuning twingo_tuning;
    twingo_tuning.model = "twingo";
    twingo_tuning.parts["primarycolor"] = "orange";
    // twingo_tuning.primary_color = 0x0077FF;
    // twingo_tuning.wheels_idx = 1; // enkei
    // twingo_tuning.wheel_color = 0x00FF00;

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

    daytime_offset_ = static_cast<float>(rand() % 24);

}

void game::OpenWorld::Update(int64_t delta_time)
{
    Super::Update(delta_time);

    const float timespeed = 0.05f;
    SetDayTime(static_cast<float>(GetTime()) * 0.001f * timespeed + daytime_offset_); 
}

game::DrivableVehicle& game::OpenWorld::SpawnRandomVehicle()
{
    game::VehicleTuning tuning;
    tuning.model = GetRandomCarModel();
    // tuning.primary_color = GetRandomColor24();

    auto& vehicle = Spawn<game::DrivableVehicle>(tuning);
    // vehicle.SetNametag("bot (" + std::to_string(vehicle.GetEntNum()) + ")");

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

    CharacterTuning npc_tuning;
    npc_tuning.clothes.push_back({ "tshirt", GetRandomColor24() });
    npc_tuning.clothes.push_back({ "shorts", GetRandomColor24() });

    auto& driver = Spawn<NpcCharacter>(npc_tuning);
    driver.SetVehicle(&vehicle, 0);
}
