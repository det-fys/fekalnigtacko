#include "game.hpp"

#include "openworld.hpp"
#include "player.hpp"
#include "player_character.hpp"
#include "utils/cvars.hpp"
#include "utils/chatcolors.hpp"
#include "utils/random.hpp"

CVAR(std::string, g_adminpassword, CV_CONST | CV_CONFIDENTIAL, "", 0, 64);

CVAR(float, g_daytime, CV_NONE, -1.0f, -1.0f, 24.0f);
CVAR(float, g_day_mins, CV_NONE, 1.0f);

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
    RegisterCommands();

    openworld_ = std::make_shared<OpenWorld>(*this);
    AddWorld(openworld_.get());
}

#define CMD_PREFIX_EVERYONE "^afa"
#define CMD_PREFIX_ADMIN "^f44"

void game::Game::RegisterCommands()
{
    // /help
    cmds_.RegisterCommand(
        "help",
        CMDF_NONE,
        "zobrazí seznam příkazů",
        [this](const CommandData& cmd) {
            cmd.player.SendChat("seznam příkazů: [" CMD_PREFIX_EVERYONE "všichni^r] [" CMD_PREFIX_ADMIN "admin pouze^r]");
            
            cmds_.ProcessCommads([&cmd](const Command& entry) {
                std::string msg = " - ";
                msg.append((entry.flags & CMDF_ADMIN_ONLY) ? CMD_PREFIX_ADMIN : CMD_PREFIX_EVERYONE);
                msg.append(entry.name);
                msg.append("^r: ");
                msg.append(entry.desc);
                cmd.player.SendChat(msg);
            });
        }
    );

    // /password
    cmds_.RegisterCommand(
        "password",
        CMDF_NONE,
        "přihlásíš se jako admin",
        [this](const CommandData& cmd) {
            if (g_adminpassword.Get().empty())
            {
                cmd.SendError("to tady není vedeno");
                return;
            }
            
            std::string password;
            if (!cmd.line.Read(password))
            {
                if (!cmd.player.IsAdmin())
                {
                    cmd.SendError("gde heslo?");
                    return;
                }

                cmd.player.SetAdmin(false);
                cmd.SendMessage(COL_SUCCESS "už nejsi admin");
                return;
            }

            if (cmd.player.IsAdmin())
            {
                cmd.SendError("již jsi admin");
                return;
            }

            if (password != g_adminpassword.Get())
            {
                cmd.SendError("špatný heslo");
                return;
            }

            cmd.player.SetAdmin(true);
            cmd.SendMessage(COL_SUCCESS "máš admina");
        }
    );

    // /set
    cmds_.RegisterCommand(
        "set",
        CMDF_ADMIN_ONLY,
        "nastaví nebo zobrazí proměnnou",
        [this](const CommandData& cmd) {
            std::string cvar_name, value;

            auto DumpCVar = [&cmd](const CVarBase& cvar) {
                auto val = cvar.GetString();
                if (cvar.IsConfidential())
                {
                    // replace with *
                    val = std::string(val.size(), '*');
                }

                auto col = cvar.IsConst() ? COL_VALUE_DARK : COL_VALUE;
                cmd.SendMessage(COL_LABEL + cvar.GetName() + "^r=" + (col + val));
            };

            cmd.line.Read(cvar_name);
            cmd.line.Read(value);

            if (cvar_name.empty() || value.empty())
            {
                // no value - list cvars with the prefix
                CVarRegistry::ProcessCVars([&](CVarBase& cvar) {
                    if (cvar.GetName().starts_with(cvar_name))
                    {
                        DumpCVar(cvar);
                    }

                    return false;
                });

                return;
            }

            if (!CVarRegistry::IsCVarName(cvar_name))
            {
                cmd.SendError("to neexistuje");
                return;
            }

            auto& cvar = CVarRegistry::GetCVar(cvar_name);

            if (cvar.IsConst())
            {
                cmd.SendError("to se nedá nastavit za běhu");
                return;
            }

            try
            {
                cvar.SetString(value);
            }
            catch (const std::runtime_error& e)
            {
                cmd.SendError(e.what());
                return;
            }
            
            cmd.SendMessage(COL_SUCCESS "nastaveno");
            DumpCVar(cvar);
        }
    );

    auto GetPlayerCharacter = [this](const CommandData& cmd) -> PlayerCharacter* {
        auto& player_info = GetPlayerInfo(cmd.player);
        if (!player_info.world)
        {
            cmd.SendError("nejsi ve světě");
            return nullptr;
        }

        auto character = player_info.world->GetPlayerCharacter(cmd.player);
        if (!character)
        {
            cmd.SendError("nemáš postavu");
            return nullptr;
        }

        return character;
    };

    // /god
    cmds_.RegisterCommand(
        "god",
        CMDF_ADMIN_ONLY,
        "bůh začne existovat",
        [GetPlayerCharacter](const CommandData& cmd) {
            auto character = GetPlayerCharacter(cmd);
            if (!character)
                return;

            auto invulnerable = !character->IsInvulnerable();
            character->SetInvulnerable(invulnerable);
            cmd.SendMessage(invulnerable ? COL_SUCCESS "seš nesmrtelnej" : COL_SUCCESS "už si zase smrtelník");
        }
    );

    // /lstuning
    cmds_.RegisterCommand(
        "lsvehicle",
        CMDF_NONE,
        "vypíše info o vehiklu a jeho tuning",
        [GetPlayerCharacter](const CommandData& cmd) {
            auto character = GetPlayerCharacter(cmd);
            if (!character)
                return;

            auto vehicle = character->GetVehicle();
            if (!vehicle)
            {
                cmd.SendError("nejsi ve vehiklu");
                return;
            }

            cmd.SendMessage(COL_LABEL "health^r=" COL_VALUE + std::to_string(vehicle->GetHealth()));
            cmd.SendMessage(COL_LABEL "windowhealth^r=" COL_VALUE + std::to_string(vehicle->GetWindowHealth()));
            cmd.SendMessage(COL_LABEL "speed^r=" COL_VALUE + std::to_string(vehicle->GetSpeed()));

            auto& tuning = vehicle->GetTuning();
            auto& tuning_list = vehicle->GetTuningList();
            cmd.SendMessage("TUNING - model: " COL_VALUE + tuning.model);

            for (const auto& group : tuning_list->groups)
            {
                std::string text;
                text += "\"" + group.displayname + "^r\" (" COL_LABEL + group.id + "^r): ";

                auto it = tuning.parts.find(group.id);
                if (it != tuning.parts.end())
                {
                    const auto& part_id = it->second;

                    auto group_part_it = group.parts.find(part_id);
                    if (group_part_it != group.parts.end())
                    {
                        const auto& part = group_part_it->second;

                        text += "\"" + part.displayname + "^r\" (" COL_VALUE + part.id + "^r)";
                    }
                    else
                    {
                        text += COL_ERROR + part_id + " (neexistuje)";
                    }
                }
                else
                {
                    text += COL_ERROR "nenastaveno";
                }

                cmd.SendMessage(text);
            }

        }
    );
}

void game::Game::Update()
{
    UpdateDaytime();
    UpdateWorlds();
}

void game::Game::FinishFrame()
{
    for (auto world : all_worlds_)
    {
        world->FinishFrame();
    }
}

void game::Game::AddWorld(World* world)
{
    all_worlds_.push_back(world);
}

void game::Game::PlayerJoined(Player& player)
{
    players_.insert({&player, PlayerGameInfo(player)});
    auto& player_info = players_.at(&player);
    player_info.world = openworld_.get();
    player.SetWorld(openworld_.get());
    
    HumanCharacterTuning tuning{};
    tuning.clothes.push_back({"tshirt", GetRandomColor24()});
    tuning.clothes.push_back({"shorts", GetRandomColor24()});
    
    openworld_->InsertPlayer(player, tuning, openworld_->GetSpawnPoint(), 0.0f);

    BroadcastChat(player.GetName() + "^r se připoojil jupí jupí jupííí");
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
    // case IN_DEBUG2: {
    //     if (!enabled)
    //         return;

    //     // auto& player_info = players_.at(&player);

    //     // if (player_info.world == openworld_.get())
    //     // {
    //     //     MovePlayerToWorld(player_info, testworld_.get(), test_spawn, 0.0f, true);
    //     // }
    //     // else
    //     // {
    //     //     MovePlayerToWorld(player_info, openworld_.get(), openworld_spawn, 0.0f, true);
    //     // }

    //     MovePlayerToTuning(player);

    //     break;
    // }

    // case IN_DEBUG3:
    //     // DisplayTestMenu(player);
    //     break;

    default: {
        auto world = FindPlayerWorld(player);
        if (world)
            world->PlayerInput(player, type, enabled);

        break;
    }
    }
}

void game::Game::PlayerChat(Player& player, std::string_view line)
{
    // remove leading/trailing whitespace
    line.remove_prefix(std::min(line.find_first_not_of(" \t"), line.size()));
    line.remove_suffix(line.size() - std::min(line.find_last_not_of(" \t") + 1, line.size()));

    if (line.empty())
        return;

    if (line[0] == '/')
    {
        line.remove_prefix(1);
        CmdLineStream iss(line);
        cmds_.Execute(player, iss);
        return;
    }

    BroadcastChat(player.GetName() + "^r: " + std::string(line));
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
    MovePlayerToWorld(player_info, &world, pos, yaw, with_vehicle);
}

void game::Game::UpdateDaytime()
{
    if (g_daytime.Get() < -0.1f)
    {
        g_daytime.Set(RandomFloat(0.0f, 24.0f));
    }

    float delta = 1.0f / 25.0f;
    // g_day_mins is irl minutes per ingame day
    float hours_per_second = 24.0f / (g_day_mins.Get() * 60.0f);

    g_daytime.Set(glm::mod(g_daytime.Get() + delta * hours_per_second, 24.0f));
    g_daytime.ClearModified();
}

void game::Game::UpdateWorlds()
{
    openworld_->SetDayTime(g_daytime.Get());

    for (auto world : all_worlds_)
    {
        world->Update(40);
    }
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
    auto& tuning = old_character->GetHumanTuning();
    auto inventory = old_character->TakeInventory();
    old_world.RemovePlayer(player);

    player.SetWorld(&new_world);
    auto& new_character = new_world.InsertPlayer(player, tuning, pos, yaw);
    new_character.SetInventory(std::move(inventory));

    player_info.world = &new_world;

    return new_character;
}

void game::Game::MoveVehicleToWorld(DrivableVehicle& vehicle, EnterableWorld& new_world, const glm::vec3& pos,
                                    float yaw)
{
    VehicleSpawnInfo vehicle_info{};
    vehicle_info.tuning = vehicle.GetTuning();
    vehicle_info.position = pos;
    vehicle_info.yaw = yaw;
    auto& new_vehicle = new_world.Spawn<DrivableVehicle>(vehicle_info);
    
    // move passengers
    size_t num_seats = vehicle.GetNumSeats();
    for (size_t i = 0; i < num_seats; ++i)
    {
        auto passenger = vehicle.GetPassenger(i);
        if (!passenger || !passenger->IsAlive())
            continue; // empty seat or ded

        auto player_passenger = dynamic_cast<PlayerCharacter*>(passenger);
        if (!player_passenger)
            continue; // not player but npc, will be ejected automatically upon vehicle deletion

        auto player = player_passenger->GetPlayer();
        if (!player)
            continue; // moved already or sth

        auto& player_info = GetPlayerInfo(*player);

        auto& new_character = MovePlayerToWorld(player_info, new_world, glm::vec3(0.0f), 0.0f);
        new_character.Ride(&new_vehicle, i);
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
