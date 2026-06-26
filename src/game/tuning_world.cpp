#include "tuning_world.hpp"
#include "player_character.hpp"
#include "utils/colors.hpp"
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
    tuning_list_ = vehicle.GetTuningList().get();
    player_ = player;

    vehicle_->SetInvulnerable(true);
    vehicle_->SetDestroyedRemoveTime(0);

    Setup();
}

void game::TuningWorld::OnPlayerLeaving(Player& player)
{
    if (&player != player_)
        return;

    Reset();
}

const std::string& game::TuningWorld::GetOccupantName() const
{
    return player_->GetName();
}

void game::TuningWorld::Setup()
{
    tuning_ = vehicle_->GetTuning();
    OpenMainTuningMenu();
}

void game::TuningWorld::OpenMainTuningMenu()
{
    CloseMenu();

    // display menu
    auto& menu = player_->DisplayMenu("tuning");
    menu_ = &menu;

    size_t i = 0;

    for (const auto& group : tuning_list_->groups)
    {
        auto& btn = menu.AddItem(game::RM_BUTTON, group.displayname);

        btn.SetOnClick([this, i, &group]() {
            main_menu_hover_ = i;
            OpenGroupMenu(group);
        });

        ++i;
    }

    menu.SetOnExit([this]() {
        Reset();
    });

    menu.SetHoveredIdx(main_menu_hover_);
}

void game::TuningWorld::OpenGroupMenu(const VehicleTuningGroup& group)
{
    CloseMenu();

    preview_tuning_ = tuning_;

    auto& menu = player_->DisplayMenu(group.displayname);
    menu_ = &menu;

    int current_idx = -1;
    int i = 0;

    std::string state_text;

    for (const auto& [part_id, part] : group.parts)
    {
        auto& btn = menu.AddItem(RM_BUTTON, part.displayname);

        btn.SetOnHovered([this, &group, &part_id]() {
            preview_tuning_.parts[group.id] = part_id;
            vehicle_->SetTuning(preview_tuning_);
        });

        btn.SetOnClick([this, &btn, &group_id = group.id, &part_id]() {
            std::string prev_part_id = GetCurrentPartId(group_id);

            if (prev_part_id == part_id)
                return;
                
            tuning_.parts[group_id] = part_id; // apply

            std::string state_text;
            
            if (mounted_part_menu_item_)
            {
                GetPartState(group_id, prev_part_id, &state_text);
                mounted_part_menu_item_->SetSelection(state_text);
            }
            
            GetPartState(group_id, part_id, &state_text);
            btn.SetSelection(state_text);

            mounted_part_menu_item_ = &btn;

            // play vrtačka sound
            vehicle_->PlaySound("tuning");
        });

        if (GetPartState(group.id, part_id, &state_text))
        {
            current_idx = i;
            mounted_part_menu_item_ = &btn;
        }

        btn.SetSelection(state_text);

        ++i;
    }

    if (current_idx >= 0)
    {
        menu.SetHoveredIdx(current_idx);
    }

    menu.SetOnExit([this]() {
        mounted_part_menu_item_ = nullptr;
        vehicle_->SetTuning(tuning_); // undo preview
        OpenMainTuningMenu();
    });

}

std::string game::TuningWorld::GetCurrentPartId(const std::string& group_id)
{
    auto part_it = tuning_.parts.find(group_id);
    if (part_it != tuning_.parts.end())
    {
        return part_it->second;
    }

    return std::string();
}

bool game::TuningWorld::GetPartState(const std::string& group_name, const std::string& part_id,
                                     std::string* state_text)
{
    bool mounted = part_id == GetCurrentPartId(group_name);

    if (state_text)
    {
        *state_text = mounted ? "aktuální" : "0 Kč";
    }

    return mounted;
}


void game::TuningWorld::CloseMenu()
{
    if (!menu_)
        return;

    player_->CloseMenu(*menu_);
    menu_ = nullptr;
}

void game::TuningWorld::Reset()
{
    if (player_)
    {
        CloseMenu();

        // make sure to undo preview tuning 
        vehicle_->SetTuning(tuning_);

        game_.MovePlayerToWorld(*player_, exit_world_, true, exit_pos_, exit_yaw_);

        if (exit_cb_)
            exit_cb_();
    }

    player_ = nullptr;
    vehicle_ = nullptr;
    tuning_list_ = nullptr;
    menu_ = nullptr;
    main_menu_hover_ = 0;
}
