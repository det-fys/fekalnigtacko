#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace game
{

class Usable;

struct UseTarget
{
    Usable* usable;
    uint32_t id = 0;
    glm::vec3 position;
    std::string desc;

    UseTarget(Usable* usable, uint32_t id, const glm::vec3& position, std::string desc)
        : usable(usable), id(id), position(position), desc(std::move(desc))
    {
    }
};

class PlayerCharacter;

class Usable
{
public:
    const std::vector<UseTarget>& GetUseTargets() const { return use_targets_; }

    virtual void Use(PlayerCharacter& character, uint32_t target_id) = 0;

protected:
    std::vector<UseTarget> use_targets_;
};

} // namespace game