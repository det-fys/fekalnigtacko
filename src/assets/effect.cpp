#include "effect.hpp"

#include "cmdfile.hpp"

std::shared_ptr<assets::Effect> assets::Effect::Load(const std::string& name)
{
    return LoadFromFile("data/" + name + ".fx");
}

std::shared_ptr<assets::Effect> assets::Effect::LoadFromFile(const std::string& path)
{
    auto fx = std::make_shared<Effect>();

    ParticleDef* particle = nullptr;

    LoadCMDFile(path, [&](const std::string& command, CmdLineStream& iss) {
        if (command == "sound")
        {
            std::string sound_name;
            iss >> sound_name;

            auto sound = AssetManager::GetInstance().Get<audio::Sound>(sound_name);

            auto& fx_sound = fx->sounds_.emplace_back();
            fx_sound.sound = std::move(sound);
            iss >> fx_sound.volume_min >> fx_sound.volume_max >> fx_sound.pitch_min >> fx_sound.pitch_max;
        }
        else if (command == "particle")
        {
            particle = &fx->particle_defs_.emplace_back();
        
            size_t num = 0;
            iss >> num;
            for (size_t i = 0; i < num; ++i)
            {
                float probability = 1.0f;
                iss >> probability;
                particle->probabilities.push_back(probability);
            }
        
        }
        else if (particle)
        {
            if (command == "texture")
            {
                std::string texture_name;
                iss >> texture_name;
                particle->texture = AssetManager::GetInstance().Get<gfx::Texture>(texture_name);
            }
            else if (command == "blend")
            {
                std::string blend_str;
                iss >> blend_str;

                if (blend_str == "normal")
                    particle->blend = PTB_BLEND_NORMAL;
                else if (blend_str == "additive")
                    particle->blend = PTB_BLEND_ADDITIVE;
            }
            else if (command == "size")
            {
                iss >> particle->size_min >> particle->size_max;
            }
            else if (command == "sizespeed")
            {
                iss >> particle->size_speed_min >> particle->size_speed_max;
            }
            else if (command == "velocity")
            {
                iss >> particle->velocity_min >> particle->velocity_max;
            }
            else if (command == "dispersion")
            {
                iss >> particle->max_dispersion;
            }
            else if (command == "gravity")
            {
                iss >> particle->gravity_min >> particle->gravity_max;
            }
            else if (command == "lifetime")
            {
                iss >> particle->lifetime_min >> particle->lifetime_max;
            }
            else if (command == "fadetime")
            {
                iss >> particle->fadetime_min >> particle->fadetime_max;
            }
            else if (command == "offset")
            {
                auto& min = particle->offset_min;
                auto& max = particle->offset_max;
                iss >> min.x >> min.y >> min.z >> max.x >> max.y >> max.z;
            }
            else if (command == "rotationspeed")
            {
                iss >> particle->rotation_speed_min >> particle->rotation_speed_min;
            }
            else if (command == "lightcolor")
            {
                auto& min = particle->lightcolor_min;
                auto& max = particle->lightcolor_max;
                iss >> min.r >> min.g >> min.b >> min.a >> max.r >> max.g >> max.b >> max.a;
            }
        }
        else
        {
            throw std::runtime_error("Unknown or unexpected command in effect: " + command);
        }
    });

    return fx;
}