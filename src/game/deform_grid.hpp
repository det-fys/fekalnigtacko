#pragma once

#include <vector>
#include <span>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include "gfx/deform_grid_info.hpp"

namespace game
{

class DeformGrid
{
public:
    DeformGrid(const gfx::DeformGridInfo& info);

    void ApplyImpulse(const glm::vec3& pos, const glm::vec3& impulse, float radius);

    const gfx::DeformGridInfo& GetInfo() const { return info_; }

    std::span<glm::i8vec3> GetData() { return data_; }
    std::span<const glm::i8vec3> GetData() const { return data_; }

private:
    glm::i8vec3 PackOffset(const glm::vec3& offset);
    glm::vec3 UnpackOffset(const glm::i8vec3& packed);

    size_t GetTexelIndex(const glm::ivec3& pos) const { return static_cast<size_t>(pos.x + (pos.y + pos.z * info_.res.y) * info_.res.x); }

private:
    const gfx::DeformGridInfo info_;
    std::vector<glm::i8vec3> data_;

};



}