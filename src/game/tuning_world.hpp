#pragma once

#include "enterable_world.hpp"
#include "drivable_vehicle.hpp"

namespace game
{

class Game;
class RemoteMenu;

class TuningWorld : public EnterableWorld
{
public:
    using Super = EnterableWorld;

    TuningWorld(Game& game, EnterableWorld& exit_world, const glm::vec3& exit_pos, float exit_yaw, std::string mapname);

    virtual void PlayerInput(Player& player, PlayerInputType type, bool enabled);
    virtual void OnVehicleJoined(DrivableVehicle& vehicle);
    virtual void OnPlayerLeaving(Player& player);

    void SetOnExit(std::function<void()> cb) { exit_cb_ = cb; }

    bool IsOccupied() const { return player_ != nullptr; }
    const std::string& GetOccupantName() const;

private:
    void Setup();

    void AddTuningGroupSelect(game::RemoteMenu& menu, const VehicleTuningGroup& group);
    void DisplayTuningMenu();

    void Reset();

private:
    Game& game_;
    EnterableWorld& exit_world_;
    glm::vec3 exit_pos_;
    float exit_yaw_;

     // active player
    Player* player_ = nullptr;
    DrivableVehicle* vehicle_ = nullptr;
    const VehicleTuningList* tuning_list_ = nullptr;
    
    game::RemoteMenu* menu_ = nullptr;

    VehicleTuning tuning_;

    std::function<void()> exit_cb_;


};

}