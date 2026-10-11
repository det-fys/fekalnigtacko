#include "camera_controller.hpp"

#include "utils/math.hpp"

void game::CameraController::SetViewAngles(float yaw, float pitch)
{
    yaw_ = yaw;
    pitch_ = pitch;
    // TODO: validate
}

static glm::vec3 UpFromMatrix(const FarMatrix& matrix)
{
    return matrix.basis[2];
}

void game::CameraController::Update(float time)
{
    // update aim factor
    MoveToward(aim_factor_, aiming_ ? 1.0f : 0.0f, time * 3.0f);
}

void game::CameraController::Recalculate(collision::DynamicsWorld* world)
{
    float yaw_cos = glm::cos(yaw_);
    float yaw_sin = glm::sin(yaw_);
    float pitch_cos = glm::cos(pitch_);
    float pitch_sin = glm::sin(pitch_);
    forward_ = glm::vec3(-yaw_sin * pitch_cos, yaw_cos * pitch_cos, pitch_sin);
    glm::vec3 right = glm::cross(forward_, glm::vec3(0.0f, 0.0f, 1.0f));

    glm::dvec3 start_noaim(0.0);
    glm::dvec3 start_aim(0.0);
    bool scope = ShouldDrawScope();

    float distance_noaim = 5.0f;
    glm::vec3 aim_end_offset = right * 0.4f - forward_ * 1.8f;

    if (character_transform_)
    {
        auto up = UpFromMatrix(*character_transform_);
        start_noaim = character_transform_->position + glm::dvec3(up * (scope ? 1.8f : 2.0f));

        if (!scope)
        {
            start_aim = start_noaim - glm::dvec3(up * 0.3f);
        }
        else
        {
            start_aim = start_noaim;
            aim_end_offset = glm::vec3(0.0f);
        }
    }

    if (rideable_transform_)
    {
        start_noaim = rideable_transform_->position + glm::dvec3(0.0, 0.0, 2.0);
        distance_noaim = 8.0f;

        if (!scope)
        {
            aim_end_offset = right * 0.3f - forward_ * 4.5f + glm::vec3(0.0f, 0.0f, 0.8f);
        }
    }

    auto end_noaim = start_noaim - glm::dvec3(forward_ * distance_noaim); 
    auto end_aim = start_aim + glm::dvec3(aim_end_offset);

    auto aim_factor_smooth = scope ? 1.0f : glm::smoothstep(0.0f, 1.0f, aim_factor_);
    auto start = glm::mix(start_noaim, start_aim, aim_factor_smooth);
    auto end = glm::mix(end_noaim, end_aim, aim_factor_smooth);

    eye_ = end;

    if (world)
    {
        // prevent penetration through static objects
        eye_ = world->CameraSweep(start, end);
    }
}

// glm::mat4 game::CameraController::GetViewMatrix() const
// {
//     return glm::lookAt(eye_, eye_ + forward_, glm::vec3(0.0f, 0.0f, 1.0f));
// }

bool game::CameraController::ShouldDrawCrosshair() const
{
    return aim_crosshair_ && !aim_scope_ && aim_factor_ > 0.5f;
}

bool game::CameraController::ShouldDrawScope() const
{
    return aim_crosshair_ && aim_scope_ && aim_factor_ > 0.7f;
}

float game::CameraController::GetFov() const
{
    if (ShouldDrawScope())
    {
        return glm::mix(25.0f, 20.0f, glm::smoothstep(0.0f, 1.0f, (aim_factor_ - 0.7f) / 0.3f));
    }

    // return glm::mix(90.0f, 80.0f, aim_factor_);
    return 90.0f;
}
