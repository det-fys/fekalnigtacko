#include "enterable_world.hpp"
#include "player_character.hpp"

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

game::EnterableWorld::EnterableWorld(std::string mapname) : World(std::move(mapname)) {}

void game::EnterableWorld::InsertPlayer(Player& player, const glm::vec3& pos, float yaw)
{
    CreatePlayerCharacter(player, pos, yaw);
}

void game::EnterableWorld::PlayerInput(Player& player, PlayerInputType type, bool enabled)
{
    auto it = player_characters_.find(&player);
    if (it == player_characters_.end())
        return;

    auto character = it->second;

    switch (type)
    {
    case IN_DEBUG1:
        if (enabled)
        {
            if (character->GetVehicle())
                character->GetVehicle()->SetPosition({100.0f, 100.0f, 5.0f});
            else
                character->SetPosition({100.0f, 100.0f, 5.0f});
        }
        break;

    case IN_DEBUG2:
        if (enabled)
            CreatePlayerCharacter(player, glm::vec3(100.0f, 100.0f, 5.0f), 0.0f);
        break;

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

game::PlayerCharacter& game::EnterableWorld::CreatePlayerCharacter(Player& player, const glm::vec3& position, float yaw)
{
    RemovePlayerCharacter(player);

    auto& character = Spawn<PlayerCharacter>(player);
    character.AddClothes("tshirt", GetRandomColor());
    character.AddClothes("shorts", GetRandomColor());

    // character.SetNametag("player (" + std::to_string(character.GetEntNum()) + ")");
    character.SetPosition(position);
    character.SetYaw(yaw);

    player_characters_[&player] = &character;
    return character;
}

void game::EnterableWorld::RemovePlayerCharacter(Player& player)
{
    auto it = player_characters_.find(&player);
    if (it != player_characters_.end())
    {
        it->second->Remove();
        player_characters_.erase(it);
    }
}
