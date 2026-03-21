#include "character.hpp"
#include "assets/cache.hpp"
#include "net/utils.hpp"
#include "utils/math.hpp"
#include "world.hpp"

game::Character::Character(World& world, const CharacterTuning& tuning)
    : Super(world, net::ET_CHARACTER), tuning_(tuning), bt_shape_(tuning_.shape.radius, tuning_.shape.height)
{
    z_offset_ = tuning_.shape.height * 0.5f + tuning_.shape.radius - 0.05f;

    sk_ = SkeletonInstance(assets::CacheManager::GetSkeleton("data/human.sk"), &root_);
    animstate_.idle_anim_idx = GetAnim("idle");
    animstate_.walk_anim_idx = GetAnim("walk");
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

    SyncTransformFromController();
    root_.UpdateMatrix();

    UpdateMovement();

    sync_current_ = 1 - sync_current_;
    UpdateSyncState();
    SendUpdateMsg();
}

void game::Character::SendInitData(Player& player, net::OutMessage& msg) const
{
    Super::SendInitData(player, msg);

    // write clothes
    msg.Write<net::NumClothes>(tuning_.clothes.size());
    for (const auto& clothes : tuning_.clothes)
    {
        msg.Write(net::ClothesName(clothes.name));
        net::WriteRGB(msg, clothes.color);
    }

    // write state against default
    static const CharacterSyncState default_state;
    size_t fields_pos = msg.Reserve<CharacterSyncFieldFlags>();
    auto fields = WriteState(msg, default_state);
    msg.WriteAt(fields_pos, fields);
}

void game::Character::EnablePhysics(bool enable)
{
    if (enable && !controller_)
    {
        controller_ = std::make_unique<CharacterPhysicsController>(world_.GetBtWorld(), bt_shape_);
        SyncControllerTransform();
    }
    else if (!enable && controller_)
    {
        controller_.reset();
    }
}

void game::Character::SetInput(CharacterInputType type, bool enable)
{
    if (enable)
        in_ |= (1 << type);
    else
        in_ &= ~(1 << type);
}

// static bool SweepCapsule(btCollisionWorld& world, const btCapsuleShapeZ& shape, const glm::vec3& start,
//                          const glm::vec3& end, float& hit_fraction, glm::vec3& hit_normal)
// {
//     btVector3 bt_start(start.x, start.y, start.z);
//     btVector3 bt_end(end.x, end.y, end.z);

//     static const btMatrix3x3 bt_basis(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

//     btTransform start_transform(bt_basis, bt_start);
//     btTransform end_transform(bt_basis, bt_end);

//     btCollisionWorld::ClosestConvexResultCallback result_callback(bt_start, bt_end);
//     world.convexSweepTest(&shape, start_transform, end_transform, result_callback);

//     if (result_callback.hasHit())
//     {
//         hit_fraction = result_callback.m_closestHitFraction;
//         hit_normal = glm::vec3(result_callback.m_hitNormalWorld.x(), result_callback.m_hitNormalWorld.y(),
//                                result_callback.m_hitNormalWorld.z());

//         return true;
//     }

//     return false;
// }

void game::Character::SetPosition(const glm::vec3& position)
{
    root_.local.position = position;
    SyncControllerTransform();
}

void game::Character::SetMainAnim(const std::string& anim_name)
{
    animstate_.idle_anim_idx = GetAnim(anim_name);
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

void game::Character::UpdateMovement()
{
    constexpr float dt = 1.0f / 25.0f;
    constexpr float running_mult = 3.0f;
    bool walking = false;
    bool running = false;
    glm::vec2 movedir(0.0f);

    if (in_ & (1 << CIN_FORWARD))
        movedir.x += 1.0f;

    if (in_ & (1 << CIN_BACKWARD))
        movedir.x -= 1.0f;

    if (in_ & (1 << CIN_RIGHT))
        movedir.y -= 1.0f;

    if (in_ & (1 << CIN_LEFT))
        movedir.y += 1.0f;

    glm::vec3 walkdir(0.0f);

    if (movedir.x != 0.0f || movedir.y != 0.0f)
    {
        walking = true;
        
        if (in_ & (1 << CIN_SPRINT))
            running = true;

        float target_yaw = forward_yaw_ + std::atan2(movedir.y, movedir.x);
        Turn(yaw_, target_yaw, 8.0f * dt);

        glm::vec3 forward_dir(glm::cos(yaw_), glm::sin(yaw_), 0.0f);
        walkdir = forward_dir * walk_speed_ * dt;

        if (running)
            walkdir *= running_mult;

    }

    if (controller_)
    {
        auto& bt_character = controller_->GetBtController();
        bt_character.setWalkDirection(btVector3(walkdir.x, walkdir.y, walkdir.z));

        if (in_ & (1 << CIN_JUMP) && bt_character.canJump())
        {
            bt_character.jump(btVector3(0.0f, 0.0f, 10.0f));
        }
    }

    // update anim
    float run_blend_target = walking ? 0.5f : 0.0f;
    MoveToward(animstate_.loco_blend, run_blend_target, dt * 2.0f);
    float anim_speed = glm::mix(0.5f, 1.5f, UnMix(0.0f, 0.5f, animstate_.loco_blend));
    if (running)
        anim_speed *= running_mult;
    animstate_.loco_phase = glm::mod(animstate_.loco_phase + anim_speed * dt, 1.0f);
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

    return fields;
}

void game::Character::Move(glm::vec3& velocity, float t)
{
    // glm::vec3 u = velocity * t; // Calculate the movement vector

    // btCollisionWorld& bt_world = world_.GetBtWorld();
    // btCapsuleShapeZ bt_shape(shape_.radius, shape_.height);

    // const int MAX_ITERS = 16;
    // for (size_t i = 0; i < MAX_ITERS && glm::dot(u, u) > 0.0f; ++i)
    // {
    //     // printf("Entity::Move: Iteration %zu, u = (%f, %f, %f)\n", i, u.x, u.y, u.z);

    //     glm::vec3 to = position_ + u;

    //     float hit_fraction = 1.0f;
    //     glm::vec3 hit_normal;

    //     bool hit = SweepCapsule(bt_world, bt_shape, position_, to, hit_fraction, hit_normal);

    //     // Update the position based on the hit fraction
    //     position_ += hit_fraction * u;

    //     if (!hit)
    //         break;

    //     // hit_normal *= -1.0f; // Invert the normal to point outwards
    //     // printf("Entity::Move: Hit detected, hit_fraction = %f, hit_normal = (%f, %f, %f)\n", hit_fraction,
    //     // hit_normal.x, hit_normal.y, hit_normal.z);

    //     u -= hit_fraction * u;                                   // Reduce the movement vector by the hit fraction
    //     u -= glm::dot(u, hit_normal) * hit_normal;               // Reflect the velocity along the hit normal
    //     velocity -= glm::dot(velocity, hit_normal) * hit_normal; // Adjust the velocity
    // }
}

assets::AnimIdx game::Character::GetAnim(const std::string& name) const
{
    return sk_.GetSkeleton()->GetAnimationIdx(name);
}

game::CharacterPhysicsController::CharacterPhysicsController(btDynamicsWorld& bt_world, btCapsuleShapeZ& bt_shape)
    : bt_world_(bt_world), bt_character_(&bt_ghost_, &bt_shape, 0.3f, btVector3(0, 0, 1))
{
    btTransform start_transform;
    start_transform.setIdentity();
    bt_ghost_.setWorldTransform(start_transform);
    bt_ghost_.setCollisionShape(&bt_shape);
    bt_ghost_.setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

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
