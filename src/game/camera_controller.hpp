#pragma once

#include "collision/dynamicsworld.hpp"

namespace game
{

class CameraController
{
public:
    CameraController() = default;

    void SetCharacterTransform(const glm::mat4* transform) { character_transform_ = transform; }
    void SetRideableTransform(const glm::mat4* transform) { rideable_transform_ = transform; }

    void SetViewAngles(float yaw, float pitch);
    float GetYaw() const { return yaw_; }
    float GetPitch() const { return pitch_; }

    void SetAiming(bool aiming) { aiming_ = aiming; }

    void Update(float time);
    void Recalculate(collision::DynamicsWorld* world);

    const glm::vec3& GetEye() const { return eye_; }
    const glm::vec3& GetForward() const { return forward_; }
    glm::mat4 GetViewMatrix() const;
    float GetAimFactor() const { return aim_factor_; }

private:
    const glm::mat4* character_transform_ = nullptr;
    const glm::mat4* rideable_transform_ = nullptr;

    float yaw_ = 0.0f;
    float pitch_ = 0.0f;

    bool aiming_ = false;
    float aim_factor_ = 0.0f;

    glm::vec3 eye_;
    glm::vec3 forward_;

};


}