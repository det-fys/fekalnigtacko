#pragma once

#include <map>

#include "enterable_world.hpp"
#include "tuning_world.hpp"
#include "openworld.hpp"
#include "commands.hpp"
#include "db/db.hpp"

namespace game
{

class Player;

struct PlayerGameInfo
{
    Player& player;
    EnterableWorld* world = nullptr;

    PlayerGameInfo(Player& player) : player(player) {}
};

struct GameInfo
{
    std::unique_ptr<db::GameDatabase> db;
};

class Game
{
public:
    Game(GameInfo info);
    DELETE_COPY_MOVE(Game);

    void RegisterCommands();

    void Update();
    void FinishFrame();

    void AddWorld(World* world);

    void PlayerJoined(Player& player);
    void PlayerViewAnglesChanged(Player& player, float yaw, float pitch);
    void PlayerInput(Player& player, PlayerInputType type, bool enabled);
    void PlayerChat(Player& player, std::string_view line);
    void PlayerLeft(Player& player);

    void MovePlayerToWorld(Player& player, EnterableWorld& world, bool with_vehicle, const glm::vec3& pos, float yaw);

    db::GameDatabase& GetDb() const { return *db_; }

private:
    void UpdateDaytime();
    void UpdateWorlds();

    void BroadcastChat(const std::string& text);

    PlayerCharacter& MovePlayerToWorld(PlayerGameInfo& player_info, EnterableWorld& new_world, const glm::vec3& pos, float yaw);
    void MoveVehicleToWorld(DrivableVehicle& vehicle, EnterableWorld& new_world, const glm::vec3& pos, float yaw);

    void MovePlayerToWorld(PlayerGameInfo& player_info, EnterableWorld* new_world, const glm::vec3& pos, float yaw, bool with_vehicle = false);

    PlayerGameInfo& GetPlayerInfo(Player& player);
    EnterableWorld* FindPlayerWorld(Player& player) const;

private:
    std::unique_ptr<db::GameDatabase> db_;
    CommandList cmds_;

    std::shared_ptr<OpenWorld> openworld_;
    std::shared_ptr<EnterableWorld> testworld_;
    std::shared_ptr<TuningWorld> garage_;

    std::vector<World*> all_worlds_; // for common update etc.
    std::map<Player*, PlayerGameInfo> players_;

};

}