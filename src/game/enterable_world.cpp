#include "enterable_world.hpp"

#include "player_character.hpp"
#include "drivable_vehicle.hpp"

#include "utils/cvars.hpp"

CVAR(std::string, ew_navmesh_beams, CV_NONE, "", 0, 16);

game::EnterableWorld::EnterableWorld(const collision::DynamicsWorldInfo& info, std::string mapname)
    : World(info, std::move(mapname))
{
}

void game::EnterableWorld::Update(int64_t delta_time)
{
    World::Update(delta_time);

    if (!ew_navmesh_beams.Get().empty())
    {
        DrawNavMeshBeams();
    }
}

game::PlayerCharacter& game::EnterableWorld::InsertPlayer(Player& player, const HumanCharacterTuning& tuning,
                                                          const glm::vec3& pos, float yaw)
{
    return CreatePlayerCharacter(player, tuning, pos, yaw);
}

void game::EnterableWorld::PlayerInput(Player& player, PlayerInputType type, bool enabled)
{
    auto it = player_characters_.find(&player);
    if (it == player_characters_.end())
        return;

    auto character = it->second;

    // check respawn
    if (type == IN_ATTACK_PRIMARY && enabled)
    {
        auto current_character = GetPlayerCharacter(player);

        if (current_character->IsDead() && current_character->GetDeathTime() >= 3000)
        {
            auto& tuning = current_character->GetHumanTuning();
            CreatePlayerCharacter(player, tuning, spawnpoint_, 0.0f);
        }

        return;
    }

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

void game::EnterableWorld::DrawNavMeshBeams()
{
    const auto& navmesh_id = ew_navmesh_beams.Get();

    auto& navmesh_set = GetNavMeshSet();
    auto& navmesh = (navmesh_id == "vehicle") ? navmesh_set.GetVehicleNavMesh() : navmesh_set.GetPawnNavMesh();

    for (auto& [player, character] : player_characters_)
    {
        if (!character)
            continue;
        static std::vector<assets::NavMeshDebugLine> lines;
        lines.clear();
        navmesh.DrawDebug(character->GetRoot().GetGlobalPosition(), lines);
        for (const auto& line : lines)
        {
            Beam(line.start, line.end, 0x00ff00ff, 0.1f);
        }
    }
}
