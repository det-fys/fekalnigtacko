#include "character.hpp"

#include "assets/cache.hpp"
#include "net/utils.hpp"
#include "utils/math.hpp"
#include "world.hpp"
#include "utils/random.hpp"


game::Character::Character(World& world, const CharacterTuning& tuning)
    : Super(world, net::ET_CHARACTER), tuning_(tuning), bt_shape_(tuning_.shape.radius, tuning_.shape.height)
{
    z_offset_ = tuning_.shape.height * 0.5f + tuning_.shape.radius - 0.05f;

    sk_ = SkeletonInstance(assets::CacheManager::GetSkeleton("data/" + tuning.model_name + ".sk"), &root_);
    SetupHitBones();
}

static bool Turn(float& angle, float target, float step)
{
    constexpr float PI = glm::pi<float>();
    constexpr float TWO_PI = glm::two_pi<float>();

    angle = glm::mod(angle, TWO_PI);
    target = glm::mod(target, TWO_PI);

    float diff = glm::mod(target - angle + PI, TWO_PI) - PI;

    if (glm::abs(diff) <= step)
    {
        angle = target;
        return true;
    }

    angle = glm::mod(angle + glm::sign(diff) * step, TWO_PI);
    return false;
}

void game::Character::Update()
{
    Super::Update();

    pose_valid_ = false;
    hitbones_valid_ = false;

    SyncTransformFromController();
    UpdateMovement();
    UpdateAiming();
    UpdatePain();
    UpdateAnimAngles();
    UpdateActionAnim();
    root_.UpdateMatrix();
    UpdateHitBones();

    sync_current_ = 1 - sync_current_;
    UpdateSyncState();
    SendUpdateMsg();
}

void game::Character::SendInitData(Player& player, net::OutMessage& msg) const
{
    Super::SendInitData(player, msg);

    // write model name
    msg.Write(net::ModelName(tuning_.model_name));

    // write clothes
    msg.Write<net::NumClothes>(tuning_.clothes.size());
    for (const auto& clothes : tuning_.clothes)
    {
        msg.Write(net::ClothesName(clothes.name));
        net::WriteRGB(msg, clothes.color);
    }

    // write item
    msg.Write(net::ModelName(item_));

    // write state against default
    static const CharacterSyncState default_state;
    size_t fields_pos = msg.Reserve<CharacterSyncFieldFlags>();
    auto fields = WriteState(msg, default_state);
    msg.WriteAt(fields_pos, fields);
}

void game::Character::ReceiveDamage(const DamageInfo& damage)
{
    Super::ReceiveDamage(damage);

    if (!IsAlive())
    {
        return; // already ded
    }

    float actual_damage = damage.damage;

    if (damage.type == DAMAGE_BULLET)
    {
        std::string_view hit_name;
    
        auto it = hitbone_names_.find(damage.hit_object);
        if (it != hitbone_names_.end())
        {
            hit_name = it->second;
        }

        actual_damage *= GetHitBoneDamageMultiplier(hit_name);

    }

    health_ -= actual_damage;

    // just died
    if (health_ <= 0.0f)
    {
        health_ = 0.0f;
        death_time_ = GetWorld().GetTime();

        if (on_death_)
            on_death_();
    }

    ApplyPain();

    // std::string text = "au! " + std::string(hit_name);
    // GetWorld().SendChat(text);
}

void game::Character::Attach(net::EntNum parentnum)
{
    Super::Attach(parentnum);

    // remake these if already updated
    if (IsUpToDate())
    {
        UpdateSyncState();
        SendUpdateMsg();
    }
}

void game::Character::EnablePhysics(bool enable)
{
    if (enable && !controller_)
    {
        controller_ = std::make_unique<CharacterPhysicsController>(*this, world_.GetBtWorld(), bt_shape_);
        SyncControllerTransform();
    }
    else if (!enable && controller_)
    {
        controller_.reset();
        root_.local.rotation = glm::quat(); // reset rotation
    }
}

void game::Character::SetInput(CharacterInputType type, bool enable)
{
    if (enable)
        in_ |= (1 << type);
    else
        in_ &= ~(1 << type);
}

void game::Character::SetMovementType(CharacterMovementType type)
{
    movement_ = type;
}

void game::Character::SetViewAngles(float yaw, float pitch)
{
    view_yaw_ = yaw;
    view_pitch_ = pitch;
}

void game::Character::SetPosition(const glm::vec3& position)
{
    root_.local.position = position;
    SyncControllerTransform();
}

void game::Character::ActivateHitBones()
{
    Super::ActivateHitBones();
    EnableHitBones(true);
}

void game::Character::FinalizeFrame()
{
    Super::FinalizeFrame();
    pose_valid_ = false;
    hitbones_valid_ = false;
}

int64_t game::Character::GetDeathTime() const
{
     return GetWorld().GetTime() - death_time_;
}

game::Character::~Character()
{
    DeleteHitBones();
}

void game::Character::SetIdleAnim(const std::string& anim_name)
{
    animstate_.idle_anim_idx = GetAnim(anim_name);
}

void game::Character::SetWalkAnim(const std::string& anim_name)
{
    animstate_.walk_anim_idx = GetAnim(anim_name);
}

void game::Character::SetRunAnim(const std::string& anim_name)
{
    animstate_.run_anim_idx = GetAnim(anim_name);
}

void game::Character::PlayActionAnim(assets::AnimIdx anim_idx, float speed)
{
    action_anim_end_ = (anim_idx != assets::NO_ANIM) ? sk_.GetSkeleton()->GetAnimation(anim_idx)->GetDuration() : 0.0f;

    if (animstate_.action_anim_idx != anim_idx)
    {
        // continue from current time if same anim
        animstate_.action_time = (speed > 0.0f) ? 0.0f : action_anim_end_;
    }
    animstate_.action_anim_idx = anim_idx;
    action_anim_playback_speed_ = speed;
    action_anim_done_ = anim_idx == assets::NO_ANIM;
}

void game::Character::PlayActionAnim(const std::string& anim_name, float speed)
{
    if (anim_name.empty())
    {
        ClearActionAnim();
        return;
    }

    PlayActionAnim(GetAnim(anim_name), speed);
}

void game::Character::ClearActionAnim()
{
    PlayActionAnim(assets::NO_ANIM, 0.0f);
}

void game::Character::SetAimTarget(const glm::vec3& target)
{
    aim_target_ = target;
}

void game::Character::SetViewItem(const std::string& item_name)
{
    if (item_ == item_name)
        return;

    item_ = item_name;

    auto msg = BeginEntMsg(net::EMSG_EQUIP);
    msg.Write(net::ModelName(item_name));
}

void game::Character::SendFire()
{
    auto msg = BeginEntMsg(net::EMSG_FIRE);
}

void game::Character::ApplyPain()
{
    pain_pitch_ = glm::clamp(pain_pitch_ + RandomFloat(glm::radians(-5.0f), glm::radians(20.0f)), glm::radians(-30.0f), glm::radians(30.0f));
    pain_yaw_ = glm::clamp(pain_yaw_ + RandomFloat(glm::radians(-20.0f), glm::radians(20.0f)), glm::radians(-30.0f), glm::radians(30.0f));
}

float game::Character::GetHitBoneDamageMultiplier(const std::string_view hitbone)
{
    if (hitbone == "head" || hitbone == "neck")
        return 3.0f;

    if (hitbone == "torso1" || hitbone == "torso2")
        return 1.0f;

    return 0.2f;
}

void game::Character::SyncControllerTransform()
{
    if (!controller_)
        return;

    auto& position = root_.local.position;
    auto& bt_ghost = controller_->GetBtGhost();
    auto trans = bt_ghost.getWorldTransform();
    trans.setOrigin(btVector3(position.x, position.y, position.z + z_offset_));
    bt_ghost.setWorldTransform(trans);
}

void game::Character::SyncTransformFromController()
{
    if (!controller_)
        return;

    auto bt_trans = controller_->GetBtGhost().getWorldTransform();
    root_.local.SetBtTransform(bt_trans);
    root_.local.position.z -= z_offset_; // foot pos
}

static glm::vec2 GetInputDir(game::CharacterInputFlags in)
{
    glm::vec2 dir(0.0f);

    if (in & (1 << game::CIN_FORWARD))
        dir.y += 1.0f;

    if (in & (1 << game::CIN_BACKWARD))
        dir.y -= 1.0f;

    if (in & (1 << game::CIN_RIGHT))
        dir.x -= 1.0f;

    if (in & (1 << game::CIN_LEFT))
        dir.x += 1.0f;

    return dir;
}

void game::Character::UpdateMovement()
{
    if (movement_ == CMT_DISABLED)
    {
        animstate_.loco_blend = 0.0f;
        return;
    }

    constexpr float dt = 1.0f / 25.0f;
    bool walking = false;
    bool running = false;

    glm::vec3 move_dir(0.0f);

    auto input_dir = GetInputDir(in_);
    if (input_dir.x != 0.0f || input_dir.y != 0.0f)
    {
        walking = true;
        
        if ((in_ & (1 << CIN_SPRINT)) && can_sprint_)
            running = true;

        const bool directional = (movement_ == CMT_DIRECTIONAL);

        float relative_yaw = std::atan2(input_dir.x, input_dir.y);
        float turn_yaw = directional ? view_yaw_ : view_yaw_ + relative_yaw;
        Turn(yaw_, turn_yaw, turn_speed_ * dt);
        float move_yaw = directional ? yaw_ + relative_yaw : yaw_;
    
        move_dir = glm::vec3(-glm::sin(move_yaw), glm::cos(move_yaw), 0.0f) * walk_speed_ * dt * weight_speed_mult_;
       
        if (running)
            move_dir *= run_speed_mult_;

    }

    root_.local.rotation = glm::angleAxis(yaw_, glm::vec3(0.0f, 0.0f, 1.0f));

    if (controller_)
    {
        auto& bt_character = controller_->GetBtController();
        bt_character.setWalkDirection(btVector3(move_dir.x, move_dir.y, move_dir.z));

        if (in_ & (1 << CIN_JUMP) && bt_character.canJump())
        {
            bt_character.jump(btVector3(0.0f, 0.0f, 10.0f));
        }
    }

    // update anim
    float run_blend_target = walking ? 0.5f : 0.0f;
    MoveToward(animstate_.loco_blend, run_blend_target, dt * 2.0f);
    float anim_speed = glm::mix(0.3f, 1.5f, UnMix(0.0f, 0.5f, animstate_.loco_blend)) * weight_speed_mult_;
    if (running)
        anim_speed *= run_speed_mult_;
    animstate_.loco_phase = glm::mod(animstate_.loco_phase + anim_speed * dt, 1.0f);
}

void game::Character::UpdateAiming()
{
    float delta = 10.0f;

    if (!aiming_)
    {
        delta = 6.0f / 25.0f;
        MoveToward(aim_yaw_, 0.0f, delta);
        MoveToward(aim_pitch_, 0.0f, delta);
        UpdateAimDirection();
        return;
    }

    // get yaw and pitch relative to transform
    glm::vec3 dir = aim_target_ - GetRoot().local.position;

    if (parent_)
    {
        auto inv_parent =  glm::inverse(parent_->GetRoot().matrix);

        // glm::vec3 character_pos_in_parent = inv_parent * glm::vec4(GetRoot().local.position, 1.0f);
        glm::vec3 aim_target_in_parent = inv_parent * glm::vec4(aim_target_, 1.0f);
        dir = aim_target_in_parent - GetRoot().local.position;
    }

    dir.z -= aim_z_offset_; // from eye
    dir = glm::normalize(dir);

    float pitch = glm::asin(dir.z);
    float yaw = glm::atan(-dir.x, dir.y);

    auto target_pitch = glm::clamp(pitch, glm::radians(-60.0f), glm::radians(55.0f)); // clamp to make it less weird
    MoveToward(aim_pitch_, target_pitch, delta);

    if (movement_ == CMT_DISABLED)
    {
        auto target_yaw = glm::mod(yaw + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>();
        const float yaw_limit = glm::radians(120.0f);
        target_yaw = glm::clamp(target_yaw, -yaw_limit, yaw_limit);
        MoveToward(aim_yaw_, target_yaw, delta);
    }
    else
    {
        Turn(yaw_, yaw, delta);
        MoveToward(aim_yaw_, 0.0f, delta);
    }

    UpdateAimDirection();
}

void game::Character::UpdateAimDirection()
{
    eye_pos_ = GetRoot().matrix * glm::vec4(0.0f, 0.0f, aim_z_offset_, 1.0f);

    auto pitch = aim_pitch_;
    auto yaw = yaw_ + aim_yaw_;
    aim_dir_ = glm::vec3(-glm::sin(yaw) * glm::cos(pitch), glm::cos(yaw) * glm::cos(pitch), glm::sin(pitch));

    if (parent_)
    {
        aim_dir_ = glm::normalize(parent_->GetRoot().matrix * glm::vec4(aim_dir_, 0.0f));
    }

    // GetWorld().Beam(eye_pos_, eye_pos_ + aim_dir_ * 100.0f, 0x0000FF, 1.0f / 25.0f);
}

void game::Character::UpdatePain()
{
    float delta = 1.5f / 25.0f;
    MoveToward(pain_yaw_, 0.0f, delta);
    MoveToward(pain_pitch_, 0.0f, delta);
}

void game::Character::UpdateAnimAngles()
{
    animstate_.yaw = aim_yaw_ + pain_yaw_;
    animstate_.pitch = aim_pitch_ + pain_pitch_;
}

void game::Character::UpdateSyncState()
{
    auto& state = sync_[sync_current_];

    // transform
    net::EncodePosition(root_.local.position, state.pos);
    state.yaw.Encode(yaw_);

    // idle
    state.idle_anim = animstate_.idle_anim_idx;

    // loco
    state.walk_anim = animstate_.walk_anim_idx;
    state.run_anim = animstate_.run_anim_idx;
    state.loco_phase.Encode(animstate_.loco_phase);
    state.loco_blend.Encode(animstate_.loco_blend);

    // action
    state.action_anim = animstate_.action_anim_idx;
    state.action_time.Encode(animstate_.action_time);

    // aim
    state.aim_yaw.Encode(animstate_.yaw);
    state.aim_pitch.Encode(animstate_.pitch);
}

void game::Character::SendUpdateMsg()
{
    auto msg = BeginUpdateMsg();
    auto fields_pos = msg.Reserve<CharacterSyncFieldFlags>();
    auto fields = WriteState(msg, sync_[1 - sync_current_]);

    if (fields == 0)
    {
        DiscardUpdateMsg();
        return;
    }

    msg.WriteAt(fields_pos, fields);
}

game::CharacterSyncFieldFlags game::Character::WriteState(net::OutMessage& msg, const CharacterSyncState& base) const
{
    const auto& curr = sync_[sync_current_];

    game::CharacterSyncFieldFlags fields = 0;

    // transform
    if (curr.pos.x.value != base.pos.x.value || curr.pos.y.value != base.pos.y.value ||
        curr.pos.z.value != base.pos.z.value || curr.yaw.value != base.yaw.value)
    {
        fields |= CSF_TRANSFORM;

        net::WriteDelta(msg, curr.pos.x, base.pos.x);
        net::WriteDelta(msg, curr.pos.y, base.pos.y);
        net::WriteDelta(msg, curr.pos.z, base.pos.z);

        net::WriteDelta(msg, curr.yaw, base.yaw);
    }

    // idle
    if (curr.idle_anim != base.idle_anim)
    {
        fields |= CSF_IDLE_ANIM;

        msg.Write(curr.idle_anim);
    }

    // loco anims
    if (curr.walk_anim != base.walk_anim || curr.run_anim != base.run_anim)
    {
        fields |= CSF_LOCO_ANIMS;

        msg.Write(curr.walk_anim);
        msg.Write(curr.run_anim);
    }

    // loco vals
    if (curr.loco_blend.value != base.loco_blend.value || curr.loco_phase.value != base.loco_phase.value)
    {
        fields |= CSF_LOCO_VALS;

        net::WriteDelta(msg, curr.loco_blend, base.loco_blend);
        net::WriteDelta(msg, curr.loco_phase, base.loco_phase);
    }

    // action anim
    if (curr.action_anim != base.action_anim)
    {
        fields |= CSF_ACTION_ANIM;

        msg.Write(curr.action_anim);
    }

    // action phase
    if (curr.action_time.value != base.action_time.value)
    {
        fields |= CSF_ACTION_TIME;

        net::WriteDelta(msg, curr.action_time, base.action_time);
    }

    // aim
    if (curr.aim_yaw.value != base.aim_yaw.value || curr.aim_pitch.value != base.aim_pitch.value)
    {
        fields |= CSF_AIM;

        net::WriteDelta(msg, curr.aim_yaw.value, base.aim_yaw.value);
        net::WriteDelta(msg, curr.aim_pitch.value, base.aim_pitch.value);
    }

    return fields;
}

assets::AnimIdx game::Character::GetAnim(const std::string& name) const
{
    return sk_.GetSkeleton()->GetAnimationIdx(name);
}

void game::Character::SetupHitBones()
{
    const auto& sk_hitbones = sk_.GetSkeleton()->GetHitBones();
    hitbones_.resize(sk_hitbones.size());

    for (size_t i = 0; i < hitbones_.size(); ++i)
    {
        auto& hitbone = hitbones_[i];
        auto& sk_hitbone = sk_hitbones[i];

        // setup node
        hitbone.node.parent = &sk_.GetBoneNode(sk_hitbone.bone_idx);
        hitbone.node.local = sk_hitbone.offset;

        // setup object
        auto& col_obj = hitbone.col_obj;
        col_obj.setCollisionShape(sk_hitbone.col_shape.get());
        collision::SetObjectInfo(&col_obj, collision::OT_ENTITY, 0, this);

        hitbone_names_[&col_obj] = sk_hitbone.name;
    }
    
    // setup proxy
    static btSphereShape proxy_shape(1.5f);
    hitbone_proxy_.setCollisionShape(&proxy_shape);
    collision::SetObjectInfo(&hitbone_proxy_, collision::OT_ENTITY, collision::OF_EXPLOSION_DAMAGE, this);
    GetWorld().GetBtWorld().addCollisionObject(&hitbone_proxy_, collision::OG_HITBONES_PROXY, collision::OG_PROJECTILE);
}

void game::Character::EnableHitBones(bool enable)
{
    if (enable)
    {
        hitbones_timer_ = 2; // reset timer
    }

    if (enable == hitbones_active_)
        return;

    hitbones_active_ = enable;
    hitbones_valid_ = false;

    auto& bt_world = GetWorld().GetBtWorld();

    if (enable)
    {
        UpdateHitBoneTransforms(); // update transforms first

        for (auto& hitbone : hitbones_)
        {
            bt_world.addCollisionObject(&hitbone.col_obj, collision::OG_DEFAULT, collision::OG_PROJECTILE);
        }
    }
    else
    {
        for (auto& hitbone : hitbones_)
        {
            bt_world.removeCollisionObject(&hitbone.col_obj);
        }
    }

}

void game::Character::UpdateHitBones()
{
    // update proxy transform
    glm::vec3 center = GetRoot().matrix * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    btTransform trans;
    trans.setIdentity();
    trans.setOrigin(btVector3(center.x, center.y, center.z));
    hitbone_proxy_.setWorldTransform(trans);

    if (hitbones_active_)
    {
        if (hitbones_timer_ > 0)
            --hitbones_timer_;
        else
            EnableHitBones(false);
    }

    UpdateHitBoneTransforms();
}

static btTransform BtTransformFromMat4(const glm::mat4& m)
{
    btMatrix3x3 basis(
        m[0][0], m[1][0], m[2][0],
        m[0][1], m[1][1], m[2][1],
        m[0][2], m[1][2], m[2][2]
    );

    btVector3 origin(
        m[3][0],
        m[3][1],
        m[3][2]
    );

    btTransform trans;
    trans.setBasis(basis);
    trans.setOrigin(origin);
    return trans;
}

void game::Character::UpdateHitBoneTransforms()
{
    if (hitbones_valid_ || !hitbones_active_)
        return;

    UpdatePose();

    for (auto& hitbone : hitbones_)
    {
        hitbone.node.UpdateMatrix();
        hitbone.col_obj.setWorldTransform(BtTransformFromMat4(hitbone.node.matrix));

        // debug boxes
        // GetWorld().BeamBox(hitbone.node.GetGlobalPosition() - 0.05f, hitbone.node.GetGlobalPosition() + 0.05f, 0xFFFF00,
        //                    1.5f / 25.0f);
    }
}

void game::Character::DeleteHitBones()
{
    EnableHitBones(false);
    GetWorld().GetBtWorld().removeCollisionObject(&hitbone_proxy_);
}

void game::Character::UpdateActionAnim()
{
    if (action_anim_done_)
        return;

    animstate_.action_time += action_anim_playback_speed_ * (1.0f / 25.0f);
    
    if (action_anim_playback_speed_ > 0.0f)
    {
        if (animstate_.action_time >= action_anim_end_)
        {
            animstate_.action_time = action_anim_end_;
            action_anim_done_ = true;
        }
    }
    else
    {
        if (animstate_.action_time <= 0.0f)
        {
            animstate_.action_time = 0.0f;
            action_anim_done_ = true;
        }
    }
}

void game::Character::UpdatePose()
{
    if (pose_valid_)
        return;

    animstate_.ApplyToSkeleton(sk_);
    sk_.UpdateBoneMatrices();

    pose_valid_ = true;
}

game::CharacterPhysicsController::CharacterPhysicsController(Character& character, btDynamicsWorld& bt_world, btCapsuleShapeZ& bt_shape)
    : character_(character), bt_world_(bt_world), bt_character_(&bt_ghost_, &bt_shape, 0.3f, btVector3(0, 0, 1))
{
    btTransform start_transform;
    start_transform.setIdentity();
    bt_ghost_.setWorldTransform(start_transform);
    bt_ghost_.setCollisionShape(&bt_shape);
    bt_ghost_.setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

    collision::SetObjectInfo(&bt_ghost_, collision::OT_ENTITY, collision::OF_CRASH_DAMAGE, &character);

    bt_world_.addCollisionObject(&bt_ghost_, btBroadphaseProxy::CharacterFilter,
                                 btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter);
    // bt_world.addCollisionObject(&bt_ghost_);
    bt_world_.addAction(&bt_character_);
}

game::CharacterPhysicsController::~CharacterPhysicsController()
{
    bt_world_.removeAction(&bt_character_);
    bt_world_.removeCollisionObject(&bt_ghost_);
}
