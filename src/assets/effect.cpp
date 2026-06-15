#include "effect.hpp"

#include "cmdfile.hpp"
#include "assets/cache.hpp"

std::shared_ptr<const assets::Effect> assets::Effect::LoadFromFile(const std::string& path)
{
    auto fx = std::make_shared<Effect>();

    ParticleDef* particle = nullptr;

    LoadCMDFile(path, [&](const std::string& command, std::istringstream& iss) {
        if (command == "sound")
        {
            std::string sound_name;
            iss >> sound_name;
            fx->sounds_.emplace_back(assets::CacheManager::GetSound("data/" + sound_name + ".snd"));
        }
        else if (command == "particle")
        {
            particle = &fx->particle_defs_.emplace_back();
        }
        else if (particle)
        {
            if (command == "texture")
            {
                std::string texture_name;
                iss >> texture_name;
                particle->texture = assets::CacheManager::GetTexture("data/" + texture_name + ".png");
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
            else if (command == "count")
            {
                iss >> particle->count_min >> particle->count_max;
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

        }
        else
        {
            throw std::runtime_error("Unknown or unexpected command in effect: " + command);
        }
    });

    return fx;
}