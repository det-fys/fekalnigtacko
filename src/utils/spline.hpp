#pragma once

#include <cmath>
#include <span>
#include <vector>

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

    // Calculates the analytical first derivative dP/du for u in [0, 1]
    glm::vec3 GetDerivative(float u) const
    {
        u = glm::clamp(u, 0.0f, 1.0f);

        float dt = t2 - t1;
        if (dt <= 0.0f)
        {
            return glm::vec3(0.0f);
        }

        float t = glm::mix(t1, t2, u);

        // Level 1 positions
        glm::vec3 A1 = SafeLerp(p0, p1, t, t0, t1);
        glm::vec3 A2 = SafeLerp(p1, p2, t, t1, t2);
        glm::vec3 A3 = SafeLerp(p2, p3, t, t2, t3);

        // Level 1 derivatives (d/dt)
        glm::vec3 dA1 = SafeLerpDerivative(p0, p1, glm::vec3(0.0f), glm::vec3(0.0f), t, t0, t1);
        glm::vec3 dA2 = SafeLerpDerivative(p1, p2, glm::vec3(0.0f), glm::vec3(0.0f), t, t1, t2);
        glm::vec3 dA3 = SafeLerpDerivative(p2, p3, glm::vec3(0.0f), glm::vec3(0.0f), t, t2, t3);

        // Level 2 positions
        glm::vec3 B1 = SafeLerp(A1, A2, t, t0, t2);
        glm::vec3 B2 = SafeLerp(A2, A3, t, t1, t3);

        // Level 2 derivatives (d/dt)
        glm::vec3 dB1 = SafeLerpDerivative(A1, A2, dA1, dA2, t, t0, t2);
        glm::vec3 dB2 = SafeLerpDerivative(A2, A3, dA2, dA3, t, t1, t3);

        // Level 3 derivative (d/dt)
        glm::vec3 dC_dt = SafeLerpDerivative(B1, B2, dB1, dB2, t, t1, t2);

        // Chain rule: dP/du = dP/dt * (t2 - t1)
        return dC_dt * dt;
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

    // Differentiates SafeLerp(a(t), b(t), t) with respect to t
    glm::vec3 SafeLerpDerivative(const glm::vec3& a, const glm::vec3& b, const glm::vec3& da, const glm::vec3& db,
                                 float t, float t_start, float t_end) const
    {
        float interval = t_end - t_start;
        if (interval <= 0.0f)
        {
            return da;
        }

        float weight = (t - t_start) / interval;
        return glm::mix(da, db, weight) + (b - a) / interval;
    }

private:
    glm::vec3 p0, p1, p2, p3;
    float t0, t1, t2, t3;
};

struct SplineFrame
{
    float arc_length;
    glm::vec3 pos;
    glm::vec3 tangent;  // Forward
    glm::vec3 normal;   // Up
    glm::vec3 binormal; // Right
};

class SplinePath
{
public:
    SplinePath() = default;
    SplinePath(std::span<const glm::vec3> points, int samples_per_segment = 32);

    glm::vec3 DeformVertex(const glm::vec3& vert_pos) const;

    float GetTotalLength() const { return total_length_; }
    float GetSpacing() const { return spacing_; }

private:
    void ResampleUniform(std::span<const SplineFrame> raw_frames, float desired_spacing);

private:
    std::vector<SplineFrame> uniform_frames_;
    float total_length_ = 0.0f;
    float spacing_ = 0.0f;
};
