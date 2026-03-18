#pragma once

#include <concepts>
#include <set>

#include "mapinstance.hpp"
#include "collision/dynamicsworld.hpp"
#include "entity.hpp"
#include "net/defs.hpp"
#include "player_input.hpp"
#include "usable.hpp"

namespace game
{

class World : public collision::DynamicsWorld, public net::MsgProducer, public Scheduler
{
public:
    World(std::string mapname);
    DELETE_COPY_MOVE(World)

    void SendInitData(Player& player, net::OutMessage& msg);

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
    void FinishFrame();

    // events
    virtual void PlayerJoined(Player& player) {}
    virtual void PlayerInput(Player& player, PlayerInputType type, bool enabled) {}
    virtual void PlayerViewAnglesChanged(Player& player, float yaw, float pitch) {}
    virtual void PlayerLeft(Player& player) {}

    virtual void DestructibleDestroyed(net::ObjNum num, std::unique_ptr<MapObjectCollision> col) {}

    Entity* GetEntity(net::EntNum entnum);

    void RespawnObj(net::ObjNum objnum);

    const UseTarget* GetBestUseTarget(const glm::vec3& pos) const;

    const assets::Map& GetMap() const { return map_.GetMap(); }
    const std::string& GetMapName() const { return map_.GetName(); }
    const std::map<net::EntNum, std::unique_ptr<Entity>>& GetEntities() const { return ents_; }
    const int64_t& GetTime() const { return time_ms_; }

    virtual ~World() = default;

private:
    void HandleContacts();

    void DestroyObject(net::ObjNum objnum);

    void SendObjDestroyedMsg(net::ObjNum objnum);
    void SendObjRespawnedMsg(net::ObjNum objnum);

private:
    MapInstance map_;
    std::set<net::ObjNum> destroyed_objs_;

    std::map<net::EntNum, std::unique_ptr<Entity>> ents_;
    net::EntNum last_entnum_ = 0;

    int64_t time_ms_ = 0;
};

} // namespace game