#pragma once

#include <functional>
#include <random>
#include <span>
#include <glm/glm.hpp>

namespace mg
{

void PoissonDiscSampling(std::mt19937& gen, float radius, const glm::vec2& region_size, std::span<const glm::vec2> current_points,
                         std::function<bool(const glm::vec2&)> cb);

}
