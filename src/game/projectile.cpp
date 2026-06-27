#include "projectile.hpp"

#include "world.hpp"
#include "utils/math.hpp"

game::Projectile::Projectile(World& world, const ProjectileInfo& info)
    : Super(world, "data/" + info.model_name + ".mdl"), info_(info)
{
    spawn_time_ = GetWorld().GetTime();
    root_.local.position = info.start_pos;
    root_.UpdateMatrix();

    if (!info.sound_name.empty())
    {
        Schedule(80, [this, sound_name = info.sound_name]{
            PlaySound(sound_name);
        });
    }
}

void game::Projectile::UpdatePreSync()
{
    auto& world = GetWorld();
    auto alive_time = world.GetTime() - spawn_time_;
    float alive_frames = static_cast<float>(alive_time) / 40.0f;

    // update shooter
    if (info_.shooter_num)
    {
        shooter_ = dynamic_cast<HumanCharacter*>(world.GetEntity(info_.shooter_num));
        if (!shooter_)
        {
            info_.shooter_num = 0;
        }
    }

    info_.velocity.z -= info_.gravity / 25.0f;

    // step forward
    auto step = info_.velocity / 25.0f;
    auto start = root_.local.position - step * glm::min(alive_frames, 1.0f);
    auto end = root_.local.position + step; 

    // make fx
    if (!info_.fx_name.empty() && alive_time > 40)
    {
        world.Effect(info_.fx_name, root_.local.position - step * 0.7f, glm::normalize(info_.velocity));
    }

    glm::vec3 hit_pos;
    collision::ObjectCallback* hit_obj_cb = nullptr;
    if (world.TraceBullet(start, end, shooter_, hit_pos, &hit_obj_cb))
    {
        auto boom_pos = hit_pos - glm::normalize(info_.velocity) * 0.1f;

        Remove();
        world.Effect("explo", boom_pos, glm::vec3(0.0f, 0.0f, 1.0f));
        
        ExplosionInfo explo{};
        explo.center = boom_pos;
        explo.damage = info_.explo_damage;
        explo.radius = info_.explo_radius;
        explo.impulse = info_.explo_impulse;
        explo.inflictor = shooter_;
        explo.direct_hit = hit_obj_cb;
        world.MakeExplosion(explo);
        
        return;
    }

    if (alive_time > info_.lifetime || root_.local.position.z > 300.0f || root_.local.position.z < -100.0f)
    {
        Remove();
        return;
    }

    root_.local.position = end;
    root_.local.rotation = RotationTowards(info_.velocity);
    root_.UpdateMatrix();
}
