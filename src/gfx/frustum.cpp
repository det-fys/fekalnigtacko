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

    //glm::mat4 inv_vp = glm::inverse(m_vp);

    //m_min = glm::vec3(FLT_MAX);
    //m_max = glm::vec3(-FLT_MAX);

    //for (int x = 0; x < 2; ++x)
    //{
    //    for (int y = 0; y < 2; ++y)
    //    {
    //        for (int z = 0; z < 2; ++z)
    //        {
    //            glm::vec4 pt = inv_vp * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);

    //            pt /= pt.w;

    //            m_min = glm::min(m_min, glm::vec3(pt));
    //            m_max = glm::max(m_max, glm::vec3(pt));
    //        }
    //    }
    //}

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
