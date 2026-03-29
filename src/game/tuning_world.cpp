#include "tuning_world.hpp"
#include "player_character.hpp"
#include "utils/colors.hpp"
#include "assets/vehicle_tuning_list.hpp"
#include "game.hpp"

game::TuningWorld::TuningWorld(Game& game, EnterableWorld& exit_world, const glm::vec3& exit_pos, float exit_yaw, std::string mapname) : 
    Super(std::move(mapname)), game_(game), exit_world_(exit_world), exit_pos_(exit_pos), exit_yaw_(exit_yaw)
{
}

void game::TuningWorld::PlayerInput(Player& player, PlayerInputType type, bool enabled)
{
    if (&player == player_ || !enabled || type != IN_USE)
        return;

    // passenger wants exit
    game_.MovePlayerToWorld(player, exit_world_, false, exit_pos_, exit_yaw_);
}

void game::TuningWorld::OnVehicleJoined(DrivableVehicle& vehicle)
{
    if (IsOccupied())
        throw std::runtime_error("vehicle joined to occupied tuning garage");
        
    auto driver = dynamic_cast<PlayerCharacter*>(vehicle.GetPassenger(0));    
    if (!driver)
        throw std::runtime_error("vehicle joined to tuning without player driver??");
        
    auto player = driver->GetPlayer();
    if (!player)
        throw std::runtime_error("vehicle joined to tuning without ACTIVE player driver??");
        
    vehicle_ = &vehicle;
    tuning_list_ = &vehicle.GetModel()->GetTuningList();
    player_ = player;

    Setup();
}

void game::TuningWorld::OnPlayerLeaving(Player& player)
{
    if (&player != player_)
        return;

    Reset();
}

void game::TuningWorld::Setup()
{
    tuning_ = vehicle_->GetTuning();
    UpdateTuningVals();
    DisplayTuningMenu();
}

void game::TuningWorld::UpdateTuningVals()
{
    tun_primary_color_ = ColorU32ToU8Vec3(tuning_.primary_color);

    tun_wheel_idx_ = tuning_.wheels_idx;
    tun_wheel_color_ = ColorU32ToU8Vec3(tuning_.wheel_color);
}

void game::TuningWorld::UpdateTuning()
{
    tuning_.primary_color = ColorU8Vec3ToU32(tun_primary_color_);

    tuning_.wheels_idx = tun_wheel_idx_;
    tuning_.wheel_color = ColorU8Vec3ToU32(tun_wheel_color_);

    vehicle_->SetTuning(tuning_);
}

static void AddColorChannelSlider(game::RemoteMenu& menu, uint8_t& ch, std::string name, std::function<void()> on_change)
{
    auto& slider = menu.AddItem(game::RM_SELECT, std::move(name));
    
    auto on_select = [&slider, &ch, on_change = std::move(on_change)] (int dir) {
        int new_val = ch + dir * 5;
        if (new_val < 0)
            ch = 0;
        else if (new_val > 255)
            ch = 255;
        else
            ch = new_val;

        slider.SetSelection(std::to_string(ch));
        on_change();
    };
    
    on_select(0);
    slider.SetOnSelect(on_select);
}

static void AddColorSliders(game::RemoteMenu& menu, glm::u8vec3& color, std::string name, std::function<void()> on_change)
{
    AddColorChannelSlider(menu, color.r, name + " ^f00R", on_change);
    AddColorChannelSlider(menu, color.g, name + " ^0f0G", on_change);
    AddColorChannelSlider(menu, color.b, name + " ^00fB", on_change);
}

static void AddWheelTypeSlider(game::RemoteMenu& menu, const assets::VehicleTuningList& tuning_list, size_t& idx, std::function<void()> on_change)
{
    auto& slider = menu.AddItem(game::RM_SELECT, "kola");
    
    auto on_select = [&slider, &idx, &tuning_list, on_change = std::move(on_change)] (int dir) {
        auto& wheels = tuning_list.wheels;

        if (dir < 0 && idx == 0)
            return;

        if (dir > 0 && (idx + 1) >= wheels.size())
            return;

        idx += dir;

        slider.SetSelection(tuning_list.wheels[idx].displayname);
        on_change();
    };
    
    on_select(0);
    slider.SetOnSelect(on_select);
}

void game::TuningWorld::DisplayTuningMenu()
{
    // display menu
    auto& menu = player_->DisplayMenu("tuning");
    menu_ = &menu;

    auto on_change =  [this] { UpdateTuning(); };

    AddColorSliders(menu, tun_primary_color_, "primární", on_change);

    if (!tuning_list_->wheels.empty())
    {
        AddWheelTypeSlider(menu, *tuning_list_, tun_wheel_idx_, on_change);
        AddColorSliders(menu, tun_wheel_color_, "kola", on_change);
    }

    auto& exit_btn = menu.AddItem(RM_BUTTON, "vylézt");
    exit_btn.SetOnClick([this] {
        Reset();
    });
}

void game::TuningWorld::Reset()
{
    if (player_)
    {
        player_->CloseMenu(*menu_);
        game_.MovePlayerToWorld(*player_, exit_world_, true, exit_pos_, exit_yaw_);
    }

    player_ = nullptr;
    vehicle_ = nullptr;
    tuning_list_ = nullptr;
    menu_ = nullptr;
}
