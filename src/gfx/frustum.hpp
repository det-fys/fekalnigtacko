#pragma once

#include <glm/glm.hpp>
#include "utils/aabb.hpp"
#include "utils/sphere.hpp"

namespace gfx
{

class Frustum
{
public:
    Frustum(const glm::mat4& vp);

    bool IsAABBVisible(const AABB3& aabb) const;
    bool IsSphereVisible(const Sphere& sphere) const;

    const AABB3& GetAABB() const { return aabb_; }

private:
    glm::vec4 planes_[6];
    AABB3 aabb_;
};

} // namespace gfx
