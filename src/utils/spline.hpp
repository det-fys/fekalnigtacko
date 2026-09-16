#pragma once

#include <span>
#include <vector>

#include <glm/glm.hpp>

class CatmullRomSpline
{
public:
    /**
     * @param alpha
     *   0.0f = Uniform
     *   0.5f = Centripetal
     *   1.0f = Chordal
     */
    CatmullRomSpline(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3,
                     float alpha = 0.5f);

    // u in [0, 1], returning a position between p1 and p2.
    glm::vec3 Get(float u) const;

    // Analytical derivative dP/du.
    glm::vec3 GetDerivative(float u) const;

private:
    static constexpr float kEpsilon = 1e-6f;

    float CalculateKnot(float t_prev, const glm::vec3& p_prev, const glm::vec3& p_curr, float alpha) const;

    glm::vec3 SafeLerp(const glm::vec3& a, const glm::vec3& b, float t, float t_start, float t_end) const;

    glm::vec3 SafeLerpDerivative(const glm::vec3& a, const glm::vec3& b, const glm::vec3& da, const glm::vec3& db,
                                 float t, float t_start, float t_end) const;

    glm::vec3 FallbackDerivative() const;

private:
    glm::vec3 p0_;
    glm::vec3 p1_;
    glm::vec3 p2_;
    glm::vec3 p3_;

    float t0_ = 0.0f;
    float t1_ = 0.0f;
    float t2_ = 0.0f;
    float t3_ = 0.0f;
};

struct SplineFrame
{
    float arc_length = 0.0f;

    glm::vec3 pos{0.0f};
    glm::vec3 tangent{0.0f, 1.0f, 0.0f};  // Forward
    glm::vec3 normal{0.0f, 0.0f, 1.0f};   // Z-up
    glm::vec3 binormal{1.0f, 0.0f, 0.0f}; // Right
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
    static constexpr float kDefaultSpacing = 0.5f;
    static constexpr float kEpsilon = 1e-6f;

    void ResampleUniform(std::span<const SplineFrame> raw_frames, float desired_spacing);

private:
    std::vector<SplineFrame> uniform_frames_;

    float total_length_ = 0.0f;
    float spacing_ = 0.0f;
};