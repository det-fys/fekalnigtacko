#include "common.hpp"

#include <vector>
#include <cmath>
#include <glm/gtc/constants.hpp>

void mg::PoissonDiscSampling(std::mt19937& gen, float radius, const glm::vec2& region_size,
                             std::span<const glm::vec2> current_points, std::function<bool(const glm::vec2&)> cb)
{
    if (radius <= 0.0f || region_size.x <= 0.0f || region_size.y <= 0.0f || !cb)
    {
        return;
    }

    constexpr int k = 30; // Candidate attempts per active point

    // Grid cell size ensures max 1 point per cell for the CURRENT phase radius (a = r / sqrt(2))
    const float cell_size = radius / std::sqrt(2.0f);
    const int grid_w = static_cast<int>(std::ceil(region_size.x / cell_size));
    const int grid_h = static_cast<int>(std::ceil(region_size.y / cell_size));

    // Spatial grid storing point indices (-1 means empty)
    std::vector<int> grid(grid_w * grid_h, -1);

    std::vector<glm::vec2> points;
    std::vector<size_t> active_indices;

    // Random number generation
    std::uniform_real_distribution<float> dist_x(0.0f, region_size.x);
    std::uniform_real_distribution<float> dist_y(0.0f, region_size.y);
    std::uniform_real_distribution<float> dist_angle(0.0f, 2.0f * glm::pi<float>());
    std::uniform_real_distribution<float> dist_radius(radius, 2.0f * radius);

    // Helper lambda to lookup grid index safely
    auto get_grid_idx = [grid_w](int gx, int gy) { return gy * grid_w + gx; };

    // Helper lambda to check if a candidate point is far enough from existing points in the grid
    const float radius_sq = radius * radius;
    auto is_valid_candidate = [&](const glm::vec2& candidate) -> bool {
        int gx = static_cast<int>(candidate.x / cell_size);
        int gy = static_cast<int>(candidate.y / cell_size);

        int min_x = std::max(0, gx - 2);
        int max_x = std::min(grid_w - 1, gx + 2);
        int min_y = std::max(0, gy - 2);
        int max_y = std::min(grid_h - 1, gy + 2);

        for (int ny = min_y; ny <= max_y; ++ny)
        {
            for (int nx = min_x; nx <= max_x; ++nx)
            {
                int neighbor_pt_idx = grid[get_grid_idx(nx, ny)];
                if (neighbor_pt_idx != -1)
                {
                    glm::vec2 diff = candidate - points[neighbor_pt_idx];
                    if (glm::dot(diff, diff) < radius_sq)
                    {
                        return false;
                    }
                }
            }
        }
        return true;
    };

    // Helper lambda to register a new point
    auto add_point = [&](const glm::vec2& pt, bool is_active) -> bool {
        // Only invoke callback for newly generated points, or if you want callers to be notified
        if (is_active && !cb(pt))
        {
            return false; // Stop sampling early if callback returns false
        }

        size_t new_idx = points.size();
        points.push_back(pt);

        // Prior points act as obstacles; only newly generated points go into active_indices
        if (is_active)
        {
            active_indices.push_back(new_idx);
        }

        int gx = static_cast<int>(pt.x / cell_size);
        int gy = static_cast<int>(pt.y / cell_size);

        if (gx >= 0 && gx < grid_w && gy >= 0 && gy < grid_h)
        {
            grid[get_grid_idx(gx, gy)] = static_cast<int>(new_idx);
        }

        return true;
    };

    // --- MODIFICATION 1: Register prior phase points as passive obstacles ---
    for (const auto& prior_pt : current_points)
    {
        // Skip prior points if they fall outside the current region
        if (prior_pt.x >= 0.0f && prior_pt.x < region_size.x && prior_pt.y >= 0.0f && prior_pt.y < region_size.y)
        {
            add_point(prior_pt, /*is_active=*/false);
        }
    }

    // --- MODIFICATION 2: Find a valid initial seed point that doesn't conflict ---
    constexpr int max_initial_seed_attempts = 100;
    bool initial_seed_found = false;

    for (int attempt = 0; attempt < max_initial_seed_attempts; ++attempt)
    {
        glm::vec2 candidate_seed(dist_x(gen), dist_y(gen));
        if (is_valid_candidate(candidate_seed))
        {
            if (!add_point(candidate_seed, /*is_active=*/true))
            {
                return;
            }
            initial_seed_found = true;
            break;
        }
    }

    // If no initial seed could be placed (region is full or obstructed), exit safely
    if (!initial_seed_found && current_points.empty())
    {
        return;
    }

    // --- STEP 2: Main Sampling Loop ---
    while (!active_indices.empty())
    {
        std::uniform_int_distribution<size_t> dist_active(0, active_indices.size() - 1);
        size_t active_list_idx = dist_active(gen);
        size_t point_idx = active_indices[active_list_idx];
        glm::vec2 base_pt = points[point_idx];

        bool found_valid_candidate = false;

        for (int i = 0; i < k; ++i)
        {
            float angle = dist_angle(gen);
            float dist = dist_radius(gen);

            glm::vec2 candidate = base_pt + glm::vec2(std::cos(angle), std::sin(angle)) * dist;

            // Check bounds
            if (candidate.x < 0.0f || candidate.x >= region_size.x || candidate.y < 0.0f ||
                candidate.y >= region_size.y)
            {
                continue;
            }

            // Check distance against both existing and newly added points
            if (is_valid_candidate(candidate))
            {
                if (!add_point(candidate, /*is_active=*/true))
                {
                    return; // Stop execution if callback returns false
                }
                found_valid_candidate = true;
                break;
            }
        }

        // Remove point from active list if no valid candidates were found
        if (!found_valid_candidate)
        {
            active_indices[active_list_idx] = active_indices.back();
            active_indices.pop_back();
        }
    }

}
