#pragma once

#include "entity.hpp"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "BulletDynamics/Character/btKinematicCharacterController.h"

namespace game
{

using CharacterInputFlags = uint8_t;

enum CharacterInputType
{
    CIN_FORWARD,
    CIN_BACKWARD,
    CIN_LEFT,
    CIN_RIGHT,
    CIN_JUMP,
};

struct CapsuleShape
{
    float radius;
    float height;

    CapsuleShape(float radius, float height) : radius(radius), height(height) {}
};

struct CharacterInfo
{
    CapsuleShape shape = CapsuleShape(0.3f, 0.75f);
};

class Character : public Entity
{
public:
    using Super = Entity;

    Character(World& world, const CharacterInfo& info);

    virtual void Update() override;
    virtual void SendInitData(Player& player, net::OutMessage& msg) const override;

    void SetInput(CharacterInputType type, bool enable);
    void SetInputs(CharacterInputFlags inputs) { in_ = inputs; }

    void SetForwardYaw(float yaw) { forward_yaw_ = yaw; }

    void SetPosition(const glm::vec3& position);

    ~Character() override;

private:
    void UpdateMovement();
    void SendUpdateMsg();

    void Move(glm::vec3& velocity, float t);

private:
    CapsuleShape shape_;

    // glm::vec3 position_ = glm::vec3(0.0f);
    // glm::vec3 velocity_ = glm::vec3(0.0f);

    CharacterInputFlags in_ = 0;

    btCapsuleShapeZ bt_shape_;
    btPairCachingGhostObject bt_ghost_;

    btKinematicCharacterController bt_character_;

    float yaw_ = 0.0f;
    float forward_yaw_ = 0.0f;

    float walk_speed_ = 2.0f;

};

} // namespace game