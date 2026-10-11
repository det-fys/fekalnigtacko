#include "spline.hpp"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>

namespace
{
constexpr double kEpsilon = 1e-6;

glm::dvec3 SafeNormalize(const glm::dvec3& v, const glm::dvec3& fallback)
{
    const auto len2 = glm::length2(v);

    if (!std::isfinite(len2) || len2 <= kEpsilon * kEpsilon)
    {
        return fallback;
    }

    return v / glm::sqrt(len2);
}

/**
 * Returns a stable vector perpendicular to tangent.
 *
 * This chooses the world axis least aligned with tangent,
 * avoiding a poorly conditioned cross product.
 */
glm::dvec3 MakePerpendicular(const glm::dvec3& tangent)
{
    const glm::dvec3 abs_t = glm::abs(tangent);

    glm::dvec3 reference;

    if (abs_t.x <= abs_t.y && abs_t.x <= abs_t.z)
    {
        reference = glm::dvec3(1.0, 0.0, 0.0);
    }
    else if (abs_t.y <= abs_t.z)
    {
        reference = glm::dvec3(0.0, 1.0, 0.0);
    }
    else
    {
        reference = glm::dvec3(0.0, 0.0, 1.0);
    }

    return SafeNormalize(glm::cross(tangent, reference), glm::dvec3(1.0, 0.0, 0.0));
}
} // namespace

// ============================================================================
// CatmullRomSpline
// ============================================================================

CatmullRomSpline::CatmullRomSpline(const glm::dvec3& p0, const glm::dvec3& p1, const glm::dvec3& p2, const glm::dvec3& p3,
                                   double alpha)
    : p0_(p0), p1_(p1), p2_(p2), p3_(p3)
{
    alpha = glm::clamp(alpha, 0.0, 1.0);

    t0_ = 0.0;
    t1_ = CalculateKnot(t0_, p0_, p1_, alpha);
    t2_ = CalculateKnot(t1_, p1_, p2_, alpha);
    t3_ = CalculateKnot(t2_, p2_, p3_, alpha);
}

double CatmullRomSpline::CalculateKnot(double t_prev, const glm::dvec3& p_prev, const glm::dvec3& p_curr,
                                       double alpha) const
{
    const double distance = glm::distance(p_prev, p_curr);

    // Prevent coincident control points from collapsing the knot
    // interval entirely.
    const double safe_distance = std::max(distance, kEpsilon);

    return t_prev + std::pow(safe_distance, alpha);
}

glm::dvec3 CatmullRomSpline::SafeLerp(const glm::dvec3& a, const glm::dvec3& b, double t, double t_start,
                                      double t_end) const
{
    const double interval = t_end - t_start;

    if (std::abs(interval) <= kEpsilon)
    {
        return a;
    }

    const double weight = (t - t_start) / interval;

    return glm::mix(a, b, weight);
}

glm::dvec3 CatmullRomSpline::SafeLerpDerivative(const glm::dvec3& a, const glm::dvec3& b, const glm::dvec3& da,
                                               const glm::dvec3& db, double t, double t_start, double t_end) const
{
    const double interval = t_end - t_start;

    if (std::abs(interval) <= kEpsilon)
    {
        return da;
    }

    const double weight = (t - t_start) / interval;

    return glm::mix(da, db, weight) + (b - a) / interval;
}

glm::dvec3 CatmullRomSpline::FallbackDerivative() const
{
    // The primary segment direction is the best fallback.
    glm::dvec3 result = p2_ - p1_;

    if (glm::length2(result) > kEpsilon * kEpsilon)
    {
        return result;
    }

    // For degenerate p1/p2, try a wider chord.
    result = p2_ - p0_;

    if (glm::length2(result) > kEpsilon * kEpsilon)
    {
        return result;
    }

    result = p3_ - p1_;

    if (glm::length2(result) > kEpsilon * kEpsilon)
    {
        return result;
    }

    result = p3_ - p0_;

    if (glm::length2(result) > kEpsilon * kEpsilon)
    {
        return result;
    }

    return glm::dvec3(0.0);
}

glm::dvec3 CatmullRomSpline::Get(double u) const
{
    u = glm::clamp(u, 0.0, 1.0);

    // Map normalized u to the actual knot interval.
    const double t = glm::mix(t1_, t2_, u);

    // Level 1.
    const glm::dvec3 A1 = SafeLerp(p0_, p1_, t, t0_, t1_);
    const glm::dvec3 A2 = SafeLerp(p1_, p2_, t, t1_, t2_);
    const glm::dvec3 A3 = SafeLerp(p2_, p3_, t, t2_, t3_);

    // Level 2.
    const glm::dvec3 B1 = SafeLerp(A1, A2, t, t0_, t2_);
    const glm::dvec3 B2 = SafeLerp(A2, A3, t, t1_, t3_);

    // Level 3.
    return SafeLerp(B1, B2, t, t1_, t2_);
}

glm::dvec3 CatmullRomSpline::GetDerivative(double u) const
{
    u = glm::clamp(u, 0.0, 1.0);

    const double dt_du = t2_ - t1_;

    if (dt_du <= kEpsilon)
    {
        return FallbackDerivative();
    }

    const double t = glm::mix(t1_, t2_, u);

    // ------------------------------------------------------------------------
    // Level 1 positions.
    // ------------------------------------------------------------------------
    const glm::dvec3 A1 = SafeLerp(p0_, p1_, t, t0_, t1_);
    const glm::dvec3 A2 = SafeLerp(p1_, p2_, t, t1_, t2_);
    const glm::dvec3 A3 = SafeLerp(p2_, p3_, t, t2_, t3_);

    // ------------------------------------------------------------------------
    // Level 1 derivatives dA/dt.
    // ------------------------------------------------------------------------
    const glm::dvec3 dA1 = SafeLerpDerivative(p0_, p1_, glm::dvec3(0.0), glm::dvec3(0.0), t, t0_, t1_);
    const glm::dvec3 dA2 = SafeLerpDerivative(p1_, p2_, glm::dvec3(0.0), glm::dvec3(0.0), t, t1_, t2_);
    const glm::dvec3 dA3 = SafeLerpDerivative(p2_, p3_, glm::dvec3(0.0), glm::dvec3(0.0), t, t2_, t3_);

    // ------------------------------------------------------------------------
    // Level 2 positions.
    // ------------------------------------------------------------------------
    const glm::dvec3 B1 = SafeLerp(A1, A2, t, t0_, t2_);
    const glm::dvec3 B2 = SafeLerp(A2, A3, t, t1_, t3_);

    // ------------------------------------------------------------------------
    // Level 2 derivatives dB/dt.
    // ------------------------------------------------------------------------
    const glm::dvec3 dB1 = SafeLerpDerivative(A1, A2, dA1, dA2, t, t0_, t2_);
    const glm::dvec3 dB2 = SafeLerpDerivative(A2, A3, dA2, dA3, t, t1_, t3_);

    // ------------------------------------------------------------------------
    // Level 3 derivative dC/dt.
    // ------------------------------------------------------------------------
    const glm::dvec3 dC_dt = SafeLerpDerivative(B1, B2, dB1, dB2, t, t1_, t2_);

    // Chain rule:
    //
    //     dP/du = dP/dt * dt/du
    //
    // and dt/du = t2 - t1.
    const glm::dvec3 result = dC_dt * dt_du;

    const double result_len2 = glm::length2(result);

    if (std::isfinite(result_len2) && result_len2 > kEpsilon * kEpsilon)
    {
        return result;
    }

    // Never inject an arbitrary world-space tangent.
    return FallbackDerivative();
}

// ============================================================================
// SplinePath
// ============================================================================

SplinePath::SplinePath(std::span<const glm::dvec3> points, int samples_per_segment)
{
    if (points.size() < 4 || samples_per_segment <= 0)
    {
        return;
    }

    std::vector<SplineFrame> raw_frames;

    raw_frames.reserve((points.size() - 3) * static_cast<size_t>(samples_per_segment + 1));

    // ------------------------------------------------------------------------
    // 1. Dense sampling.
    //
    // We first sample positions only. Tangents are computed afterwards from
    // neighboring positions. This is especially robust at the endpoints.
    // ------------------------------------------------------------------------

    double curr_length = 0.0;

    glm::dvec3 last_pos(0.0);
    bool have_last_pos = false;

    for (size_t i = 0; i < points.size() - 3; ++i)
    {
        CatmullRomSpline segment(points[i], points[i + 1], points[i + 2], points[i + 3], 0.5);

        // Avoid duplicating the shared boundary between segments.
        const int start_s = (i == 0) ? 0 : 1;

        for (int s = start_s; s <= samples_per_segment; ++s)
        {
            const double u = static_cast<double>(s) / static_cast<double>(samples_per_segment);

            const glm::dvec3 pos = segment.Get(u);

            if (!have_last_pos)
            {
                last_pos = pos;
                have_last_pos = true;
            }
            else
            {
                curr_length += glm::distance(last_pos, pos);

                last_pos = pos;
            }

            SplineFrame frame;
            frame.arc_length = curr_length;
            frame.pos = pos;

            raw_frames.push_back(frame);
        }
    }

    total_length_ = curr_length;

    if (raw_frames.size() < 2)
    {
        return;
    }

    // ------------------------------------------------------------------------
    // 2. Calculate tangents from actual sampled positions.
    //
    // Start/end use one-sided differences.
    // Interior samples use centered differences.
    //
    // This avoids the artificial (0,1,0) fallback which was causing the
    // endpoint deformation problem.
    // ------------------------------------------------------------------------

    for (size_t i = 0; i < raw_frames.size(); ++i)
    {
        glm::dvec3 tangent(0.0);

        if (i == 0)
        {
            tangent = raw_frames[1].pos - raw_frames[0].pos;
        }
        else if (i == raw_frames.size() - 1)
        {
            tangent = raw_frames[i].pos - raw_frames[i - 1].pos;
        }
        else
        {
            tangent = raw_frames[i + 1].pos - raw_frames[i - 1].pos;
        }

        raw_frames[i].tangent = SafeNormalize(tangent, glm::dvec3(1.0, 0.0, 0.0));
    }

    // ------------------------------------------------------------------------
    // 3. Build Z-up frame.
    //
    // The desired normal is world Z projected onto the plane perpendicular
    // to the curve tangent.
    //
    // N = Z - T * dot(Z, T)
    //
    // B = T x N
    //
    // This preserves "upright" geometry while remaining exactly orthogonal.
    // ------------------------------------------------------------------------

    const glm::dvec3 world_up(0.0, 0.0, 1.0);

    for (SplineFrame& frame : raw_frames)
    {
        glm::dvec3 T = SafeNormalize(frame.tangent, glm::dvec3(1.0, 0.0, 0.0));

        // Project world Z onto the plane perpendicular to T.
        glm::dvec3 N = world_up - T * glm::dot(world_up, T);

        // Your path is almost horizontal, so this should never normally
        // happen. The fallback simply keeps pathological input finite.
        N = SafeNormalize(N, MakePerpendicular(T));

        // Right vector.
        glm::dvec3 B = SafeNormalize(glm::cross(T, N), MakePerpendicular(T));

        // Recompute N so the basis is exactly orthogonal after normalization.
        N = SafeNormalize(glm::cross(B, T), world_up);

        frame.tangent = T;
        frame.normal = N;
        frame.binormal = B;
    }

    // ------------------------------------------------------------------------
    // 4. Resample by arc length.
    // ------------------------------------------------------------------------

    ResampleUniform(raw_frames, kDefaultSpacing);
}

void SplinePath::ResampleUniform(std::span<const SplineFrame> raw_frames, double desired_spacing)
{
    uniform_frames_.clear();

    if (raw_frames.size() < 2 || total_length_ <= 0.0)
    {
        spacing_ = 0.0;
        return;
    }

    desired_spacing = std::max(desired_spacing, 0.001);

    // Number of intervals required so no interval exceeds the requested
    // spacing.
    const size_t interval_count = std::max<size_t>(1, static_cast<size_t>(std::ceil(total_length_ / desired_spacing)));

    // Adjust spacing so the last frame lands EXACTLY at total_length_.
    spacing_ = total_length_ / static_cast<double>(interval_count);

    uniform_frames_.reserve(interval_count + 1);

    size_t raw_idx = 0;

    for (size_t i = 0; i <= interval_count; ++i)
    {
        // Force the final sample to be exactly the endpoint.
        const double dist = (i == interval_count) ? total_length_ : static_cast<double>(i) * spacing_;

        while (raw_idx + 1 < raw_frames.size() && raw_frames[raw_idx + 1].arc_length < dist)
        {
            ++raw_idx;
        }

        const SplineFrame& f1 = raw_frames[raw_idx];
        const SplineFrame& f2 = raw_frames[std::min(raw_idx + 1, raw_frames.size() - 1)];

        const double segment_length = f2.arc_length - f1.arc_length;
        double t = 0.0;

        if (segment_length > kEpsilon)
        {
            t = (dist - f1.arc_length) / segment_length;
        }

        t = glm::clamp(t, 0.0, 1.0);

        SplineFrame frame;

        frame.arc_length = dist;

        // Position.
        frame.pos = glm::mix(f1.pos, f2.pos, t);

        // Tangent.
        frame.tangent = SafeNormalize(glm::mix(f1.tangent, f2.tangent, t), f1.tangent);

        // Interpolate the Z-up normals, then re-project onto the tangent
        // plane to guarantee orthogonality.
        glm::dvec3 normal = glm::mix(f1.normal, f2.normal, t);

        normal -= frame.tangent * glm::dot(normal, frame.tangent);

        frame.normal = SafeNormalize(normal, f1.normal);

        // Rebuild binormal from tangent and normal.
        frame.binormal = SafeNormalize(glm::cross(frame.tangent, frame.normal), f1.binormal);

        // One final re-orthogonalization of N.
        frame.normal = SafeNormalize(glm::cross(frame.binormal, frame.tangent), frame.normal);

        uniform_frames_.push_back(frame);
    }
}

glm::dvec3 SplinePath::DeformVertex(const glm::dvec3& vert_pos) const
{
    if (uniform_frames_.empty() || spacing_ <= 0.0)
    {
        return vert_pos;
    }

    const double requested_dist = vert_pos.y;

    // Distance used for spline-frame lookup.
    const double dist = glm::clamp(requested_dist, 0.0, total_length_);
    const double frame_pos = dist / spacing_;
    const int last_index = static_cast<int>(uniform_frames_.size()) - 1;
    int idx1 = static_cast<int>(std::floor(frame_pos));
    idx1 = glm::clamp(idx1, 0, last_index);
    const int idx2 = std::min(idx1 + 1, last_index);
    double t = frame_pos - static_cast<double>(idx1);
    t = glm::clamp(t, 0.0, 1.0);

    const SplineFrame& f1 = uniform_frames_[idx1];
    const SplineFrame& f2 = uniform_frames_[idx2];

    // ------------------------------------------------------------------------
    // Interpolate position.
    // ------------------------------------------------------------------------
    glm::dvec3 pos = glm::mix(f1.pos, f2.pos, t);

    // ------------------------------------------------------------------------
    // Interpolate tangent.
    // ------------------------------------------------------------------------
    glm::dvec3 tangent = SafeNormalize(glm::mix(f1.tangent, f2.tangent, t), f1.tangent);

    // ------------------------------------------------------------------------
    // Interpolate normal and enforce orthogonality.
    // ------------------------------------------------------------------------
    glm::dvec3 normal = glm::mix(f1.normal, f2.normal, t);
    normal -= tangent * glm::dot(normal, tangent);
    normal = SafeNormalize(normal, f1.normal);

    // ------------------------------------------------------------------------
    // Recompute binormal.
    // ------------------------------------------------------------------------
    glm::dvec3 binormal = SafeNormalize(glm::cross(tangent, normal), f1.binormal);

    // Final orthogonalization.
    normal = SafeNormalize(glm::cross(binormal, tangent), normal);

    // ------------------------------------------------------------------------
    // Transform local mesh coordinates.
    //
    // X = lateral/right  -> binormal
    // Y = along path     -> distance
    // Z = height         -> normal
    // ------------------------------------------------------------------------

    glm::dvec3 result = pos + binormal * vert_pos.x + normal * vert_pos.z;

    // ------------------------------------------------------------------------
    // Linear extension before the beginning.
    //
    // Use the exact start frame rather than the clamped/interpolated frame.
    // ------------------------------------------------------------------------

    if (requested_dist < 0.0)
    {
        const SplineFrame& start = uniform_frames_.front();

        result = start.pos + start.tangent * requested_dist + start.binormal * vert_pos.x + start.normal * vert_pos.z;
    }

    // ------------------------------------------------------------------------
    // Linear extension after the end.
    // ------------------------------------------------------------------------

    else if (requested_dist > total_length_)
    {
        const SplineFrame& end = uniform_frames_.back();

        result = end.pos + end.tangent * (requested_dist - total_length_) + end.binormal * vert_pos.x +
                 end.normal * vert_pos.z;
    }

    return result;
}