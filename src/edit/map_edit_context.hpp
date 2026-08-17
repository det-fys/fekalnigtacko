#pragma once

#include <optional>

#include <imgui.h>
#include <ImGuizmo.h>

#include "map_project.hpp"

namespace edit
{

struct MapEditContext
{
    // 2D
    glm::vec2 cam_pos_2d{0.0f, 0.0f};
    float zoom_2d = 1.0f;
    float cam_fov_2d = 50.0f;

    // 3D
    glm::vec3 cam_pos_3d{0.0f, 0.0f, 0.0f};
    float cam_pitch_3d = 0.0f;
    float cam_yaw_3d = 0.0f;

    // world
    float day_time = 12.0f; // 0-24

    std::optional<Project> project;

    ImGuizmo::OPERATION gizmo_operation = ImGuizmo::TRANSLATE;
};

} // namespace edit
