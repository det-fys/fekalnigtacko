#pragma once

#include <BulletDynamics/Character/btCharacterControllerInterface.h>
#include <LinearMath/btTransform.h>

class btConvexShape;
class btPairCachingGhostObject;

namespace collision
{

// Source-compatible movement API for Bullet's kinematic controller. The ghost
// and shape remain owned by the caller. The supplied transform/shape must already
// be oriented along up; construction does not rotate them.
class KinematicCharacterController : public btCharacterControllerInterface
{
public:
    BT_DECLARE_ALIGNED_ALLOCATOR();

    KinematicCharacterController(btPairCachingGhostObject* ghostObject, btConvexShape* convexShape, btScalar stepHeight,
                                 const btVector3& up = btVector3(1, 0, 0));

    void updateAction(btCollisionWorld* world, btScalar dt) override;
    void debugDraw(btIDebugDraw* drawer) override;
    void preStep(btCollisionWorld* world) override;
    void playerStep(btCollisionWorld* world, btScalar dt) override;
    void reset(btCollisionWorld* world) override;
    void warp(const btVector3& origin) override;

    // Displacement per updateAction/playerStep, independent of dt (Bullet API).
    void setWalkDirection(const btVector3& displacement) override;
    // Velocity in world units/second. Replaces the previous interval.
    void setVelocityForTimeInterval(const btVector3& velocity, btScalar interval) override;
    // Persistent velocity in world units/second; the up component sets jump/fall
    // velocity. Gravity continues after a timed velocity expires.
    virtual void setLinearVelocity(const btVector3& velocity);
    virtual btVector3 getLinearVelocity() const;
    virtual void setAngularVelocity(const btVector3& velocity);
    virtual const btVector3& getAngularVelocity() const { return angularVelocity_; }

    void setLinearDamping(btScalar damping);
    btScalar getLinearDamping() const { return linearDamping_; }
    void setAngularDamping(btScalar damping);
    btScalar getAngularDamping() const { return angularDamping_; }

    void setUp(const btVector3& up);
    const btVector3& getUp() const { return up_; }
    void setGravity(const btVector3& gravity);
    btVector3 getGravity() const { return -up_ * gravity_; }
    void setStepHeight(btScalar height);
    btScalar getStepHeight() const { return stepHeight_; }
    void setFallSpeed(btScalar speed);
    btScalar getFallSpeed() const { return fallSpeed_; }
    void setJumpSpeed(btScalar speed);
    btScalar getJumpSpeed() const { return jumpSpeed_; }
    void setMaxJumpHeight(btScalar height);
    void setMaxSlope(btScalar radians);
    btScalar getMaxSlope() const { return maxSlope_; }
    void setMaxPenetrationDepth(btScalar depth);
    btScalar getMaxPenetrationDepth() const { return penetrationTolerance_; }

    bool onGround() const override { return grounded_; }
    bool canJump() const override { return grounded_; }
    // Like Bullet, jump/applyImpulse may also be used to force an airborne jump.
    void jump(const btVector3& velocity = btVector3(0, 0, 0)) override;
    void applyImpulse(const btVector3& velocity) { jump(velocity); }
    btPairCachingGhostObject* getGhostObject() { return ghost_; }

    // Compatibility switches: all moves always use world sweeps and stop at the
    // hit fraction. Ghost-cache-only sweeps and unchecked step-up are unsafe.
    void setUseGhostSweepTest(bool) {}
    void setUpInterpolate(bool) override {}

protected:
    virtual bool needsCollision(const btCollisionObject* body0, const btCollisionObject* body1);

private:
    struct SweepCallback;
    struct ContactCallback;
    struct Hit
    {
        bool hit = false;
        btScalar fraction = 1;
        btVector3 normal{0, 0, 0};
    };
    struct Move
    {
        btVector3 position{0, 0, 0};
        bool blocked = false;
        bool floor = false;
        btVector3 floorNormal{0, 0, 0};
    };
    enum class InputMode
    {
        Displacement,
        TimedVelocity,
        Velocity
    };

    Hit sweep(btCollisionWorld* world, const btVector3& start, const btVector3& displacement, bool supportOnly = false);
    btScalar safeFraction(const Hit& hit, const btVector3& displacement) const;
    Move slide(btCollisionWorld* world, const btVector3& start, const btVector3& displacement, bool walking,
               bool stopAtFloor);
    bool tryStep(btCollisionWorld* world, const btVector3& start, const btVector3& displacement, Move& move);
    bool findGround(btCollisionWorld* world, btScalar distance);
    void recoverPenetration(btCollisionWorld* world);
    void rotate(btCollisionWorld* world, btScalar dt);
    void simulate(btCollisionWorld* world, btScalar dt, const btVector3& displacement);
    void writePosition(btCollisionWorld* world);
    bool walkable(const btVector3& normal) const;

    btPairCachingGhostObject* ghost_;
    btConvexShape* shape_;
    btVector3 up_;
    btVector3 position_{0, 0, 0};
    btQuaternion orientation_{0, 0, 0, 1};
    btVector3 input_{0, 0, 0};
    btVector3 jumpDrift_{0, 0, 0};
    btVector3 angularVelocity_{0, 0, 0};
    btVector3 groundNormal_{0, 0, 0};
    InputMode mode_ = InputMode::Displacement;
    btScalar interval_ = 0;
    btScalar lastDt_ = 0;
    btScalar verticalVelocity_ = 0;
    btScalar gravity_ = btScalar(29.4);
    btScalar fallSpeed_ = 55;
    btScalar jumpSpeed_ = 10;
    btScalar maxJumpHeight_ = 0; // zero means unlimited
    btScalar jumpStartHeight_ = 0;
    btScalar stepHeight_;
    btScalar maxSlope_;
    btScalar slopeCosine_;
    btScalar penetrationTolerance_ = btScalar(0.001);
    btScalar linearDamping_ = 0;
    btScalar angularDamping_ = 0;
    bool grounded_ = false;
    bool jumping_ = false;
    bool stepping_ = false; // a verified tread is just ahead of a capsule edge
};

} // namespace collision
