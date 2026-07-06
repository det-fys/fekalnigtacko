#include "particle_emitter.hpp"
#include "assets/asset_manager.hpp"
#include "utils/random.hpp"
#include "utils/math.hpp"

game::view::ParticleEmitter::ParticleEmitter(audio::Player* audioplayer) : audioplayer_(audioplayer)
{
    quad_model_ = assets::AssetManager::GetInstance().Get<ModelView>("quad");
}

void game::view::ParticleEmitter::Update(float delta_time)
{
    for (auto& particle : particles_)
    {
        particle.time += delta_time;
        particle.velocity.z -= delta_time * particle.gravity;
        particle.position += particle.velocity * delta_time;

        if (particle.time > particle.fade_start)
        {
            float opacity = 1.0f - glm::clamp((particle.time - particle.fade_start) / (particle.lifetime - particle.fade_start), 0.0f, 1.0f);
            particle.color.a = opacity;
        }

        particle.size += particle.size_speed * delta_time;

        particle.rotation += particle.rotation_speed * delta_time;
    }

    // erase expired particles
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                    [this](const Particle& particle) { return particle.lifetime < particle.time; }),
                     particles_.end());
}

void game::view::ParticleEmitter::Draw(const DrawArgs& args)
{
    for (auto& particle : particles_)
    {
        // calc matrixa
        // auto forward = args.eye - particle.position;
        // auto right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f))); 
        // auto up = normalize(glm::cross(right, forward));
        auto dir = args.eye - particle.position;
        auto basis = BasisFromDir(dir);

        particle.matrix = glm::rotate(glm::mat4(
            glm::vec4(basis[0] * particle.size, 0.0f),
            glm::vec4(basis[1], 0.0f),
            glm::vec4(basis[2] * particle.size, 0.0f),
            glm::vec4(particle.position, 1.0f)
        ), particle.rotation, glm::vec3(0.0f, 1.0f, 0.0f));

        gfx::DrawSurfaceCmd cmd{};
        cmd.surface = &particle.surface;
        cmd.matrices = &particle.matrix;
        cmd.color = &particle.color;
        cmd.dist = glm::dot(dir, dir);
        args.dlist.AddSurface(cmd);

        if (particle.lightcolor.a > 0.01f)
        {
            args.dlist.AddLight(particle.position, glm::vec3(particle.lightcolor) * particle.color.a,
                                particle.lightcolor.a);
        }
    }


}

void game::view::ParticleEmitter::Emit(const std::shared_ptr<const assets::Effect>& fx, const glm::vec3& pos,
                                       const glm::vec3& dir)
{
    // spawn particles
    for (const auto& def : fx->GetParticleDefs())
    {
        size_t count = 0;

        for (const auto& p : def.probabilities)
        {
            if (Chance(p))
                ++count;
        }

        if (count <= 0)
            continue;

        // steal a quad surface from quad model
        gfx::Surface surface = quad_model_->GetSurfaces()[0];
        surface.texture = def.texture;
        surface.sflags = gfx::SF_2SIDED | gfx::SF_OBJECT_COLOR | gfx::SF_OBJECT_COLOR_MULT | gfx::SF_VERTEX_LIT | gfx::SF_TRANSLUCENT;

        // setup blending
        if (def.blend == assets::PTB_BLEND_NORMAL)
            surface.sflags |= gfx::SF_BLEND;
        else if (def.blend == assets::PTB_BLEND_ADDITIVE)
            surface.sflags |= gfx::SF_BLEND | gfx::SF_BLEND_ADDITIVE | gfx::SF_UNLIT;

        for (int i = 0; i < count; ++i)
        {
            auto& particle = particles_.emplace_back();
            particle.fx = fx;

            particle.surface = surface;
            particle.time = 0.0f;

            glm::vec3 offset_ps(RandomFloat(def.offset_min.x, def.offset_max.x),
                                RandomFloat(def.offset_min.y, def.offset_max.y),
                                RandomFloat(def.offset_min.z, def.offset_max.z));

            particle.position = pos + BasisFromDir(dir) * offset_ps;
            particle.rotation = RandomFloat(0.0f, glm::two_pi<float>());
            particle.rotation_speed = RandomFloat(def.rotation_speed_min, def.rotation_speed_max);
            particle.size = RandomFloat(def.size_min, def.size_max);
            particle.size_speed = RandomFloat(def.size_speed_min, def.size_speed_max);

            float dispersion = RandomFloat(0.0f, def.max_dispersion);
            float speed = RandomFloat(def.velocity_min, def.velocity_max);
            particle.velocity = ApplyRandomDispersion(dir, dispersion) * speed;

            particle.gravity = RandomFloat(def.gravity_min, def.gravity_max);

            particle.lifetime = RandomFloat(def.lifetime_min, def.lifetime_max);
            particle.fade_start = particle.lifetime - RandomFloat(def.fadetime_min, def.fadetime_max);

            particle.color = glm::vec4(1.0f);

            particle.lightcolor.r = RandomFloat(def.lightcolor_min.r, def.lightcolor_max.r);
            particle.lightcolor.g = RandomFloat(def.lightcolor_min.g, def.lightcolor_max.g);
            particle.lightcolor.b = RandomFloat(def.lightcolor_min.b, def.lightcolor_max.b);
            particle.lightcolor.a = RandomFloat(def.lightcolor_min.a, def.lightcolor_max.a);
        }
    }

    // play sounds
    const auto& sounds = fx->GetSounds();
    if (audioplayer_ && sounds.size() > 0)
    {
        auto sound_idx = rand() % sounds.size();

        auto fx_snd = sounds[sound_idx];

        auto snd = audioplayer_->PlaySound(fx_snd.sound, nullptr);
        snd->SetPosition(pos);
        snd->SetVolume(RandomFloat(fx_snd.volume_min, fx_snd.volume_max));
        snd->SetPitch(RandomFloat(fx_snd.pitch_min, fx_snd.pitch_max));
    }
}
