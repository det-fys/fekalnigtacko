#include "enterable_world.hpp"

#include "player_character.hpp"
#include "drivable_vehicle.hpp"

game::EnterableWorld::EnterableWorld(std::string mapname) : World(std::move(mapname)) {}

game::PlayerCharacter& game::EnterableWorld::InsertPlayer(Player& player, const HumanCharacterTuning& tuning, const glm::vec3& pos, float yaw)
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
    //         if (character->GetVehicleOld())
    //             character->GetVehicleOld()->SetPosition({100.0f, 100.0f, 5.0f});
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

    character->SetViewAngles(yaw, pitch);
}

void game::EnterableWorld::RemovePlayer(Player& player)
{
    RemovePlayerCharacter(player);
}

game::PlayerCharacter* game::EnterableWorld::GetPlayerCharacter(Player& player)
{
    auto it = player_characters_.find(&player);
    if (it == player_characters_.end())
        return nullptr;

    return it->second;
}

game::PlayerCharacter& game::EnterableWorld::CreatePlayerCharacter(Player& player, const HumanCharacterTuning& tuning, const glm::vec3& position, float yaw)
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
    auto it = player_characters_.find(&player);
    if (it == player_characters_.end())
        return;

    auto character = it->second;
    if (character)
    {
        character->DetachFromPlayer();
        character->Remove();
    }

    player_characters_.erase(it);
}
