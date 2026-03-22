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
    uint32_t id;
    glm::vec3 position;
    std::string desc;

    UseTarget(Usable* usable, uint32_t id, const glm::vec3& position, std::string desc)
        : usable(usable), id(id), position(position), desc(std::move(desc))
    {
    }
};

struct UseTargetQueryResult
{
    bool enabled;
    const char* error_text;
    float delay;
};

class PlayerCharacter;

class Usable
{
public:
    Usable(const glm::mat4& ws_matrix) : matrix_(ws_matrix) {}

    const std::vector<UseTarget>& GetUseTargets() const { return use_targets_; }
    const glm::mat4& GetWSTransformMatrix() const { return matrix_; }
    
    virtual bool QueryUseTarget(PlayerCharacter& character, uint32_t target_id, UseTargetQueryResult& res) = 0;
    virtual void Use(PlayerCharacter& character, uint32_t target_id) = 0;

protected:
    std::vector<UseTarget> use_targets_;
    const glm::mat4& matrix_;
};

} // namespace game