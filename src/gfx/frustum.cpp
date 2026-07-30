#include "frustum.hpp"

gfx::Frustum::Frustum(const glm::mat4& vp)
{
    for (int i = 0; i < 4; ++i)
    {
        planes_[0][i] = vp[i][3] + vp[i][0];
        planes_[1][i] = vp[i][3] - vp[i][0];
        planes_[2][i] = vp[i][3] + vp[i][1];
        planes_[3][i] = vp[i][3] - vp[i][1];
        planes_[4][i] = vp[i][3] + vp[i][2];
        planes_[5][i] = vp[i][3] - vp[i][2];
    }

    // normalize the planes
    for (int i = 0; i < 6; i++)
    {
        auto len = glm::length(glm::vec3(planes_[i]));
        planes_[i] /= len;
    }

    // compute AABB
    auto inv_vp = glm::inverse(vp);

    static const glm::vec3 corners[8] = {glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(1.0f, -1.0f, -1.0f),
                                         glm::vec3(-1.0f, 1.0f, -1.0f),  glm::vec3(1.0f, 1.0f, -1.0f),
                                         glm::vec3(-1.0f, -1.0f, 1.0f),  glm::vec3(1.0f, -1.0f, 1.0f),
                                         glm::vec3(-1.0f, 1.0f, 1.0f),   glm::vec3(1.0f, 1.0f, 1.0f)};

    for (int i = 0; i < 8; ++i)
    {
        glm::vec4 corner_ws = inv_vp * glm::vec4(corners[i], 1.0f);
        corner_ws /= corner_ws.w;

        aabb_.AddPoint(glm::vec3(corner_ws));
    }
}

bool gfx::Frustum::IsAABBVisible(const AABB3& aabb) const
{
    glm::vec3 extents = (aabb.max - aabb.min) * 0.5f;
    glm::vec3 center = aabb.min + extents;

    for (int i = 0; i < 6; i++)
    {
        const auto& plane = planes_[i];
        const glm::vec3 normal(plane);

        const float r = glm::dot(extents, glm::abs(normal));
        const float d = glm::dot(normal, center) + plane.w;

        if (d + r < 0.0f)
            return false;
    }

    return true;
}

bool gfx::Frustum::IsSphereVisible(const Sphere& sphere) const
{
    for (int i = 0; i < 6; ++i)
    {
        const glm::vec4 plane = planes_[i];
        const glm::vec3 normal(plane);

        const float distance = glm::dot(normal, sphere.center) + plane.w;

        if (distance < -sphere.radius)
            return false;
    }

    return true;
}
