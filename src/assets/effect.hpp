#pragma once

#include <vector>

#include "gfx/texture.hpp"
#include "audio/sound.hpp"

namespace assets
{

enum ParticleBlendType
{
    PTB_NONE,
    PTB_BLEND_NORMAL,
    PTB_BLEND_ADDITIVE,
};

struct ParticleDef
{
    std::shared_ptr<const gfx::Texture> texture;
    ParticleBlendType blend = PTB_NONE;
    float size_min = 1.0f;
    float size_max = 1.0f;
    size_t count_min = 1;
    size_t count_max = 1;
    float velocity_min = 1.0f;
    float velocity_max = 1.0f;
    float max_dispersion = 0.0f;
    float gravity_min = 1.0f;
    float gravity_max = 1.0f;
    float lifetime_min = 1.0f;
    float lifetime_max = 1.0f;
    float fadetime_min = 1.0f;
    float fadetime_max = 1.0f;
};

class Effect
{
public:
    Effect() = default;
    static std::shared_ptr<const Effect> LoadFromFile(const std::string& path);

    const std::vector<ParticleDef>& GetParticleDefs() const { return particle_defs_; }
    const std::vector<std::shared_ptr<const audio::Sound>>& GetSounds() const { return sounds_; }

private:

    std::vector<ParticleDef> particle_defs_;
    std::vector<std::shared_ptr<const audio::Sound>> sounds_;

};



}