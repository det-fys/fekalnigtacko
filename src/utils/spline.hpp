#pragma once

#include <span>
#include <vector>

#include <glm/glm.hpp>

class CatmullRomSpline
{
public:
    /**
     * @param alpha
     *   0.0 = Uniform
     *   0.5 = Centripetal
     *   1.0 = Chordal
     */
    CatmullRomSpline(const glm::dvec3& p0, const glm::dvec3& p1, const glm::dvec3& p2, const glm::dvec3& p3,
                     double alpha = 0.5);

    // u in [0, 1], returning a position between p1 and p2.
    glm::dvec3 Get(double u) const;

    // Analytical derivative dP/du.
    glm::dvec3 GetDerivative(double u) const;

private:
    static constexpr double kEpsilon = 1e-6;

    double CalculateKnot(double t_prev, const glm::dvec3& p_prev, const glm::dvec3& p_curr, double alpha) const;
    glm::dvec3 SafeLerp(const glm::dvec3& a, const glm::dvec3& b, double t, double t_start, double t_end) const;
    glm::dvec3 SafeLerpDerivative(const glm::dvec3& a, const glm::dvec3& b, const glm::dvec3& da, const glm::dvec3& db,
                                  double t, double t_start, double t_end) const;
    glm::dvec3 FallbackDerivative() const;

private:
    glm::dvec3 p0_;
    glm::dvec3 p1_;
    glm::dvec3 p2_;
    glm::dvec3 p3_;

    double t0_ = 0.0;
    double t1_ = 0.0;
    double t2_ = 0.0;
    double t3_ = 0.0;
};

struct SplineFrame
{
    double arc_length = 0.0;

    glm::dvec3 pos{0.0};
    glm::dvec3 tangent{0.0, 1.0, 0.0};  // Forward
    glm::dvec3 normal{0.0, 0.0, 1.0};   // Z-up
    glm::dvec3 binormal{1.0, 0.0, 0.0}; // Right
};

class SplinePath
{
public:
    SplinePath() = default;
    void SetPoints(std::span<const glm::dvec3> points, double resolution = 0.5,
                   int samples_per_segment = 32);
    glm::dvec3 DeformVertex(const glm::dvec3& vert_pos) const;
    double GetTotalLength() const { return total_length_; }
    double GetSpacing() const { return spacing_; }

private:
    static constexpr double kEpsilon = 1e-6;

    void ResampleUniform(double desired_spacing);

private:
    std::vector<SplineFrame> raw_frames_;
    std::vector<SplineFrame> uniform_frames_;

    double total_length_ = 0.0;
    double spacing_ = 0.0;
};
