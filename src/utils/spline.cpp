#include "spline.hpp"
#include <glm/gtc/epsilon.hpp>
#include <glm/gtx/norm.hpp>

SplinePath::SplinePath(std::span<const glm::vec3> points, int samples_per_segment)
{
    if (points.size() < 4 || samples_per_segment <= 0)
        return;

    std::vector<SplineFrame> raw_frames;
    float curr_length = 0.0f;
    glm::vec3 last_pos(0.0f);

    // 1. Densely sample curve and compute analytical frames
    for (size_t i = 0; i < points.size() - 3; ++i)
    {
        CatmullRomSpline segment(points[i], points[i + 1], points[i + 2], points[i + 3], 0.5f);

        // Skip s = 0 for subsequent segments to avoid duplicate boundary points
        int start_s = (i == 0) ? 0 : 1;

        for (int s = start_s; s <= samples_per_segment; ++s)
        {
            float u = static_cast<float>(s) / samples_per_segment;
            glm::vec3 pos = segment.Get(u);

            // Analytical tangent from spline derivative
            glm::vec3 deriv = segment.GetDerivative(u);
            glm::vec3 tan = (glm::length2(deriv) > 0.00001f) ? glm::normalize(deriv) : glm::vec3(0, 1, 0);

            if (i == 0 && s == 0)
            {
                last_pos = pos;
            }
            else
            {
                curr_length += glm::distance(last_pos, pos);
                last_pos = pos;
            }

            SplineFrame frame;
            frame.arc_length = curr_length;
            frame.pos = pos;
            frame.tangent = tan;
            raw_frames.push_back(frame);
        }
    }
    total_length_ = curr_length;

    if (raw_frames.empty())
        return;

    // 2. Calculate Normals and Binormals (World Z-Up Alignment)
    const glm::vec3 world_up = glm::vec3(0.0f, 0.0f, 1.0f);

    for (size_t i = 0; i < raw_frames.size(); ++i)
    {
        glm::vec3 T = raw_frames[i].tangent;

        // Handle vertical singularity using frame propagation or fallback
        if (glm::abs(glm::dot(T, world_up)) > 0.999f)
        {
            if (i > 0)
            {
                raw_frames[i].binormal = raw_frames[i - 1].binormal;
                raw_frames[i].normal = glm::normalize(glm::cross(raw_frames[i].binormal, T));
            }
            else
            {
                raw_frames[i].binormal = glm::vec3(1.0f, 0.0f, 0.0f);
                raw_frames[i].normal = glm::normalize(glm::cross(raw_frames[i].binormal, T));
            }
        }
        else
        {
            // Binormal = T x Up, Normal = B x T
            glm::vec3 B = glm::normalize(glm::cross(T, world_up));
            glm::vec3 N = glm::cross(B, T);

            raw_frames[i].binormal = B;
            raw_frames[i].normal = N;
        }
    }

    // 3. Resample uniformly
    ResampleUniform(raw_frames, 0.5f);
}

void SplinePath::ResampleUniform(std::span<const SplineFrame> raw_frames, float desired_spacing)
{
    uniform_frames_.clear();
    spacing_ = std::max(desired_spacing, 0.001f);

    if (raw_frames.empty() || total_length_ <= 0.0f)
        return;

    size_t raw_idx = 0;
    for (float dist = 0.0f; dist <= total_length_; dist += spacing_)
    {
        while (raw_idx < raw_frames.size() - 2 && raw_frames[raw_idx + 1].arc_length < dist)
        {
            raw_idx++;
        }

        const auto& f1 = raw_frames[raw_idx];
        const auto& f2 = raw_frames[std::min(raw_idx + 1, raw_frames.size() - 1)];

        float segment_len = f2.arc_length - f1.arc_length;
        float t = (segment_len > 0.00001f) ? (dist - f1.arc_length) / segment_len : 0.0f;
        t = glm::clamp(t, 0.0f, 1.0f);

        SplineFrame interp;
        interp.arc_length = dist;
        interp.pos = glm::mix(f1.pos, f2.pos, t);
        interp.tangent = glm::normalize(glm::mix(f1.tangent, f2.tangent, t));

        // Re-orthonormalize frame basis
        glm::vec3 mixed_norm = glm::normalize(glm::mix(f1.normal, f2.normal, t));
        interp.normal = glm::normalize(mixed_norm - interp.tangent * glm::dot(mixed_norm, interp.tangent));

        // Correct handedness: B = T x N
        interp.binormal = glm::cross(interp.tangent, interp.normal);

        uniform_frames_.push_back(interp);
    }
}

glm::vec3 SplinePath::DeformVertex(const glm::vec3& vert_pos) const
{
    if (uniform_frames_.empty())
        return vert_pos;

    float dist_along_curve = glm::clamp(vert_pos.y, 0.0f, total_length_);

    float f_idx = dist_along_curve / spacing_;
    int idx1 = glm::clamp(static_cast<int>(f_idx), 0, static_cast<int>(uniform_frames_.size()) - 1);
    int idx2 = glm::clamp(idx1 + 1, 0, static_cast<int>(uniform_frames_.size()) - 1);
    float t = f_idx - static_cast<float>(idx1);

    const SplineFrame& frame1 = uniform_frames_[idx1];
    const SplineFrame& frame2 = uniform_frames_[idx2];

    glm::vec3 pos = glm::mix(frame1.pos, frame2.pos, t);
    glm::vec3 tan = glm::normalize(glm::mix(frame1.tangent, frame2.tangent, t));

    // Gram-Schmidt orthogonalization during interpolation
    glm::vec3 nrm = glm::normalize(glm::mix(frame1.normal, frame2.normal, t));
    nrm = glm::normalize(nrm - tan * glm::dot(nrm, tan));

    // Binormal = Tangent x Normal
    glm::vec3 bin = glm::cross(tan, nrm);

    // X = lateral (binormal), Z = height (normal)
    glm::vec3 deformed_pos = pos + (bin * vert_pos.x) + (nrm * vert_pos.z);

    // Linear extension beyond endpoints
    if (vert_pos.y < 0.0f || vert_pos.y > total_length_)
    {
        deformed_pos += tan * (vert_pos.y - dist_along_curve);
    }

    return deformed_pos;
}