#include "kinematic_character_controller.hpp"

#include <BulletCollision/CollisionDispatch/btCollisionWorld.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletCollision/CollisionShapes/btConvexShape.h>
#include <LinearMath/btIDebugDraw.h>
#include <LinearMath/btTransformUtil.h>

#include <cmath>

namespace
{
constexpr btScalar skin = btScalar(0.003);
constexpr btScalar probeDistance = skin * 3;
constexpr btScalar motionEpsilon = btScalar(1e-6);
constexpr int maxSlideIterations = 8;
constexpr int maxPlanes = maxSlideIterations * 2;

// Find the closest vector satisfying every contact plane. Trying individual
// planes and their intersection lines prevents alternating projections from
// pushing the character through the other wall of a corner.
btVector3 clip(const btVector3& desired, const btVector3* planes, int count)
{
    auto allowed = [&](const btVector3& value) {
        for (int i = 0; i < count; ++i)
            if (value.dot(planes[i]) < -motionEpsilon)
                return false;
        return true;
    };
    if (allowed(desired))
        return desired;

    btVector3 best(0, 0, 0);
    btScalar bestError = desired.length2();
    auto consider = [&](const btVector3& candidate) {
        btScalar error = (candidate - desired).length2();
        if (error < bestError && allowed(candidate))
        {
            best = candidate;
            bestError = error;
        }
    };
    for (int i = 0; i < count; ++i)
    {
        consider(desired - planes[i] * desired.dot(planes[i]));
        for (int j = 0; j < i; ++j)
        {
            btVector3 crease = planes[i].cross(planes[j]);
            btScalar length2 = crease.length2();
            if (length2 > motionEpsilon * motionEpsilon)
                consider(crease * (desired.dot(crease) / length2));
        }
    }
    return best;
}
} // namespace

namespace collision
{

struct KinematicCharacterController::SweepCallback : btCollisionWorld::ClosestConvexResultCallback
{
    KinematicCharacterController& controller;
    btVector3 displacement;
    bool supportOnly;

    SweepCallback(KinematicCharacterController& owner, const btVector3& start, const btVector3& delta,
                  bool support = false)
        : ClosestConvexResultCallback(start, start + delta), controller(owner), displacement(delta),
          supportOnly(support)
    {
        if (const auto* proxy = owner.ghost_->getBroadphaseHandle())
        {
            m_collisionFilterGroup = proxy->m_collisionFilterGroup;
            m_collisionFilterMask = proxy->m_collisionFilterMask;
        }
    }

    bool needsCollision(btBroadphaseProxy* proxy) const override
    {
        return ClosestConvexResultCallback::needsCollision(proxy) &&
               controller.needsCollision(controller.ghost_, static_cast<btCollisionObject*>(proxy->m_clientObject));
    }

    btScalar addSingleResult(btCollisionWorld::LocalConvexResult& result, bool worldNormal) override
    {
        // Filter here too: compound children can reach this callback directly.
        if (!controller.needsCollision(controller.ghost_, result.m_hitCollisionObject))
            return m_closestHitFraction;
        btVector3 normal = result.m_hitNormalLocal;
        if (!worldNormal)
            normal = result.m_hitCollisionObject->getWorldTransform().getBasis() * normal;
        if (normal.length2() < motionEpsilon * motionEpsilon)
            return m_closestHitFraction;
        normal.normalize();
        if (supportOnly && !controller.walkable(normal))
            return m_closestHitFraction;

        // Ignore tangencies and surfaces we are leaving. Movement queries keep
        // every blocking slope; the support-only query never moves the capsule.
        if (displacement.length2() > motionEpsilon * motionEpsilon &&
            normal.dot(displacement) >= -motionEpsilon * displacement.length())
            return m_closestHitFraction;

        btScalar fraction = ClosestConvexResultCallback::addSingleResult(result, worldNormal);
        m_hitNormalWorld = normal;
        return fraction;
    }
};

struct KinematicCharacterController::ContactCallback : btCollisionWorld::ContactResultCallback
{
    KinematicCharacterController& controller;
    struct Contact
    {
        btVector3 normal;
        btScalar depth;
    };
    btAlignedObjectArray<Contact> contacts;

    explicit ContactCallback(KinematicCharacterController& owner) : controller(owner)
    {
        if (const auto* proxy = owner.ghost_->getBroadphaseHandle())
        {
            m_collisionFilterGroup = proxy->m_collisionFilterGroup;
            m_collisionFilterMask = proxy->m_collisionFilterMask;
        }
    }

    bool needsCollision(btBroadphaseProxy* proxy) const override
    {
        return ContactResultCallback::needsCollision(proxy) &&
               controller.needsCollision(controller.ghost_, static_cast<btCollisionObject*>(proxy->m_clientObject));
    }

    btScalar addSingleResult(btManifoldPoint& point, const btCollisionObjectWrapper* a, int, int,
                             const btCollisionObjectWrapper* b, int, int) override
    {
        const bool ghostIsA = a->getCollisionObject() == controller.ghost_;
        const btCollisionObject* other = ghostIsA ? b->getCollisionObject() : a->getCollisionObject();
        if (!controller.needsCollision(controller.ghost_, other))
            return 0;
        btScalar depth = -point.getDistance() - controller.penetrationTolerance_;
        if (depth > 0)
        {
            btVector3 normal = ghostIsA ? point.m_normalWorldOnB : -point.m_normalWorldOnB;
            if (normal.length2() > motionEpsilon * motionEpsilon)
                contacts.push_back({normal.normalized(), depth});
        }
        return 0;
    }
};

KinematicCharacterController::KinematicCharacterController(btPairCachingGhostObject* ghostObject,
                                                           btConvexShape* convexShape, btScalar stepHeight,
                                                           const btVector3& up)
    : ghost_(ghostObject), shape_(convexShape), up_(up.length2() > 0 ? up.normalized() : btVector3(0, 0, 1))
{
    btAssert(ghost_ && shape_);
    setStepHeight(stepHeight);
    setMaxSlope(SIMD_PI / 4);
}

bool KinematicCharacterController::needsCollision(const btCollisionObject* a, const btCollisionObject* b)
{
    if (!a || !b || a == b || !a->hasContactResponse() || !b->hasContactResponse() || !a->checkCollideWith(b) ||
        !b->checkCollideWith(a))
        return false;
    const auto* pa = a->getBroadphaseHandle();
    const auto* pb = b->getBroadphaseHandle();
    return !pa || !pb ||
           ((pa->m_collisionFilterGroup & pb->m_collisionFilterMask) &&
            (pb->m_collisionFilterGroup & pa->m_collisionFilterMask));
}

bool KinematicCharacterController::walkable(const btVector3& normal) const
{
    return normal.dot(up_) >= slopeCosine_;
}

KinematicCharacterController::Hit KinematicCharacterController::sweep(btCollisionWorld* world, const btVector3& start,
                                                                      const btVector3& displacement, bool supportOnly)
{
    Hit hit;
    if (displacement.length2() <= motionEpsilon * motionEpsilon)
        return hit;
    btTransform from(orientation_, start), to(orientation_, start + displacement);
    SweepCallback callback(*this, start, displacement, supportOnly);
    // World queries see the complete swept AABB, including triangles not in the
    // ghost's current pair cache. Zero CCD allowance is deliberate for thin meshes.
    world->convexSweepTest(shape_, from, to, callback, 0);
    if (callback.hasHit())
    {
        hit.hit = true;
        hit.fraction = callback.m_closestHitFraction;
        hit.normal = callback.m_hitNormalWorld;
    }
    return hit;
}

btScalar KinematicCharacterController::safeFraction(const Hit& hit, const btVector3& delta) const
{
    if (!hit.hit)
        return 1;
    // Keep a small gap measured along the contact normal. Never change a shared
    // convex shape's margin, including while testing a speculative stair move.
    btScalar approach = -delta.dot(hit.normal);
    return btMax(btScalar(0), hit.fraction - skin / btMax(approach, motionEpsilon));
}

KinematicCharacterController::Move KinematicCharacterController::slide(btCollisionWorld* world, const btVector3& start,
                                                                       const btVector3& displacement, bool walking,
                                                                       bool stopAtFloor)
{
    Move move;
    move.position = start;
    btVector3 remaining = displacement;
    btVector3 planes[maxPlanes];
    int planeCount = 0;
    for (int iteration = 0; iteration < maxSlideIterations && remaining.length2() > motionEpsilon * motionEpsilon;
         ++iteration)
    {
        Hit hit = sweep(world, move.position, remaining);
        btScalar fraction = safeFraction(hit, remaining);
        move.position += remaining * fraction;
        if (!hit.hit)
            break;
        move.blocked = true;
        if (stopAtFloor && walkable(hit.normal))
        {
            move.floor = true;
            move.floorNormal = hit.normal;
            break;
        }
        remaining *= 1 - fraction;
        planes[planeCount++] = hit.normal;
        btScalar upDot = hit.normal.dot(up_);
        if (walking && upDot > 0 && !walkable(hit.normal))
        {
            // A steep slope acts as a wall to input, but keeps its real normal
            // for gravity. This prevents wall climbing without freezing falls.
            planes[planeCount++] = (hit.normal - up_ * upDot).normalized();
        }
        remaining = clip(remaining, planes, planeCount);
    }
    return move;
}

bool KinematicCharacterController::tryStep(btCollisionWorld* world, const btVector3& start,
                                           const btVector3& displacement, Move& move)
{
    btVector3 lift = up_ * stepHeight_;
    Hit ceiling = sweep(world, start, lift);
    lift *= safeFraction(ceiling, lift);
    if (lift.dot(up_) <= probeDistance)
        return false;

    Move raised = slide(world, start + lift, displacement, true, false);
    btVector3 down = -up_ * (lift.dot(up_) + probeDistance);
    Hit floor = sweep(world, raised.position, down);
    if (!floor.hit)
        return false;
    btVector3 forward = displacement - up_ * displacement.dot(up_);
    if (forward.length2() <= motionEpsilon * motionEpsilon)
        return false;
    forward.normalize();
    if (!walkable(floor.normal))
    {
        // At a curb the capsule first touches the tread's edge with a steep
        // normal. Verify that a walkable tread exists within one shape radius,
        // using another fully swept path. A continuous steep ramp fails this
        // test, so repeated stair attempts cannot turn into wall climbing.
        btMatrix3x3 basis(orientation_);
        btVector3 support = basis * shape_->localGetSupportingVertex(basis.transpose() * forward);
        btScalar reach = btMax(skin, support.dot(forward));
        Move ahead = slide(world, raised.position, forward * reach, true, false);
        Hit tread = sweep(world, ahead.position, down);
        if (!tread.hit || !walkable(tread.normal))
            return false;
    }
    raised.position += down * safeFraction(floor, down);
    if ((raised.position - move.position).dot(forward) <= skin ||
        (raised.position - start).dot(up_) > stepHeight_ + skin)
        return false;

    raised.floor = walkable(floor.normal);
    raised.floorNormal = floor.normal;
    move = raised;
    stepping_ = !raised.floor;
    return true;
}

bool KinematicCharacterController::findGround(btCollisionWorld* world, btScalar distance)
{
    grounded_ = false;
    if (verticalVelocity_ > 0)
        return false;
    btVector3 down = -up_ * distance;
    Hit hit = sweep(world, position_, down);
    if (!hit.hit)
        return false;
    if (!walkable(hit.normal))
    {
        // At a floor/wall junction a steep edge can be the first hit even while
        // the feet touch a walkable floor. Check support within the short skin
        // probe, but DO NOT move through that first blocking edge to reach it.
        hit = sweep(world, position_, -up_ * probeDistance, true);
        if (!hit.hit)
            return false;
    }
    else
        position_ += down * safeFraction(hit, down);
    groundNormal_ = hit.normal;
    grounded_ = true;
    jumping_ = false;
    verticalVelocity_ = 0;
    jumpDrift_.setZero();
    return true;
}

void KinematicCharacterController::writePosition(btCollisionWorld* world)
{
    ghost_->setWorldTransform(btTransform(orientation_, position_));
    if (ghost_->getBroadphaseHandle())
        world->updateSingleAabb(ghost_);
}

void KinematicCharacterController::recoverPenetration(btCollisionWorld* world)
{
    // Fresh contact queries after each correction; no stale ghost manifolds and
    // no sum of identical triangle corrections (which depends on tessellation).
    for (int iteration = 0; iteration < 8; ++iteration)
    {
        writePosition(world);
        ContactCallback callback(*this);
        world->contactTest(ghost_, callback);
        if (callback.contacts.size() == 0)
            break;
        btVector3 correction(0, 0, 0);
        for (int pass = 0; pass < 4; ++pass)
            for (int i = 0; i < callback.contacts.size(); ++i)
            {
                const auto& contact = callback.contacts[i];
                btScalar missing = contact.depth - correction.dot(contact.normal);
                if (missing > 0)
                    correction += contact.normal * missing;
            }
        btScalar length = correction.length();
        if (length <= motionEpsilon)
            break;
        // Bound recovery of bad spawn positions or moving-world overlaps.
        position_ += correction * (btMin(length, btScalar(0.2)) / length);
    }
    writePosition(world);
}

void KinematicCharacterController::rotate(btCollisionWorld* world, btScalar dt)
{
    if (angularVelocity_.length2() <= motionEpsilon * motionEpsilon)
        return;
    btTransform from(orientation_, position_), to;
    btTransformUtil::integrateTransform(from, btVector3(0, 0, 0), angularVelocity_, dt, to);
    SweepCallback callback(*this, position_, btVector3(0, 0, 0));
    world->convexSweepTest(shape_, from, to, callback, 0);
    orientation_ = orientation_.slerp(to.getRotation(), callback.m_closestHitFraction).normalized();
}

void KinematicCharacterController::simulate(btCollisionWorld* world, btScalar dt, const btVector3& displacement)
{
    rotate(world, dt);
    bool supported = grounded_ && verticalVelocity_ <= 0 && btFabs(displacement.dot(up_)) <= motionEpsilon;
    bool canStep = (supported || stepping_) && verticalVelocity_ <= 0 && !jumping_;
    stepping_ = false;
    btVector3 lateral = displacement + jumpDrift_ * dt;
    if (supported)
    {
        // Follow the walkable surface while preserving the requested horizontal
        // displacement. Airborne input is not projected onto an old ground plane.
        btVector3 horizontal = lateral - up_ * lateral.dot(up_);
        lateral = horizontal - up_ * (horizontal.dot(groundNormal_) / groundNormal_.dot(up_));
    }
    Move move = slide(world, position_, lateral, true, false);
    if (canStep && move.blocked && stepHeight_ > 0 && tryStep(world, position_, lateral, move))
        verticalVelocity_ = 0;
    position_ = move.position;

    // Ground snap is available only to a previously supported character. A
    // falling character cannot teleport down a step or acquire midair support.
    if (supported && findGround(world, stepHeight_ + probeDistance))
    {
        writePosition(world);
        return;
    }
    grounded_ = false;

    btScalar oldVelocity = verticalVelocity_;
    verticalVelocity_ = btMax(verticalVelocity_ - gravity_ * dt, -fallSpeed_);
    btScalar height = (oldVelocity + verticalVelocity_) * btScalar(0.5) * dt;
    if (jumping_ && maxJumpHeight_ > 0 && height > 0)
    {
        btScalar left = btMax(btScalar(0), jumpStartHeight_ + maxJumpHeight_ - position_.dot(up_));
        if (height >= left)
        {
            height = left;
            verticalVelocity_ = 0;
        }
    }
    if (height > 0)
    {
        btVector3 rise = up_ * height;
        Hit hit = sweep(world, position_, rise);
        position_ += rise * safeFraction(hit, rise);
        if (hit.hit)
            verticalVelocity_ = 0; // ceilings never imply grounded
    }
    else if (height < 0)
    {
        Move fall = slide(world, position_, up_ * height, false, true);
        position_ = fall.position;
        if (fall.floor)
        {
            grounded_ = true;
            groundNormal_ = fall.floorNormal;
            verticalVelocity_ = 0;
            jumping_ = false;
            jumpDrift_.setZero();
        }
    }
    findGround(world, probeDistance);
    recoverPenetration(world);
}

void KinematicCharacterController::updateAction(btCollisionWorld* world, btScalar dt)
{
    preStep(world);
    playerStep(world, dt);
}

void KinematicCharacterController::preStep(btCollisionWorld* world)
{
    position_ = ghost_->getWorldTransform().getOrigin();
    orientation_ = ghost_->getWorldTransform().getRotation();
    recoverPenetration(world);
    findGround(world, probeDistance);
    writePosition(world);
}

void KinematicCharacterController::playerStep(btCollisionWorld* world, btScalar dt)
{
    if (!(dt > 0) || !std::isfinite(double(dt)))
        return;
    // Subdivide integration and stair trials, not the walk-direction contract.
    // Every segment is swept even for large input displacements or frame hitches.
    int steps = int(btMin(btScalar(128), btScalar(std::ceil(double(dt * 60)))));
    btScalar subDt = dt / steps;
    lastDt_ = dt;
    for (int step = 0; step < steps; ++step)
    {
        input_ *= btPow(1 - linearDamping_, subDt);
        verticalVelocity_ *= btPow(1 - linearDamping_, subDt);
        jumpDrift_ *= btPow(1 - linearDamping_, subDt);
        angularVelocity_ *= btPow(1 - angularDamping_, subDt);
        btVector3 displacement;
        if (mode_ == InputMode::Displacement)
            displacement = input_ / btScalar(steps);
        else if (mode_ == InputMode::TimedVelocity)
        {
            btScalar movingTime = btMin(subDt, interval_);
            displacement = input_ * movingTime;
            interval_ = btMax(btScalar(0), interval_ - subDt);
        }
        else
            displacement = input_ * subDt;
        simulate(world, subDt, displacement);
    }
}

void KinematicCharacterController::setWalkDirection(const btVector3& displacement)
{
    mode_ = InputMode::Displacement;
    input_ = displacement;
    interval_ = 0;
}

void KinematicCharacterController::setVelocityForTimeInterval(const btVector3& velocity, btScalar interval)
{
    mode_ = InputMode::TimedVelocity;
    input_ = velocity;
    interval_ = btMax(btScalar(0), interval);
}

void KinematicCharacterController::setLinearVelocity(const btVector3& velocity)
{
    mode_ = InputMode::Velocity;
    verticalVelocity_ = velocity.dot(up_);
    input_ = velocity - up_ * verticalVelocity_;
    jumpDrift_.setZero();
    jumping_ = verticalVelocity_ > 0;
    if (verticalVelocity_ != 0)
        grounded_ = false;
    jumpStartHeight_ = ghost_->getWorldTransform().getOrigin().dot(up_);
}

btVector3 KinematicCharacterController::getLinearVelocity() const
{
    btVector3 velocity = input_;
    if (mode_ == InputMode::Displacement)
        velocity = lastDt_ > 0 ? input_ / lastDt_ : btVector3(0, 0, 0);
    else if (mode_ == InputMode::TimedVelocity && interval_ <= 0)
        velocity.setZero();
    return velocity + up_ * verticalVelocity_ + jumpDrift_;
}

void KinematicCharacterController::setAngularVelocity(const btVector3& velocity)
{
    angularVelocity_ = velocity;
}
void KinematicCharacterController::setLinearDamping(btScalar d)
{
    linearDamping_ = btClamped(d, btScalar(0), btScalar(1));
}
void KinematicCharacterController::setAngularDamping(btScalar d)
{
    angularDamping_ = btClamped(d, btScalar(0), btScalar(1));
}
void KinematicCharacterController::setStepHeight(btScalar h)
{
    stepHeight_ = btMax(btScalar(0), h);
}
void KinematicCharacterController::setFallSpeed(btScalar speed)
{
    fallSpeed_ = btFabs(speed);
}
void KinematicCharacterController::setJumpSpeed(btScalar speed)
{
    jumpSpeed_ = btFabs(speed);
}
void KinematicCharacterController::setMaxJumpHeight(btScalar h)
{
    maxJumpHeight_ = btMax(btScalar(0), h);
}
void KinematicCharacterController::setMaxPenetrationDepth(btScalar d)
{
    penetrationTolerance_ = btMax(btScalar(0), d);
}

void KinematicCharacterController::setMaxSlope(btScalar radians)
{
    maxSlope_ = btClamped(radians, btScalar(0), SIMD_HALF_PI - btScalar(0.001));
    slopeCosine_ = btCos(maxSlope_);
}

void KinematicCharacterController::setUp(const btVector3& up)
{
    if (up.length2() <= motionEpsilon * motionEpsilon)
        return;
    btVector3 next = up.normalized();
    btTransform transform = ghost_->getWorldTransform();
    transform.setRotation((shortestArcQuat(up_, next) * transform.getRotation()).normalized());
    ghost_->setWorldTransform(transform);
    up_ = next;
    grounded_ = false;
    stepping_ = false;
}

void KinematicCharacterController::setGravity(const btVector3& gravity)
{
    gravity_ = gravity.length();
    if (gravity_ > motionEpsilon)
        setUp(-gravity / gravity_);
}

void KinematicCharacterController::jump(const btVector3& velocity)
{
    btVector3 launch = velocity.length2() > 0 ? velocity : up_ * jumpSpeed_;
    verticalVelocity_ = launch.dot(up_);
    jumpDrift_ = launch - up_ * verticalVelocity_;
    jumpStartHeight_ = ghost_->getWorldTransform().getOrigin().dot(up_);
    grounded_ = false;
    jumping_ = true;
    stepping_ = false;
}

void KinematicCharacterController::reset(btCollisionWorld* world)
{
    input_.setZero();
    jumpDrift_.setZero();
    angularVelocity_.setZero();
    interval_ = 0;
    verticalVelocity_ = 0;
    grounded_ = false;
    jumping_ = false;
    stepping_ = false;
    mode_ = InputMode::Displacement;
    position_ = ghost_->getWorldTransform().getOrigin();
    orientation_ = ghost_->getWorldTransform().getRotation();
    if (world && ghost_->getBroadphaseHandle())
    {
        world->getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(ghost_->getBroadphaseHandle(),
                                                                               world->getDispatcher());
        world->updateSingleAabb(ghost_);
    }
}

void KinematicCharacterController::warp(const btVector3& origin)
{
    btTransform transform = ghost_->getWorldTransform();
    transform.setOrigin(origin);
    ghost_->setWorldTransform(transform);
    position_ = origin;
    grounded_ = false;
    stepping_ = false;
    // The next preStep refreshes contacts and the broadphase in its world.
}

void KinematicCharacterController::debugDraw(btIDebugDraw* drawer)
{
    if (drawer && grounded_)
        drawer->drawLine(position_, position_ + groundNormal_, btVector3(0, 1, 0));
}

} // namespace collision
