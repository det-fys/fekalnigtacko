#pragma once

#include "enterable_world.hpp"
#include "drivable_vehicle.hpp"
#include "remote_menu.hpp"

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

    void OpenMainTuningMenu();
    void OpenGroupMenu(const VehicleTuningGroup& group);
    std::string GetCurrentPartId(const std::string& group_id);
    bool GetPartState(const std::string& group_id, const std::string& part_id, std::string* state_text); // mounted?
    void CloseMenu();

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

    size_t main_menu_hover_ = 0;

    VehicleTuning tuning_;
    VehicleTuning preview_tuning_;

    RemoteMenuItem* mounted_part_menu_item_ = nullptr;

    std::function<void()> exit_cb_;


};

}