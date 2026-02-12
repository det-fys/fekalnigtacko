#include "world.hpp"

#include <stdexcept>

#include "assets/cache.hpp"
#include "utils/allocnum.hpp"

game::World::World(std::string mapname) : mapname_(std::move(mapname))
{
    map_ = assets::CacheManager::GetMap("data/" + mapname_ + ".map");
    AddMapCollision(map_);
}

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
    float delta_s = static_cast<float>(delta_time) * 0.001f;
    // GetBtWorld().stepSimulation(delta_s, 1, delta_s);
    GetBtWorld().stepSimulation(delta_s, 2, delta_s * 0.5f);

    // update entities
    for (auto it = ents_.begin(); it != ents_.end();)
    {
        it->second->Update();

        if (it->second->IsRemoved())
            it = ents_.erase(it);
        else
            ++it;
    }
}

game::Entity* game::World::GetEntity(net::EntNum entnum)
{
    auto it = ents_.find(entnum);
    if (it == ents_.end())
        return nullptr;

    return it->second.get();
}
