#include "character.hpp"
#include "world.hpp"
#include "net/utils.hpp"

game::Character::Character(World& world, const CharacterInfo& info)
    : Super(world, net::ET_CHARACTER), shape_(info.shape), bt_shape_(shape_.radius, shape_.height),
      bt_character_(&bt_ghost_, &bt_shape_, 0.3f, btVector3(0, 0, 1))
{
    btTransform start_transform;
    start_transform.setIdentity();
    bt_ghost_.setWorldTransform(start_transform);
    bt_ghost_.setCollisionShape(&bt_shape_);
    bt_ghost_.setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

    
    btDynamicsWorld& bt_world = world_.GetBtWorld();
    static btGhostPairCallback ghostpaircb;
    bt_world.getBroadphase()->getOverlappingPairCache()->setInternalGhostPairCallback(&ghostpaircb);
    // bt_world.addCollisionObject(&bt_ghost_, btBroadphaseProxy::CharacterFilter,
    //                             btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter);
    bt_world.addCollisionObject(&bt_ghost_);
    bt_world.addAction(&bt_character_);
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

    auto bt_trans = bt_ghost_.getWorldTransform();
    root_.local.SetBtTransform(bt_trans);

    UpdateMovement();
    SendUpdateMsg();
}

void game::Character::SendInitData(Player& player, net::OutMessage& msg) const
{
    Super::SendInitData(player, msg);
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
    auto trans = bt_ghost_.getWorldTransform();
    trans.setOrigin(btVector3(position.x, position.y, position.z));
    bt_ghost_.setWorldTransform(trans);
}

game::Character::~Character()
{
    btDynamicsWorld& bt_world = world_.GetBtWorld();
    bt_world.removeAction(&bt_character_);
    bt_world.removeCollisionObject(&bt_ghost_);
}

void game::Character::UpdateMovement()
{
    constexpr float dt = 1.0f / 25.0f;

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
        float target_yaw = forward_yaw_ + std::atan2(movedir.y, movedir.x);
        Turn(yaw_, target_yaw, 4.0f * dt);

        glm::vec3 forward_dir(glm::cos(yaw_), glm::sin(yaw_), 0.0f);
        walkdir = forward_dir * walk_speed_ * dt;
    }

    bt_character_.setWalkDirection(btVector3(walkdir.x, walkdir.y, walkdir.z));

    if (in_ & (1 << CIN_JUMP) && bt_character_.canJump())
    {
        bt_character_.jump(btVector3(0.0f, 0.0f, 10.0f));
    }
}

void game::Character::SendUpdateMsg()
{  
    net::PositionQ posq;
    net::EncodePosition(root_.local.position, posq);

    auto msg = BeginEntMsg(net::EMSG_UPDATE);
    net::WritePositionQ(msg, posq);
    msg.Write<net::PositiveAngleQ>(yaw_);
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
