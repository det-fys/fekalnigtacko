#include "map_viewport_3d.hpp"

#include "im/utils.hpp"
#include <glm/gtc/type_ptr.hpp>

edit::MapViewport3D::MapViewport3D(MapEditContext& context) : Super("3D viewport", context) {}

void edit::MapViewport3D::Update()
{
    auto& properties = GetProperties();

    gfx::CameraParams cam{};

    auto& io = ImGui::GetIO();

    if ((IsActive() || IsHovered()) && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        const float sensitivity = 0.003f;

        // update pitch and yaw from mouse movement
        auto mouse_delta = io.MouseDelta;
        properties.cam_yaw_3d += mouse_delta.x * sensitivity;
        properties.cam_pitch_3d -= mouse_delta.y * sensitivity;

        // modulo yaw to keep it in range
        properties.cam_yaw_3d = glm::mod(properties.cam_yaw_3d, glm::two_pi<float>());

        // clamp pitch to avoid gimbal lock
        properties.cam_pitch_3d =
            glm::clamp(properties.cam_pitch_3d, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);
    }

    // calculate dir from pitch and yaw
    cam.dir = glm::vec3(glm::cos(properties.cam_pitch_3d) * glm::sin(properties.cam_yaw_3d),
                        glm::cos(properties.cam_pitch_3d) * glm::cos(properties.cam_yaw_3d), glm::sin(properties.cam_pitch_3d));

    if (IsHovered())
    {
        float delta = io.DeltaTime * 20.0f; // movement speed

        if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
        {
            delta *= 5.0f; // faster
        }

        if (ImGui::IsKeyDown(ImGuiKey_Q))
        {
            properties.cam_pos_3d.z += delta;
        }

        if (ImGui::IsKeyDown(ImGuiKey_E))
        {
            properties.cam_pos_3d.z -= delta;
        }

        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            properties.cam_pos_3d += cam.dir * delta;
        }

        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            properties.cam_pos_3d -= cam.dir * delta;
        }

        auto right = glm::normalize(glm::cross(cam.dir, glm::vec3(0.0f, 0.0f, 1.0f)));

        if (ImGui::IsKeyDown(ImGuiKey_A))
        {
            properties.cam_pos_3d -= right * delta;
        }

        if (ImGui::IsKeyDown(ImGuiKey_D))
        {
            properties.cam_pos_3d += right * delta;
        }
    }

    cam.eye = properties.cam_pos_3d;
    cam.fov = 90.0f;

    SetCameraParams(cam);

    auto proj = glm::perspective(glm::radians(cam.fov * 0.5f), GetCanvasSize().x / GetCanvasSize().y, 0.1f, 1000.0f);
    auto view = glm::lookAt(cam.eye, cam.eye + cam.dir, cam.up);
    SetOverlayMatrices(view, proj);

    Super::Update();
}

void edit::MapViewport3D::Draw(ImDrawList& draw_list)
{
    Super::Draw(draw_list);
}
