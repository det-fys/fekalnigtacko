#include "game.hpp"

#include "openworld.hpp"
#include "player.hpp"
#include "player_character.hpp"

static constexpr glm::vec3 openworld_spawn(100.0f, 100.0f, 1.0f);
static constexpr glm::vec3 test_spawn(0.0f, 0.0f, 0.1f);

static uint32_t GetRandomColor24()
{
    uint8_t r, g, b;
    r = rand() % 256;
    g = rand() % 256;
    b = rand() % 256;
    return (b << 16) | (g << 8) | r;
}

game::Game::Game()
{
    openworld_ = std::make_shared<OpenWorld>();
    all_worlds_.push_back(openworld_.get());

    testworld_ = std::make_shared<EnterableWorld>("testarena");
    all_worlds_.push_back(testworld_.get());
    
    garage_ = std::make_shared<TuningWorld>(*this, *openworld_, glm::vec3(0.0f), 0.0f, "garage");
    all_worlds_.push_back(garage_.get());
}

void game::Game::Update()
{
    for (auto world : all_worlds_)
    {
        world->Update(40);
    }
}

void game::Game::FinishFrame()
{
    for (auto world : all_worlds_)
    {
        world->FinishFrame();
    }
}

void game::Game::PlayerJoined(Player& player)
{
    BroadcastChat(player.GetName() + "^r se připoojil jupí jupí jupííí");

    players_.insert({&player, PlayerGameInfo(player)});
    auto& player_info = players_.at(&player);
    player_info.world = openworld_.get();
    player.SetWorld(openworld_.get());

    CharacterTuning tuning{};
    tuning.clothes.push_back({"tshirt", GetRandomColor24()});
    tuning.clothes.push_back({"shorts", GetRandomColor24()});

    openworld_->InsertPlayer(player, tuning, openworld_spawn, 0.0f);
}

void game::Game::PlayerViewAnglesChanged(Player& player, float yaw, float pitch)
{
    auto world = FindPlayerWorld(player);
    if (world)
        world->PlayerViewAnglesChanged(player, yaw, pitch);
}

void game::Game::PlayerInput(Player& player, PlayerInputType type, bool enabled)
{
    switch (type)
    {
    case IN_DEBUG2: {
        if (!enabled)
            return;

        // auto& player_info = players_.at(&player);

        // if (player_info.world == openworld_.get())
        // {
        //     MovePlayerToWorld(player_info, testworld_.get(), test_spawn, 0.0f, true);
        // }
        // else
        // {
        //     MovePlayerToWorld(player_info, openworld_.get(), openworld_spawn, 0.0f, true);
        // }

        MovePlayerToTuning(player);

        break;
    }

    case IN_DEBUG3:
        DisplayTestMenu(player);
        break;

    default: {
        auto world = FindPlayerWorld(player);
        if (world)
            world->PlayerInput(player, type, enabled);

        break;
    }
    }
}

void game::Game::PlayerLeft(Player& player)
{
    auto world = FindPlayerWorld(player);
    if (world)
        world->OnPlayerLeaving(player);
    
    // find again as may have changed
    world = FindPlayerWorld(player);
    if (world)
        world->RemovePlayer(player);

    players_.erase(&player);

    BroadcastChat(player.GetName() + "^r se vodpojil zmrd");
}

void game::Game::MovePlayerToWorld(Player& player, EnterableWorld& world, bool with_vehicle, const glm::vec3& pos,
                                   float yaw)
{
    auto& player_info = GetPlayerInfo(player);
    MovePlayerToWorld(player_info, &world, pos, yaw, true);
}

void game::Game::BroadcastChat(const std::string& text)
{
    for (auto& [player, info] : players_)
    {
        player->SendChat(text);
    }
}

game::PlayerCharacter& game::Game::MovePlayerToWorld(PlayerGameInfo& player_info, EnterableWorld& new_world,
                                                     const glm::vec3& pos, float yaw)
{
    auto& player = player_info.player;
    auto& old_world = *player_info.world;

    auto old_character = old_world.GetPlayerCharacter(player); 
    auto& tuning = old_character->GetTuning();
    old_world.RemovePlayer(player);

    player.SetWorld(&new_world);
    auto& new_character = new_world.InsertPlayer(player, tuning, pos, yaw);

    player_info.world = &new_world;

    return new_character;
}

void game::Game::MoveVehicleToWorld(DrivableVehicle& vehicle, EnterableWorld& new_world, const glm::vec3& pos,
                                    float yaw)
{
    auto& tuning = vehicle.GetTuning();
    auto& new_vehicle = new_world.Spawn<DrivableVehicle>(tuning);
    new_vehicle.SetPosition(pos);
    // TODO: yaw

    // move passengers
    size_t num_seats = vehicle.GetNumSeats();
    for (size_t i = 0; i < num_seats; ++i)
    {
        auto passenger = vehicle.GetPassenger(i);
        if (!passenger)
            continue; // empty seat

        auto player_passenger = dynamic_cast<PlayerCharacter*>(passenger);
        if (!player_passenger)
            continue; // not player but npc, will be ejected automatically upon vehicle deletion

        auto player = player_passenger->GetPlayer();
        if (!player)
            continue; // moved already or sth

        auto& player_info = GetPlayerInfo(*player);

        auto& new_character = MovePlayerToWorld(player_info, new_world, glm::vec3(0.0f), 0.0f);
        new_character.SetVehicle(&new_vehicle, i);
    }

    vehicle.Remove();

    new_world.OnVehicleJoined(new_vehicle);
}

void game::Game::MovePlayerToWorld(PlayerGameInfo& player_info, EnterableWorld* new_world, const glm::vec3& pos,
                                   float yaw, bool with_vehicle)
{
    auto& player = player_info.player;
    auto& world = *player_info.world;

    DrivableVehicle* vehicle = nullptr;

    if (with_vehicle)
    {
        auto character = world.GetPlayerCharacter(player);
        vehicle = character->GetVehicle();
    }

    if (vehicle)
        MoveVehicleToWorld(*vehicle, *new_world, pos, yaw);
    else
        MovePlayerToWorld(player_info, *new_world, pos, yaw);

    player_info.world = new_world;
}

game::PlayerGameInfo& game::Game::GetPlayerInfo(Player& player)
{
    return players_.at(&player);
}

game::EnterableWorld* game::Game::FindPlayerWorld(Player& player) const
{
    auto it = players_.find(&player);
    if (it == players_.end())
        return nullptr;

    return it->second.world;
}

void game::Game::DisplayTestMenu(Player& player)
{
    if (player.HasOpenMenu())
        return;

    auto& menu = player.DisplayMenu("test");

    auto& btn_echo = menu.AddItem(RM_BUTTON, "echo");
    btn_echo.SetOnClick([&player] {
        player.SendChat("echo test");
    });

    auto& btn_bc = menu.AddItem(RM_BUTTON, "broadcast");
    btn_bc.SetOnClick([this, &player] {
        BroadcastChat(player.GetName() + "^r mele hovna");
    });

    int test = 0;
    auto& sel_test = menu.AddItem(RM_SELECT, "výběr");
    sel_test.SetOnSelect([test, &sel_test] (int dir) mutable {
        test += dir;
        sel_test.SetSelection(std::to_string(test));
    });
    sel_test.SetSelection(std::to_string(test));


    auto& btn_close = menu.AddItem(RM_BUTTON, "zavřít");
    btn_close.SetOnClick([&menu, &player] {
        player.CloseMenu(menu);
    });

}

void game::Game::MovePlayerToTuning(Player& player)
{
    auto& player_info = GetPlayerInfo(player);
    
    if (player_info.world != openworld_.get())
        return;

    if (garage_->IsOccupied())
    {
        player.SendChat("bohužel tam teď oxiduje nějakej píčus " + garage_->GetOccupantName() + "^r!");
        return;
    }

    auto character = player_info.world->GetPlayerCharacter(player);
    if (!character)
        return;

    auto vehicle = character->GetVehicle();
    if (!vehicle)
    {
        player.SendChat("nemáš vehikl!!!");
        return;
    }

    if (vehicle->GetPassenger(0) != character)
    {
        player.SendChat("nejsi ridič!!");
        return;
    }

    MovePlayerToWorld(player_info, garage_.get(), glm::vec3(0.0f), 0.0f, true);
}
