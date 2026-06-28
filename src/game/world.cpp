#include "world.hpp"

#include <iostream>
#include <stdexcept>

#include "assets/cache.hpp"
#include "collision/object_info.hpp"
#include "destroyed_object.hpp"
#include "utils/allocnum.hpp"
#include "player_character.hpp"
#include "net/utils.hpp"
#include "marker.hpp"
#include "utils/math.hpp"

game::World::World(const collision::DynamicsWorldInfo& info, std::string mapname)
    : DynamicsWorld(info), Scheduler(time_ms_), map_(*this, std::move(mapname))
{
}

void game::World::SendInitData(Player& player, net::OutMessage& msg)
{
    msg.Write(net::MapName(map_.GetName()));

    msg.Write<net::ObjCount>(destroyed_objs_.size());
    for (auto objnum : destroyed_objs_)
    {
        msg.Write(objnum);
    }
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

    HandleContacts();

    RunTasks();

    // update entities
    for (auto it = ents_.begin(); it != ents_.end();)
    {
        it->second->TryUpdate();

        if (it->second->IsRemoved())
            it = ents_.erase(it);
        else
            ++it;
    }
}

void game::World::FinishFrame()
{
    ResetMsg();
    ResetLocalMsgs();

    // reset ent msgs
    for (auto& [entnum, ent] : ents_)
    {
        ent->FinalizeFrame();
    }
}

void game::World::DestructibleDestroyed(net::ObjNum num, std::unique_ptr<MapObjectCollision> col)
{
    auto& destroyed_obj = Spawn<DestroyedObject>(std::move(col));

    Schedule(120000, [this, num] { RespawnObj(num); });
}

game::Entity* game::World::GetEntity(net::EntNum entnum)
{
    auto it = ents_.find(entnum);
    if (it == ents_.end())
        return nullptr;

    return it->second.get();
}

void game::World::RespawnObj(net::ObjNum objnum)
{
    if (destroyed_objs_.erase(objnum) > 0)
    {
        map_.SpawnObj(objnum);
        SendObjRespawnedMsg(objnum);
    }
}

const game::UseTarget* game::World::GetBestUseTarget(PlayerCharacter& character, UseTargetQueryResult& res)
{
    const float radius = 5.0f;
    
    struct UseTargetAabbCallback : public btBroadphaseAabbCallback
    {
        PlayerCharacter& character;
        glm::vec3 pos;
        const UseTarget* best_target = nullptr;
        float best_dist = std::numeric_limits<float>::max();
        UseTargetQueryResult& best_res;
        float radius = 2.0f;

        UseTargetAabbCallback(PlayerCharacter& character, UseTargetQueryResult& res) : character(character), pos(character.GetRoot().GetGlobalPosition()), best_res(res)
        {
            auto rideable_entity = dynamic_cast<Entity*>(character.GetRideable());
            if (rideable_entity)
            {
                radius = 5.0f;
                pos = rideable_entity->GetRoot().GetGlobalPosition();
            }
        }

        virtual bool process(const btBroadphaseProxy* proxy)
        {
            auto obj = reinterpret_cast<const btCollisionObject*>(proxy->m_clientObject);

            collision::ObjectType type;
            collision::ObjectFlags flags;
            collision::ObjectCallback* obj_cb;
            collision::GetObjectInfo(obj, type, flags, obj_cb);

            if ((flags & collision::OF_USABLE) == 0 || !obj_cb)
                return true;
            
            auto usable = dynamic_cast<Usable*>(obj_cb);
            if (!usable)
                return true;

            auto& matrix = usable->GetWSTransformMatrix();

            for (const auto& target : usable->GetUseTargets())
            {
                glm::vec3 pos_world = matrix * glm::vec4(target.position, 1.0f);

                float dist = glm::distance(pos, pos_world);
                if (dist < radius && dist < best_dist)
                {
                    UseTargetQueryResult res{};
                    if (!usable->QueryUseTarget(character, target.id, res))
                        continue;

                    best_res = res;
                    best_dist = dist;
                    best_target = &target;
                }
            }

            return true;
        }
    };

    UseTargetAabbCallback cb(character, res);
    btVector3 min(cb.pos.x - radius, cb.pos.y - radius, cb.pos.z - radius);
    btVector3 max(cb.pos.x + radius, cb.pos.y + radius, cb.pos.z + radius);

    GetBtBroadphase().aabbTest(min, max, cb);
    return cb.best_target;
}

bool game::World::TraceBullet(const glm::vec3& start, const glm::vec3& end, HumanCharacter* shooter,
                              glm::vec3& out_hit_pos, collision::ObjectCallback** out_hit_obj_cb)
{
    auto hit_obj = TraceBulletInternal(start, end, shooter, out_hit_pos);

    if (!hit_obj)
    {
        return false;
    }

    if (out_hit_obj_cb)
    {
        *out_hit_obj_cb = collision::GetObjectCallback(hit_obj);
    }

    return true;
}

static uint32_t GetMaterialColor(collision::Material material)
{
    switch (material)
    {
    case collision::PM_STONE:
        return 0xFFFFFF;
    case collision::PM_DIRT:
        return 0x224488;
    case collision::PM_GRASS:
        return 0x00FF00;
    case collision::PM_WOOD:
        return 0x0000FF;
    case collision::PM_METAL:
        return 0x0077FF;
    case collision::PM_GLASS:
        return 0xFF7700;
    case collision::PM_FLESH:
        return 0xFF00FF;
    default:
        return 0xFFFFFF;
    }
}

static std::string GetMaterialImpactFx(collision::Material material)
{
    switch (material)
    {
    case collision::PM_STONE:
        return "impact_stone";
    case collision::PM_DIRT:
        return "impact_dirt";
    case collision::PM_GRASS:
        return "impact_grass";
    case collision::PM_WOOD:
        return "impact_wood";
    case collision::PM_METAL:
        return "impact_metal";
    case collision::PM_GLASS:
        return "impact_glass";
    case collision::PM_FLESH:
        return "impact_flesh";
    default:
        return "impact_stone";
    }
}

void game::World::FireBullet(const BulletInfo& bullet)
{
    glm::vec3 hit_pos, hit_normal;
    collision::Material material;
    auto hit_obj = TraceBulletInternal(bullet.start, bullet.end, bullet.shooter, hit_pos, &hit_normal, &material);
    if (!hit_obj)
    {
        // Beam(bullet.start, bullet.end, 0x0044DD, 0.04f);
        return;
    }
    
    hit_normal = glm::normalize(hit_normal);

    auto obj_cb = collision::GetObjectCallback(hit_obj);
    if (obj_cb)
    {
        // apply damage
        DamageInfo damage{};
        damage.type = DAMAGE_BULLET;
        damage.damage = bullet.damage;
        damage.impulse = bullet.impulse;
        damage.from_pos = bullet.start;
        damage.impact_pos = hit_pos;
        damage.inflictor = bullet.shooter;
        damage.hit_object = hit_obj;
        damage.normal = hit_normal;
        damage.direct_hit = true;
        obj_cb->ReceiveDamage(damage);
    }

    // TODO: remove
    // const float box_extent = 0.1f;
    // BeamBox(hit_pos - box_extent, hit_pos + box_extent, GetMaterialColor(material), 1.0f);
    
    // Beam(bullet.start, hit_pos, 0x0044DD, 0.04f);

    // effect
    Effect(GetMaterialImpactFx(material), hit_pos, hit_normal);
}

void game::World::MakeExplosion(const ExplosionInfo& explo)
{
    struct ToDestroy
    {
        net::ObjNum num = 0;
        MapObjectBreakInfo break_info{};
    };

    static std::vector<ToDestroy> to_destroy;

    struct ExplosionAabbCallback : btBroadphaseAabbCallback
    {
        const ExplosionInfo& explo;

        ExplosionAabbCallback(const ExplosionInfo& explo) : explo(explo)
        {
        }
        
        virtual bool process(const btBroadphaseProxy* proxy)
        {
            auto obj = reinterpret_cast<const btCollisionObject*>(proxy->m_clientObject);
            auto flags = collision::GetObjectFlags(obj);
            auto obj_cb = collision::GetObjectCallback(obj);

            if (!obj_cb || (flags & (collision::OF_EXPLOSION_DAMAGE | collision::OF_DESTRUCTIBLE)) == 0)
                return true;

            auto& world_trans = obj->getWorldTransform();
            auto bt_pos = world_trans.getOrigin();
            bt_pos += world_trans.getBasis() * btVector3(0.0f, 0.0f, 0.7f);
            glm::vec3 pos(bt_pos.x(), bt_pos.y(), bt_pos.z());

            auto distance = glm::distance(explo.center, pos);
            if (distance > explo.radius && obj_cb != explo.direct_hit)
                return true; // out of radius 

            auto factor = glm::max(0.1f, 1.0f - (distance / explo.radius));
            float impulse = explo.impulse * factor;
            auto normal = glm::normalize(explo.center - pos);

            if (flags & collision::OF_DESTRUCTIBLE)
            {
                auto col = dynamic_cast<MapObjectCollision*>(obj_cb);
                if (!col)
                    return true;

                if (impulse > col->GetDestroyThreshold())
                {
                    auto& td = to_destroy.emplace_back();
                    td.num = col->GetNum();
                    td.break_info.from_pos = explo.center;
                    td.break_info.hit_pos = pos;
                    td.break_info.impulse = impulse;
                }
            }
            
            if (flags & collision::OF_EXPLOSION_DAMAGE)
            {
                DamageInfo damage{};
                damage.type = DAMAGE_EXPLOSION;
                damage.damage = explo.damage * factor;
                damage.impulse = impulse;
                damage.from_pos = explo.center;
                damage.impact_pos = pos;
                damage.inflictor = explo.inflictor;
                damage.hit_object = obj;
                damage.normal = normal;
                damage.direct_hit = explo.direct_hit == obj_cb;
                obj_cb->ReceiveDamage(damage);
            }

            return true;
        }

    };

    ExplosionAabbCallback cb{explo};
    btVector3 min(explo.center.x - explo.radius, explo.center.y - explo.radius, explo.center.z - explo.radius);
    btVector3 max(explo.center.x + explo.radius, explo.center.y + explo.radius, explo.center.z + explo.radius);

    GetBtBroadphase().aabbTest(min, max, cb);

    for (auto td : to_destroy)
    {
        DestroyObject(td.num, td.break_info);
    }
    to_destroy.clear();

}

void game::World::Beam(const glm::vec3& start, const glm::vec3& end, uint32_t color, float time)
{
    auto msg = BeginLocalMsg(start, 500.0f, net::MSG_BEAM);
    net::WritePosition(msg, start);
    net::WritePosition(msg, end);
    net::WriteRGB(msg, color);
    msg.Write<net::BeamTimeQ>(time);
}

void game::World::BeamBox(const glm::vec3& min, const glm::vec3& max, uint32_t color, float time)
{
    const glm::vec3& p0 = min;
    const glm::vec3 p1(max.x, min.y, min.z);
    const glm::vec3 p2(max.x, max.y, min.z);
    const glm::vec3 p3(min.x, max.y, min.z);
    const glm::vec3 p4(min.x, min.y, max.z);
    const glm::vec3 p5(max.x, min.y, max.z);
    const glm::vec3& p6 = max;
    const glm::vec3 p7(min.x, max.y, max.z);

    Beam(p0, p1, color, time);
    Beam(p1, p2, color, time);
    Beam(p2, p3, color, time);
    Beam(p3, p0, color, time);

    Beam(p4, p5, color, time);
    Beam(p5, p6, color, time);
    Beam(p6, p7, color, time);
    Beam(p7, p4, color, time);

    Beam(p0, p4, color, time);
    Beam(p1, p5, color, time);
    Beam(p2, p6, color, time);
    Beam(p3, p7, color, time);
}

void game::World::Effect(const std::string& name, const glm::vec3& pos, const glm::vec3& dir)
{
    auto msg = BeginLocalMsg(pos, 400.0f, net::MSG_FX);
    msg.Write(net::ModelName(name));
    net::WritePosition(msg, pos);
    msg.Write<net::DirQ>(dir.x);
    msg.Write<net::DirQ>(dir.y);
    msg.Write<net::DirQ>(dir.z);
}

void game::World::SendChat(const std::string& text)
{
    auto msg = BeginMsg(net::MSG_CHAT);
    msg.Write(net::ChatMessage(text));
}

void game::World::CreateItemPickup(const glm::vec3& position, std::shared_ptr<ItemInstance> item, int64_t despawn_time,
                                   int64_t respawn_time, size_t ammo_count)
{
    MarkerInfo marker_info{};
    marker_info.position = position;
    marker_info.type = MARKER_PICKUP;
    marker_info.color = 0xFFFFFF;
    marker_info.model = item->def->model_name;

    auto& marker = Spawn<Marker>(marker_info);
    marker.SetUseTarget(
        "sebrat ^ccc" + item->def->displayname,
        [](PlayerCharacter& character, UseTargetQueryResult& res) {
            res.enabled = true;
            res.delay = 0.1f;
            res.error_text = nullptr;
            return true;
        },
        [this, position, item, despawn_time, respawn_time, ammo_count, &marker](PlayerCharacter& character) {
            auto player = character.GetPlayer();
            if (!player)
                return;

            character.GiveItem(item);

            if (ammo_count > 0)
            {
                character.GiveAmmo(item->def->ammo_type, ammo_count);
            }

            character.PlaySound("pickup_ammo");

            player->SendChat("sebrals ^ccc" + item->def->displayname);
            marker.SetUseable(false);
            marker.Remove();

            if (respawn_time > 0)
            {
                Schedule(respawn_time, [this, position, item_name = item->def->name, despawn_time, respawn_time, ammo_count] {
                    CreateItemPickup(position, std::make_shared<ItemInstance>(item_name), despawn_time, respawn_time, ammo_count);
                });
            }
        });

    if (despawn_time > 0)
    {
        marker.Schedule(despawn_time, [&marker]{
            marker.Remove();
        });
    }
}

static void ApplyCrashDamage(collision::ObjectCallback& obj_cb, collision::ObjectCallback* other_obj_cb, float impulse, const glm::vec3& normal, const glm::vec3& velocity)
{
    if (glm::length(impulse) < 1000.0f)
        return;

    if (normal.z < -0.707f)
        return;

    // float velocity_magnitude = glm::length(velocity);
    float dmg = glm::mix(10.0f, 100.0f, impulse * 0.0001f);

    // if (dmg < 10.0f)
    //     return;

    game::DamageInfo damage{};
    damage.type = game::DAMAGE_CRASH;
    damage.impulse = impulse;
    damage.inflictor = other_obj_cb ? other_obj_cb->GetResponsibleCharacter() : nullptr;
    damage.damage = dmg;

    if (damage.inflictor)
    {
        obj_cb.ReceiveDamage(damage);
    }
}

void game::World::HandleContacts()
{
    auto& bt_world = GetBtWorld();
    int numManifolds = bt_world.getDispatcher()->getNumManifolds();

    // destructibles
    static std::vector<net::ObjNum> to_destroy;
    to_destroy.clear();

    auto ProcessContact = [&](btRigidBody* body, btRigidBody* other_body, const btVector3& pos, const btVector3& normal,
                              float impulse) {
        collision::ObjectType type;
        collision::ObjectFlags flags;
        collision::ObjectCallback* cb;
        collision::GetObjectInfo(body, type, flags, cb);

        if (cb && (flags & collision::OF_NOTIFY_CONTACT))
        {
            
            collision::ContactInfo info{};
            info.pos = glm::vec3(pos.x(), pos.y(), pos.z());
            info.normal = glm::vec3(normal.x(), normal.y(), normal.z());
            info.impulse = impulse;
            info.other_cb = collision::GetObjectCallback(other_body);
            // info.other_velocity = glm::vec3(ov.x(), ov.y(), ov.z());
            cb->OnContact(info);
        }
        
        if (cb && (flags & collision::OF_CRASH_DAMAGE))
        {
            auto ov = other_body->getLinearVelocity();
            auto other_obj_cb = collision::GetObjectCallback(other_body);
            ApplyCrashDamage(*cb, other_obj_cb, impulse, glm::normalize(glm::vec3(normal.x(), normal.y(), normal.z())),
                             glm::vec3(ov.x(), ov.y(), ov.z()));
        }

        if (type == collision::OT_MAP_OBJECT && (flags & collision::OF_DESTRUCTIBLE))
        {
            collision::ObjectType other_type;
            collision::ObjectFlags other_flags;
            collision::ObjectCallback* other_cb;
            collision::GetObjectInfo(other_body, other_type, other_flags, other_cb);

            if ((other_flags & collision::OF_DESTRUCTING) == 0)
                return;

            auto col = dynamic_cast<MapObjectCollision*>(cb);
            if (!col)
                return;

            if (impulse > col->GetDestroyThreshold())
            {
                to_destroy.push_back(col->GetNum());
                other_body->applyCentralImpulse(-normal * impulse * 0.5f);
            }

            return;
        }
    };

    // std::cout << "Checking " << numManifolds << " manifolds for destructible collisions..." << std::endl;
    for (int i = 0; i < numManifolds; i++)
    {
        btPersistentManifold* contactManifold = bt_world.getDispatcher()->getManifoldByIndexInternal(i);

        btRigidBody* body0 = const_cast<btRigidBody*>(static_cast<const btRigidBody*>(contactManifold->getBody0()));
        btRigidBody* body1 = const_cast<btRigidBody*>(static_cast<const btRigidBody*>(contactManifold->getBody1()));

        for (int j = 0; j < contactManifold->getNumContacts(); j++)
        {
            btManifoldPoint& pt = contactManifold->getContactPoint(j);
            ProcessContact(body0, body1, pt.m_localPointA, -pt.m_normalWorldOnB, pt.getAppliedImpulse());
            ProcessContact(body1, body0, pt.m_localPointB, pt.m_normalWorldOnB, pt.getAppliedImpulse());
        }
    }

    MapObjectBreakInfo break_info{}; // TODO: some impulse??

    // destroy objs outside the loop to avoid corruption of the manifold list
    for (auto objnum : to_destroy)
    {
        DestroyObject(objnum, break_info);
    }
}

void game::World::DestroyObject(net::ObjNum objnum, const MapObjectBreakInfo& info)
{
    if (destroyed_objs_.contains(objnum))
        return;

    SendObjDestroyedMsg(objnum);
    destroyed_objs_.insert(objnum);

    auto col = map_.DestroyObj(objnum, info);
    if (col)
    {
        DestructibleDestroyed(objnum, std::move(col));
    }
}

void game::World::SendObjDestroyedMsg(net::ObjNum objnum)
{
    auto msg = BeginMsg(net::MSG_OBJDESTROY);
    msg.Write(objnum);
}

void game::World::SendObjRespawnedMsg(net::ObjNum objnum)
{
    auto msg = BeginMsg(net::MSG_OBJRESPAWN);
    msg.Write(objnum);
}

static bool IsMeOrMyRideOrOtherPassengerOfMyRide(const game::HumanCharacter* me, const btCollisionObject* obj)
{
    if (!me) // i am not
        return false;

    // is me?
    auto obj_cb = collision::GetObjectCallback(obj);
    if (!obj_cb)
        return false; // is nothing

    if (obj_cb == me)
        return true; // its me

    auto my_ride = me->GetRideable();
    if (!my_ride)
        return false; // im not riding anything

    // is my ride?
    if (&my_ride->GetEntity() == obj_cb)
        return true; // yes

    // is other passenger?
    auto character = dynamic_cast<game::HumanCharacter*>(obj_cb);
    if (!character)
        return false; // is not even human

    return character->GetRideable() == my_ride;
}

struct NotMeNotMyRideAndNotOtherPassengersOfMyRideClosestRayResultCallback : public btCollisionWorld::ClosestRayResultCallback
{
    using Super = ClosestRayResultCallback;

    NotMeNotMyRideAndNotOtherPassengersOfMyRideClosestRayResultCallback(const btVector3& rayFromWorld,
                                                                        const btVector3& rayToWorld)
        : ClosestRayResultCallback(rayFromWorld, rayToWorld)
    {
    }

    game::HumanCharacter* me = nullptr;
    int triangle_idx = 0;

    virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override
    {
        if (IsMeOrMyRideOrOtherPassengerOfMyRide(me, rayResult.m_collisionObject))
            return rayResult.m_hitFraction;
        
        triangle_idx = rayResult.m_localShapeInfo ? rayResult.m_localShapeInfo->m_triangleIndex : -1;

        return Super::addSingleResult(rayResult, normalInWorldSpace);
    }
};

const btCollisionObject* game::World::TraceBulletInternal(const glm::vec3& start, const glm::vec3& end,
                                                          game::HumanCharacter* shooter, glm::vec3& out_hit_pos,
                                                          glm::vec3* out_hit_normal,
                                                          collision::Material* out_hit_material)
{
    btVector3 bt_start(start.x, start.y, start.z);
    btVector3 bt_end(end.x, end.y, end.z);

    // find hitbone targets first
    btCollisionWorld::AllHitsRayResultCallback hitbone_cb(bt_start, bt_end);
    hitbone_cb.m_collisionFilterGroup = collision::OG_PROJECTILE;
    hitbone_cb.m_collisionFilterMask = collision::OG_HITBONES_PROXY;
    GetBtWorld().rayTest(bt_start, bt_end, hitbone_cb);

    for (size_t i = 0; i < hitbone_cb.m_collisionObjects.size(); ++i)
    {
        auto col_obj = hitbone_cb.m_collisionObjects[i];
        auto obj_cb = collision::GetObjectCallback(col_obj);
        if (!obj_cb)
            continue;

        obj_cb->ActivateHitBones();
    }

    NotMeNotMyRideAndNotOtherPassengersOfMyRideClosestRayResultCallback cb(bt_start, bt_end);
    cb.m_collisionFilterGroup = collision::OG_PROJECTILE;
    cb.m_collisionFilterMask = ~collision::OG_HITBONES_PROXY;
    cb.me = shooter;
    GetBtWorld().rayTest(bt_start, bt_end, cb);

    if (!cb.hasHit())
        return nullptr;

    out_hit_pos = glm::vec3(cb.m_hitPointWorld.x(), cb.m_hitPointWorld.y(), cb.m_hitPointWorld.z());
    
    if (out_hit_normal)
    {
        *out_hit_normal = glm::vec3(cb.m_hitNormalWorld.x(), cb.m_hitNormalWorld.y(), cb.m_hitNormalWorld.z());
    }

    // get material
    if (out_hit_material)
    {
        *out_hit_material = collision::PM_STONE;
        auto shape = cb.m_collisionObject->getCollisionShape();
        if (shape)
        {
            *out_hit_material = collision::GetShapeMaterial(*shape);

            // try to get triangle material
            if (cb.triangle_idx >= 0)
            {
                auto shape_info = collision::GetShapeInfo(*shape);
                if (shape_info && shape_info->triangle_materials.size() > cb.triangle_idx)
                {
                    *out_hit_material = shape_info->triangle_materials[cb.triangle_idx];
                }
            }
        }
    }

    return cb.m_collisionObject;
}
