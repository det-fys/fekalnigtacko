#include "map_project.hpp"

#include "assets/asset_manager.hpp"

edit::MapProject::MapProject() : dynamics_world_(), map_(dynamics_world_, "openworld")
{
    while (!map_.IsLoaded())
    {
        map_.LoadNext();
    }
}

void edit::MapProject::Update()
{
    map_.Update();
}

void edit::MapProject::Draw(const gfx::DrawContext& ctx) 
{
    //static const glm::mat4 identity(1.0f);

    map_.SetDayTime(daytime_);
    world_env_.SetDayTime(daytime_);

    map_.Draw(ctx);
    world_env_.Draw(ctx);
}

gfx::Environment edit::MapProject::GetSceneEnvironment()
{
    //gfx::Environment env{};
    //env.clear_color = glm::vec3(0.3f);
    //env.ambient_light = glm::vec3(1.0f);
    //return env;

    return world_env_.GetEnv();
}

float edit::MapProject::GetMapChunkSize()
{
    return map_.GetChunkSize();
}
