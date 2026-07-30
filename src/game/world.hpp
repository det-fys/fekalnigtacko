#pragma once

#include <concepts>
#include <set>

#include "mapinstance.hpp"
#include "collision/dynamicsworld.hpp"
#include "entity.hpp"
#include "net/defs.hpp"
#include "player_input.hpp"
#include "usable.hpp"
#include "item_instance.hpp"

namespace game
{

enum DamageType
{
    DAMAGE_OTHER,
    DAMAGE_BULLET,
    DAMAGE_CRASH,
    DAMAGE_EXPLOSION,
};

class HumanCharacter;

struct DamageInfo
{
    DamageType type = DAMAGE_OTHER;
    float damage = 0.0f;
    float impulse = 0.0f;
    glm::vec3 from_pos{};
    glm::vec3 impact_pos{};
    glm::vec3 normal{};
    HumanCharacter* inflictor = nullptr;
    const btCollisionObject* hit_object = nullptr;
    bool direct_hit = true;
};

struct BulletInfo
{
    HumanCharacter* shooter;
    glm::vec3 start;
    glm::vec3 end;
    float damage;
    float impulse;
};

struct ExplosionInfo
{
    HumanCharacter* inflictor;
    glm::vec3 center;
    float radius;
    float damage;
    float impulse;
    collision::ObjectCallback* direct_hit;
};

class World : public collision::DynamicsWorld, public net::MsgProducer, public net::LocalMsgProducer, public Scheduler
{
public:
    World(const collision::DynamicsWorldInfo& info, std::string mapname);
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

    virtual void DestructibleDestroyed(net::ObjNum num, std::unique_ptr<MapObjectCollision> col);

    Entity* GetEntity(net::EntNum entnum);

    void RespawnObj(net::ObjNum objnum);

    const UseTarget* GetBestUseTarget(PlayerCharacter& character, UseTargetQueryResult& res);

    const assets::Map& GetMap() const { return map_.GetMap(); }
    const std::string& GetMapName() const { return map_.GetName(); }
    const assets::MapNavMeshSet& GetNavMeshSet() const { return map_.GetNavMeshSet(); }
    const std::map<net::EntNum, std::unique_ptr<Entity>>& GetEntities() const { return ents_; }
    const int64_t& GetTime() const { return time_ms_; }
    float GetDayTime() const { return daytime_; }
    void SetDayTime(float daytime) { daytime_ = glm::mod(daytime, 24.0f); }

    bool TraceBullet(const glm::vec3& start, const glm::vec3& end, HumanCharacter* shooter, glm::vec3& out_hit_pos,
                     collision::ObjectCallback** out_hit_obj_cb = nullptr);
    void FireBullet(const BulletInfo& bullet);

    void MakeExplosion(const ExplosionInfo& explo);

    void Beam(const glm::vec3& start, const glm::vec3& end, uint32_t color, float time);
    void BeamBox(const glm::vec3& min, const glm::vec3& max, uint32_t color, float time);

    void Effect(const std::string& name, const glm::vec3& pos, const glm::vec3& dir);

    void SendChat(const std::string& text);

    void CreateItemPickup(const glm::vec3& position, std::shared_ptr<ItemInstance> item, int64_t despawn_time,
                          int64_t respawn_time, size_t ammo_count);

    void CreateCashPickup(const glm::vec3& position, int64_t amount, int64_t despawn_time, int64_t respawn_time);

    virtual ~World() = default;

private:
    void HandleContacts();

    void DestroyObject(net::ObjNum objnum, const MapObjectBreakInfo& info);

    void SendObjDestroyedMsg(net::ObjNum objnum);
    void SendObjRespawnedMsg(net::ObjNum objnum);

    const btCollisionObject* TraceBulletInternal(const glm::vec3& start, const glm::vec3& end,
                                                 game::HumanCharacter* shooter, glm::vec3& out_hit_pos,
                                                 glm::vec3* out_hit_normal = nullptr,
                                                 collision::Material* out_hit_material = nullptr);

    glm::vec3 GetGroundPosition(const glm::vec3& pos);

private:
    MapInstance map_;
    std::set<net::ObjNum> destroyed_objs_;

    std::map<net::EntNum, std::unique_ptr<Entity>> ents_;
    net::EntNum last_entnum_ = 0;

    int64_t time_ms_ = 0;
    float daytime_ = 12.0f;
};

} // namespace game