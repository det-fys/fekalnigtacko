#pragma once

#include <concepts>

#include "assets/map.hpp"
#include "collision/dynamicsworld.hpp"
#include "entity.hpp"
#include "net/defs.hpp"

namespace game
{

class World : public collision::DynamicsWorld
{
public:
    World(std::string mapname);

    // spawn entity of type T
    template <std::derived_from<Entity> T, typename... TArgs>
    T& Spawn(TArgs&&... args)
    {
        auto ent = std::make_unique<T>(*this, std::forward<TArgs>(args)...);
        auto& ref = *ent;
        RegisterEntity(std::move(ent));
        return ref;
    }

    net::EntNum GetNewEntnum();
    void RegisterEntity(std::unique_ptr<Entity> ent);

    void Update(int64_t delta_time);

    const std::string& GetMapName() const { return mapname_; }
    const std::map<net::EntNum, std::unique_ptr<Entity>>& GetEntities() const { return ents_; }
    int64_t GetTime() const { return time_ms_; }

private:
    std::string mapname_;
    std::map<net::EntNum, std::unique_ptr<Entity>> ents_;
    net::EntNum last_entnum_ = 0;

    int64_t time_ms_ = 0;
};

} // namespace game