#include "enterable_world.hpp"

#include "player_character.hpp"
#include "drivable_vehicle.hpp"

game::EnterableWorld::EnterableWorld(std::string mapname) : World(std::move(mapname)) {}

game::PlayerCharacter& game::EnterableWorld::InsertPlayer(Player& player, const CharacterTuning& tuning, const glm::vec3& pos, float yaw)
{
    return CreatePlayerCharacter(player, tuning, pos, yaw);
}

void game::EnterableWorld::PlayerInput(Player& player, PlayerInputType type, bool enabled)
{
    auto it = player_characters_.find(&player);
    if (it == player_characters_.end())
        return;

    auto character = it->second;

    switch (type)
    {
    // case IN_DEBUG1:
    //     if (enabled)
    //     {
    //         if (character->GetVehicle())
    //             character->GetVehicle()->SetPosition({100.0f, 100.0f, 5.0f});
    //         else
    //             character->SetPosition({100.0f, 100.0f, 5.0f});
    //     }
    //     break;

    default:
        character->ProcessInput(type, enabled);
        break;
    }

}

void game::EnterableWorld::PlayerViewAnglesChanged(Player& player, float yaw, float pitch)
{
    auto it = player_characters_.find(&player);
    if (it == player_characters_.end())
        return;

    auto character = it->second;

    character->SetForwardYaw(yaw);
}

void game::EnterableWorld::RemovePlayer(Player& player)
{
    RemovePlayerCharacter(player);
}

game::PlayerCharacter& game::EnterableWorld::MovePlayerToWorld(Player& player, EnterableWorld& new_world, const glm::vec3& pos, float yaw)
{
    auto old_character = player_characters_.at(&player);
    auto& tuning = old_character->GetTuning();
    RemovePlayer(player);

    player.SetWorld(&new_world);
    auto& new_character = new_world.InsertPlayer(player, tuning, pos, yaw);

    return new_character;
}

void game::EnterableWorld::MoveVehicleToWorld(DrivableVehicle& vehicle, EnterableWorld& new_world, const glm::vec3& pos,
                                              float yaw)
{
    if (&vehicle.GetWorld() != this)
        throw std::runtime_error("Attempt to move vehicle from other world");

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

        auto& new_character = MovePlayerToWorld(*player, new_world, glm::vec3(0.0f), 0.0f);
        new_character.SetVehicle(&new_vehicle, i);
    }

    vehicle.Remove();
}

game::PlayerCharacter* game::EnterableWorld::GetPlayerCharacter(Player& player)
{
    auto it = player_characters_.find(&player);
    if (it == player_characters_.end())
        return nullptr;

    return it->second;
}

game::PlayerCharacter& game::EnterableWorld::CreatePlayerCharacter(Player& player, const CharacterTuning& tuning, const glm::vec3& position, float yaw)
{
    RemovePlayerCharacter(player);

    auto& character = Spawn<PlayerCharacter>(player, tuning);
    character.SetPosition(position);
    character.SetYaw(yaw);

    player_characters_[&player] = &character;
    return character;
}

void game::EnterableWorld::RemovePlayerCharacter(Player& player)
{
    auto character = GetPlayerCharacter(player);
    if (character)
    {
        character->DetachFromPlayer();
        character->Remove();
    }
}
