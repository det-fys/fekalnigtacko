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

enum CharacterMovementType
{
    CMT_DISABLED,
    CMT_TURN,
    CMT_DIRECTIONAL,
};

struct CharacterHitBoneInstance
{
    btCollisionObject col_obj;
    TransformNode node;
};

class Character : public Entity
{
public:
    using Super = Entity;

    Character(World& world, const CharacterTuning& tuning);

    virtual void Update() override;
    virtual void SendInitData(Player& player, net::OutMessage& msg) const override;
    
    virtual void ReceiveDamage(const DamageInfo& damage) override;

    virtual void Attach(net::EntNum parentnum) override;

    const CharacterTuning& GetTuning() const { return tuning_; }

    void EnablePhysics(bool enable);
    CharacterPhysicsController* GetController() { return controller_.get(); }

    void SetInput(CharacterInputType type, bool enable);
    void SetInputs(CharacterInputFlags inputs) { in_ = inputs; }
    CharacterInputFlags GetInputs() const { return in_; }

    void SetMovementType(CharacterMovementType type);

    void SetViewAngles(float yaw, float pitch);
    float GetViewYaw() const { return view_yaw_; }
    float GetViewPitch() const { return view_pitch_; }

    const glm::vec3& GetEyePosition() const { return eye_pos_; }
    const glm::vec3& GetAimDirection() const { return aim_dir_; }

    void SetYaw(float yaw) { yaw_ = yaw; }

    void SetPosition(const glm::vec3& position);

    void SetWeightSpeedMult(float mult) { weight_speed_mult_ = mult; }

    virtual void ActivateHitBones() override;
    virtual void FinalizeFrame() override;

    float GetHealth() const { return health_; }
    bool IsAlive() const { return death_time_ < 0; }
    int64_t GetDeathTime() const;

    void SetOnDeath(std::function<void()> cb) { on_death_ = std::move(cb); }

    void ApplyImpulse(const glm::vec3& impulse);

    bool IsInAir() const;

    ~Character() override;
    
protected:
    void SetCanSprint(bool can_sprint) { can_sprint_ = can_sprint; }
    void SetIdleAnim(const std::string& anim_name);
    void SetWalkAnim(const std::string& anim_name);
    void SetRunAnim(const std::string& anim_name);
    void PlayActionAnim(assets::AnimIdx anim_idx, float speed);
    void PlayActionAnim(const std::string& anim_name, float speed = 1.0f);
    void ClearActionAnim();
    bool IsActionAnimDone() { return action_anim_done_; }
    void SetAiming(bool aiming) { aiming_ = aiming; }
    bool GetAiming() const { return aiming_; }
    void SetAimTarget(const glm::vec3& target);
    void SetViewItem(const std::string& item_name);
    void SendFire();
    void ApplyPain();
    bool CanTurnToTarget() const { return can_turn_to_target_; }

    virtual float GetDamageMultiplier(const DamageInfo& damage, std::string_view hitbone);

private:
    void SyncControllerTransform();
    void SyncTransformFromController();

    void UpdateMovement();
    void UpdateAiming();
    void UpdateAimDirection();
    void UpdatePain();
    void UpdateAnimAngles();
    void UpdateSyncState();
    void SendUpdateMsg();
    CharacterSyncFieldFlags WriteState(net::OutMessage& msg, const CharacterSyncState& base) const;

    assets::AnimIdx GetAnim(const std::string& name) const;

    void SetupHitBones();
    void EnableHitBones(bool enable);
    void UpdateHitBones();
    void UpdateHitBoneTransforms();
    void DeleteHitBones();

    void UpdateActionAnim();

    void UpdatePose();


protected:
    float turn_speed_ = 8.0f;
    float walk_speed_ = 2.0f;
    float run_speed_mult_ = 3.0f;
    float weight_speed_mult_ = 1.0f;

private:
    CharacterTuning tuning_;

    // glm::vec3 position_ = glm::vec3(0.0f);
    // glm::vec3 velocity_ = glm::vec3(0.0f);

    CharacterInputFlags in_ = 0;
    bool can_sprint_ = true;

    btCapsuleShapeZ bt_shape_;
    float z_offset_ = 0.0f; // offset of controller from root
    std::unique_ptr<CharacterPhysicsController> controller_;

    float yaw_ = 0.0f;
    float view_yaw_ = 0.0f;
    float view_pitch_ = 0.0f;

    float aim_yaw_ = 0.0f;
    float aim_pitch_ = 0.0f;

    float pain_yaw_ = 0.0f;
    float pain_pitch_ = 0.0f;

    bool can_turn_to_target_ = false;

    SkeletonInstance sk_;
    CharacterAnimState animstate_;

    CharacterSyncState sync_[2];
    size_t sync_current_ = 0;

    CharacterMovementType movement_ = CMT_DISABLED;

    float action_anim_playback_speed_ = 0.0f;
    float action_anim_end_ = 0.0f;
    bool action_anim_done_ = true;

    bool aiming_ = false;
    glm::vec3 aim_target_ = glm::vec3(0.0f);
    float aim_z_offset_ = 1.6f;
    glm::vec3 eye_pos_ = glm::vec3(0.0f);
    glm::vec3 aim_dir_ = glm::vec3(0.0f);

    std::string item_;

    bool pose_valid_ = false;

    std::vector<CharacterHitBoneInstance> hitbones_;
    btCollisionObject hitbone_proxy_;
    bool hitbones_active_ = false;
    size_t hitbones_timer_ = 0;
    bool hitbones_valid_ = false;
    std::map<const btCollisionObject*, std::string_view> hitbone_names_;

    float health_ = 100.0f;
    int64_t death_time_ = -1;

    std::function<void()> on_death_;

};

} // namespace game