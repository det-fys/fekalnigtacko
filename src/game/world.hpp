#pragma once

#include <concepts>

#include "assets/map.hpp"
#include "collision/dynamicsworld.hpp"
#include "entity.hpp"
#include "net/defs.hpp"
#include "player_input.hpp"

namespace game
{

class World : public collision::DynamicsWorld
{
public:
    World(std::string mapname);
    DELETE_COPY_MOVE(World)

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

    virtual void Update(int64_t delta_time);

    // events
    virtual void PlayerJoined(Player& player) {}
    virtual void PlayerInput(Player& player, PlayerInputType type, bool enabled) {}
    virtual void PlayerViewAnglesChanged(Player& player, float yaw, float pitch) {}
    virtual void PlayerLeft(Player& player) {}

    Entity* GetEntity(net::EntNum entnum);

    const assets::Map& GetMap() const { return *map_; }
    const std::string& GetMapName() const { return mapname_; }
    const std::map<net::EntNum, std::unique_ptr<Entity>>& GetEntities() const { return ents_; }
    const int64_t& GetTime() const { return time_ms_; }

    virtual ~World() = default;

private:
    std::shared_ptr<const assets::Map> map_;
    std::string mapname_;
    std::map<net::EntNum, std::unique_ptr<Entity>> ents_;
    net::EntNum last_entnum_ = 0;

    int64_t time_ms_ = 0;
};

} // namespace game