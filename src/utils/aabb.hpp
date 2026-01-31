#pragma once

#include <glm/glm.hpp>
#include <limits>

template <size_t dim>
struct AABB
{
    static constexpr float C_INFINITY = std::numeric_limits<float>::infinity();

    using Vec = glm::vec<dim, float, glm::defaultp>;

    Vec min = Vec(C_INFINITY);
    Vec max = Vec(-C_INFINITY);

    AABB() = default;
    AABB(const Vec& min, const Vec& max) : min(min), max(max) {}

    void AddPoint(const Vec& point)
    {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    bool CollidesWith(const AABB<dim>& other) const;

    AABB<dim> Intersection(const AABB<dim>& other) const
    {
        Vec new_min = glm::max(min, other.min);
        Vec new_max = glm::min(max, other.max);
        return AABB<dim>(new_min, new_max);
    }
};

template <>
inline bool AABB<2>::CollidesWith(const AABB<2>& other) const
{
    return (min.x <= other.max.x && max.x >= other.min.x) && (min.y <= other.max.y && max.y >= other.min.y);
}

template <>
inline bool AABB<3>::CollidesWith(const AABB<3>& other) const
{
    return (min.x <= other.max.x && max.x >= other.min.x) && (min.y <= other.max.y && max.y >= other.min.y) &&
           (min.z <= other.max.z && max.z >= other.min.z);
}

using AABB2 = AABB<2>;
using AABB3 = AABB<3>;
