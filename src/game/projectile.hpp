#pragma once

#include "simple_entity.hpp"
#include "human_character.hpp"

namespace game
{

struct ProjectileInfo
{
    std::string model_name;
    glm::vec3 start_pos;
    glm::vec3 velocity;
    net::EntNum shooter_num;
    int64_t lifetime;
    std::string fx_name;
    std::string sound_name;
    float explo_damage;
    float explo_radius;
    float explo_impulse;
};

class Projectile : public SimpleEntity
{
public:
    using Super = SimpleEntity;

    Projectile(World& world, const ProjectileInfo& info);

    virtual void UpdatePreSync() override;

private:
    int64_t spawn_time_ = 0;
    ProjectileInfo info_;

    HumanCharacter* shooter_ = nullptr;
};


}