#pragma once

#include <glm/glm.hpp>
#include <limits>

#include "transform.hpp"

template <size_t dim, typename T>
struct BasicAABB
{
    static constexpr T C_INFINITY = std::numeric_limits<T>::infinity();

    using Vec = glm::vec<dim, T, glm::defaultp>;

    Vec min = Vec(C_INFINITY);
    Vec max = Vec(-C_INFINITY);

    BasicAABB() = default;
    BasicAABB(const Vec& min, const Vec& max) : min(min), max(max) {}

    void AddPoint(const Vec& point)
    {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    void AddAABB(const BasicAABB& other)
    {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }

    BasicAABB Intersection(const BasicAABB& other) const
    {
        Vec new_min = glm::max(min, other.min);
        Vec new_max = glm::min(max, other.max);
        return BasicAABB(new_min, new_max);
    }

    bool Contains(const Vec& point) const
    {
        return glm::all(glm::greaterThanEqual(point, min)) && glm::all(glm::lessThanEqual(point, max));
    }

    bool CollidesWith(const BasicAABB& other) const
    {
        return glm::all(glm::lessThanEqual(min, other.max)) && glm::all(glm::greaterThanEqual(max, other.min));
    }
};

using AABB2 = BasicAABB<2, float>;
using AABB3 = BasicAABB<3, float>;

using AABBD2 = BasicAABB<2, double>;
using AABBD3 = BasicAABB<3, double>;

inline AABB3 TransformAABB(const AABB3& aabb, const glm::mat4& mat)
{
    const glm::vec3 corners[] = {
        glm::vec3(aabb.min.x, aabb.min.y, aabb.min.z), glm::vec3(aabb.max.x, aabb.min.y, aabb.min.z),
        glm::vec3(aabb.min.x, aabb.max.y, aabb.min.z), glm::vec3(aabb.max.x, aabb.max.y, aabb.min.z),
        glm::vec3(aabb.min.x, aabb.min.y, aabb.max.z), glm::vec3(aabb.max.x, aabb.min.y, aabb.max.z),
        glm::vec3(aabb.min.x, aabb.max.y, aabb.max.z), glm::vec3(aabb.max.x, aabb.max.y, aabb.max.z),
    };

    AABB3 new_aabb;
    
    for (size_t i = 0; i < 8; ++i)
    {
        glm::vec3 p = mat * glm::vec4(corners[i], 1.0f);
        new_aabb.AddPoint(p);
    }
    
    return new_aabb;
}

inline AABB3 TransformAABB(const AABB3& aabb, const FarMatrix& mat)
{
    return TransformAABB(aabb, mat.ToNearMatrix(glm::dvec3(0.0)));
}