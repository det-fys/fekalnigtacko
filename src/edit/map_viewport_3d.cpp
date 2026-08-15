#include "map_viewport_3d.hpp"

#include "im/utils.hpp"

edit::MapViewport3D::MapViewport3D(MapEditContext& context) : Super("3D viewport"), context_(context) {}

void edit::MapViewport3D::Draw(ImDrawList& draw_list)
{
    if (!context_.project)
    {
        return;
    }

    gfx::CameraParams cam{};

    auto& io = ImGui::GetIO();

    if (IsActive())
    {
        // update pitch and yaw from mouse movement
        auto mouse_delta = io.MouseDelta;
        context_.cam_yaw_3d += mouse_delta.x * 0.01f;
        context_.cam_pitch_3d -= mouse_delta.y * 0.01f;

        // modulo yaw to keep it in range
        context_.cam_yaw_3d = glm::mod(context_.cam_yaw_3d, glm::two_pi<float>());

        // clamp pitch to avoid gimbal lock
        context_.cam_pitch_3d =
            glm::clamp(context_.cam_pitch_3d, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);
    }

    // calculate dir from pitch and yaw
    cam.dir =
        glm::vec3(glm::cos(context_.cam_pitch_3d) * glm::sin(context_.cam_yaw_3d),
                  glm::cos(context_.cam_pitch_3d) * glm::cos(context_.cam_yaw_3d), glm::sin(context_.cam_pitch_3d));

    if (IsHovered())
    {
        float delta = io.DeltaTime * 20.0f; // movement speed

        if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
        {
            delta *= 5.0f; // faster
        }

        if (ImGui::IsKeyDown(ImGuiKey_Q))
        {
            context_.cam_pos_3d.z += delta;
        }

        if (ImGui::IsKeyDown(ImGuiKey_E))
        {
            context_.cam_pos_3d.z -= delta;
        }

        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            context_.cam_pos_3d += cam.dir * delta;
        }

        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            context_.cam_pos_3d -= cam.dir * delta;
        }

        auto right = glm::normalize(glm::cross(cam.dir, glm::vec3(0.0f, 0.0f, 1.0f)));

        if (ImGui::IsKeyDown(ImGuiKey_A))
        {
            context_.cam_pos_3d -= right * delta;
        }

        if (ImGui::IsKeyDown(ImGuiKey_D))
        {
            context_.cam_pos_3d += right * delta;
        }
    }

    cam.eye = context_.cam_pos_3d;
    cam.fov = 90.0f;

    scene_view_.Draw(draw_list, im::VecToImGui(GetCanvasP0()), im::VecToImGui(GetCanvasSize()), *context_.project, cam);
}
