#pragma once

#include <cmath>             

#include <glm/geometric.hpp> 
#include <glm/glm.hpp>

class CatmullRomSpline
{
public:
    /**
     * @param alpha: 0.0f = Uniform, 0.5f = Centripetal (default/recommended), 1.0f = Chordal
     */
    CatmullRomSpline(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3,
                     float alpha = 0.5f)
        : p0(p0), p1(p1), p2(p2), p3(p3)
    {
        // Pre-calculate knots
        t0 = 0.0f;
        t1 = CalculateKnot(t0, p0, p1, alpha);
        t2 = CalculateKnot(t1, p1, p2, alpha);
        t3 = CalculateKnot(t2, p2, p3, alpha);
    }

    // Pass u from 0.0 to 1.0 to get a position on the curve between p1 and p2
    glm::vec3 Get(float u) const
    {
        // Clamp u to prevent extrapolation outside the p1->p2 segment
        u = glm::clamp(u, 0.0f, 1.0f);

        // Map the normalized u [0, 1] into the actual knot time window [t1, t2]
        float t = glm::mix(t1, t2, u);

        // Level 1
        glm::vec3 A1 = SafeLerp(p0, p1, t, t0, t1);
        glm::vec3 A2 = SafeLerp(p1, p2, t, t1, t2);
        glm::vec3 A3 = SafeLerp(p2, p3, t, t2, t3);

        // Level 2
        glm::vec3 B1 = SafeLerp(A1, A2, t, t0, t2);
        glm::vec3 B2 = SafeLerp(A2, A3, t, t1, t3);

        // Level 3 (Final Position)
        return SafeLerp(B1, B2, t, t1, t2);
    }

private:
    // Helper to calculate knot values based on the alpha parameter
    float CalculateKnot(float t_prev, const glm::vec3& p_prev, const glm::vec3& p_curr, float alpha) const
    {
        return t_prev + std::pow(glm::distance(p_prev, p_curr), alpha);
    }

    // Barry-Goldman interpolation step with divide-by-zero protection
    glm::vec3 SafeLerp(const glm::vec3& a, const glm::vec3& b, float t, float t_start, float t_end) const
    {
        if (t_start == t_end)
        {
            return a; // Protect against overlapping control points
        }
        float weight = (t - t_start) / (t_end - t_start);

        // glm::mix performs the actual linear interpolation
        return glm::mix(a, b, weight);
    }

private:
    glm::vec3 p0, p1, p2, p3;
    float t0, t1, t2, t3;

};