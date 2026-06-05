#pragma once

#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>
#include "character_anim_state.hpp"
#include "character_sync.hpp"
#include "entity.hpp"
#include "character_tuning.hpp"

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
    CIN_SPRINT,
};

class Character;

class CharacterPhysicsController
{
public:
    CharacterPhysicsController(Character& character, btDynamicsWorld& bt_world, btCapsuleShapeZ& bt_shape);
    DELETE_COPY_MOVE(CharacterPhysicsController)

    btKinematicCharacterController& GetBtController() { return bt_character_; }
    const btKinematicCharacterController& GetBtController() const { return bt_character_; }
    btGhostObject& GetBtGhost() { return bt_ghost_; }
    const btGhostObject& GetBtGhost() const { return bt_ghost_; }

    ~CharacterPhysicsController();

private:
    Character& character_;
    btDynamicsWorld& bt_world_;
    btPairCachingGhostObject bt_ghost_;
    btKinematicCharacterController bt_character_;
};

class Character : public Entity
{
public:
    using Super = Entity;

    Character(World& world, const CharacterTuning& tuning);

    virtual void Update() override;
    virtual void SendInitData(Player& player, net::OutMessage& msg) const override;

    virtual void Attach(net::EntNum parentnum) override;

    const CharacterTuning& GetTuning() const { return tuning_; }

    void EnablePhysics(bool enable);
    CharacterPhysicsController* GetController() { return controller_.get(); }

    void SetInput(CharacterInputType type, bool enable);
    void SetInputs(CharacterInputFlags inputs) { in_ = inputs; }

    void SetForwardYaw(float yaw) { forward_yaw_ = yaw; }
    float GetForwardYaw() const { return forward_yaw_; }
    void SetYaw(float yaw) { yaw_ = yaw; }

    void SetPosition(const glm::vec3& position);

    void SetIdleAnim(const std::string& anim_name);
    void SetWalkAnim(const std::string& anim_name);
    void SetRunAnim(const std::string& anim_name);

    ~Character() override = default;

private:
    void SyncControllerTransform();
    void SyncTransformFromController();

    void UpdateMovement();
    void UpdateSyncState();
    void SendUpdateMsg();
    CharacterSyncFieldFlags WriteState(net::OutMessage& msg, const CharacterSyncState& base) const;

    assets::AnimIdx GetAnim(const std::string& name) const;

private:
    CharacterTuning tuning_;

    // glm::vec3 position_ = glm::vec3(0.0f);
    // glm::vec3 velocity_ = glm::vec3(0.0f);

    CharacterInputFlags in_ = 0;

    btCapsuleShapeZ bt_shape_;
    float z_offset_ = 0.0f; // offset of controller from root
    std::unique_ptr<CharacterPhysicsController> controller_;

    float yaw_ = 0.0f;
    float forward_yaw_ = 0.0f;

    float walk_speed_ = 2.0f;

    SkeletonInstance sk_;
    CharacterAnimState animstate_;

    CharacterSyncState sync_[2];
    size_t sync_current_ = 0;
};

} // namespace game