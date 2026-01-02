#include "world.hpp"

#include <stdexcept>

#include "assets/cache.hpp"
#include "utils/allocnum.hpp"

static std::shared_ptr<const assets::Map> LoadMapByName(const std::string& mapname)
{
    return assets::CacheManager::GetMap("data/" + mapname + ".map");
}

game::World::World(std::string mapname) : DynamicsWorld(LoadMapByName(mapname)), mapname_(std::move(mapname)) {}

net::EntNum game::World::GetNewEntnum()
{
    auto entnum = utils::AllocNum(ents_, last_entnum_);

    if (!entnum)
        throw std::runtime_error("Max entities reached");

    return entnum;
}

void game::World::RegisterEntity(std::unique_ptr<Entity> ent)
{
    auto& entslot = ents_[ent->GetEntNum()];

    if (entslot)
        throw std::runtime_error("Attempted to register entity with an occupied entnum");

    entslot = std::move(ent);
}

void game::World::Update(int64_t delta_time)
{
    time_ms_ += delta_time;
    GetBtWorld().stepSimulation(static_cast<float>(delta_time) * 1000.0f);

    for (auto& [entnum, ent] : ents_)
    {
        ent->Update();
    }
}
