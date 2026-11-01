#include "../kinematic_character_controller.hpp"

#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <btBulletDynamicsCommon.h>
#ifdef TEST_ORIGINAL_CONTROLLER
#include <BulletDynamics/Character/btKinematicCharacterController.h>
#endif

#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
#ifdef TEST_ORIGINAL_CONTROLLER
using Controller = btKinematicCharacterController;
#else
using Controller = collision::KinematicCharacterController;
#endif
constexpr btScalar dt = btScalar(0.04);        // the game's 25 Hz update
constexpr btScalar halfHeight = btScalar(0.9); // radius .35 + cylinder half-height .55

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

struct Scene
{
    btDefaultCollisionConfiguration config;
    btCollisionDispatcher dispatcher{&config};
    btDbvtBroadphase broadphase;
    btGhostPairCallback ghostPairs;
    btSequentialImpulseConstraintSolver solver;
    btDiscreteDynamicsWorld world{&dispatcher, &broadphase, &solver, &config};
    std::vector<std::unique_ptr<btTriangleMesh>> meshes;
    std::vector<std::unique_ptr<btCollisionShape>> shapes;
    std::vector<std::unique_ptr<btCollisionObject>> objects;
    btCapsuleShapeZ capsule{btScalar(0.35), btScalar(1.1)};
    btPairCachingGhostObject ghost;
    std::unique_ptr<Controller> controller;

    explicit Scene(const btVector3& position, const btVector3& up = btVector3(0, 0, 1))
    {
        // Enable this even for the original controller, so baseline failures
        // cannot be attributed to forgetting Bullet's required ghost callback.
        broadphase.getOverlappingPairCache()->setInternalGhostPairCallback(&ghostPairs);
        ghost.setCollisionShape(&capsule);
        ghost.setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);
        btQuaternion orientation = shortestArcQuat(btVector3(0, 0, 1), up);
        ghost.setWorldTransform(btTransform(orientation, position));
        controller = std::make_unique<Controller>(&ghost, &capsule, btScalar(0.3), up);
        world.addCollisionObject(&ghost, btBroadphaseProxy::CharacterFilter,
                                 btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter);
        world.addAction(controller.get());
    }
    ~Scene()
    {
        world.removeAction(controller.get());
        world.removeCollisionObject(&ghost);
        for (const auto& object : objects)
            world.removeCollisionObject(object.get());
        // All broadphase proxies must die before the ghost-pair callback.
        broadphase.getOverlappingPairCache()->setInternalGhostPairCallback(nullptr);
    }
    btCollisionObject& add(std::unique_ptr<btCollisionShape> shape, const btTransform& transform,
                           int group = btBroadphaseProxy::StaticFilter,
                           int mask = btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::StaticFilter)
    {
        auto object = std::make_unique<btCollisionObject>();
        object->setCollisionShape(shape.get());
        object->setWorldTransform(transform);
        object->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT);
        world.addCollisionObject(object.get(), group, mask);
        shapes.push_back(std::move(shape));
        objects.push_back(std::move(object));
        return *objects.back();
    }
    btCollisionObject& box(const btVector3& center, const btVector3& halfSize,
                           int group = btBroadphaseProxy::StaticFilter,
                           int mask = btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::StaticFilter)
    {
        return add(std::make_unique<btBoxShape>(halfSize), btTransform(btQuaternion::getIdentity(), center), group,
                   mask);
    }
    // Quad winding can be reversed independently of the controller's up vector.
    btCollisionObject& quad(const btVector3& a, const btVector3& b, const btVector3& c, const btVector3& d,
                            bool reverse = false)
    {
        auto mesh = std::make_unique<btTriangleMesh>();
        if (reverse)
        {
            mesh->addTriangle(a, c, b);
            mesh->addTriangle(a, d, c);
        }
        else
        {
            mesh->addTriangle(a, b, c);
            mesh->addTriangle(a, c, d);
        }
        auto shape = std::make_unique<btBvhTriangleMeshShape>(mesh.get(), true);
        meshes.push_back(std::move(mesh));
        return add(std::move(shape), btTransform::getIdentity());
    }
    void floor()
    {
        quad(btVector3(-100, -100, 0), btVector3(100, -100, 0), btVector3(100, 100, 0), btVector3(-100, 100, 0));
    }
    btVector3 position() const { return ghost.getWorldTransform().getOrigin(); }
    void tick(int count = 1, btScalar step = dt)
    {
        for (int i = 0; i < count; ++i)
        {
            world.stepSimulation(step, 0);
            btVector3 p = position();
            require(std::isfinite(double(p.x())) && std::isfinite(double(p.y())) && std::isfinite(double(p.z())),
                    "non-finite position");
        }
    }
    void settle()
    {
        tick(80);
        require(controller->onGround(), "did not settle on ground");
    }
    void noPenetration(btScalar tolerance = btScalar(0.012))
    {
        struct Contacts : btCollisionWorld::ContactResultCallback
        {
            btScalar deepest = 0;
            btScalar addSingleResult(btManifoldPoint& p, const btCollisionObjectWrapper*, int, int,
                                     const btCollisionObjectWrapper*, int, int) override
            {
                deepest = btMax(deepest, -p.getDistance());
                return 0;
            }
        } callback;
        world.contactTest(&ghost, callback);
        require(callback.deepest <= tolerance, "penetration depth " + std::to_string(double(callback.deepest)));
    }
};

void flatGround()
{
    Scene s(btVector3(0, 0, 4));
    s.floor();
    s.settle();
    require(btFabs(s.position().z() - halfHeight) < btScalar(0.06), "wrong resting height");
    btVector3 initial = s.position();
    s.tick(600);
    require((s.position() - initial).length() < btScalar(0.005), "standing character drifts");
    s.controller->setWalkDirection(btVector3(btScalar(0.1), 0, 0));
    s.tick(50);
    require(btFabs(s.position().x() - 5) < btScalar(0.03), "walk displacement changed during substeps");
    s.noPenetration();
}

void ceiling(bool reverse, bool ghostSweep)
{
    Scene s(btVector3(0, 0, 1));
    s.floor();
    s.quad(btVector3(-5, -5, btScalar(2.4)), btVector3(5, -5, btScalar(2.4)), btVector3(5, 5, btScalar(2.4)),
           btVector3(-5, 5, btScalar(2.4)), reverse);
    s.controller->setUseGhostSweepTest(ghostSweep);
    s.settle();
    s.controller->jump(btVector3(0, 0, 25));
    for (int i = 0; i < 80; ++i)
    {
        s.tick();
        require(s.position().z() + halfHeight < btScalar(2.415), "jump passed through a triangle ceiling");
        if (s.position().z() > btScalar(1.1))
            require(!s.controller->onGround(), "ceiling contact granted a jump");
        s.noPenetration();
    }
    require(s.controller->canJump(), "did not land after hitting ceiling");
}

void fallingSteepMesh(btScalar angle, bool reverse, bool oversizedTriangles = false)
{
    btScalar gradient = btTan(btRadians(angle));
    btVector3 normal(-gradient, 0, 1);
    normal.normalize();
    Scene s(btVector3(0, 0, 8));
    if (oversizedTriangles)
        s.quad(btVector3(-300, -30, -300 * gradient), btVector3(300, -30, 300 * gradient),
               btVector3(300, 30, 300 * gradient), btVector3(-300, 30, -300 * gradient), reverse);
    else
    {
        // Five-unit triangles along the surface, including many shared edges.
        // Huge, thin triangles lose GJK accuracy in Bullet's single precision.
        auto mesh = std::make_unique<btTriangleMesh>();
        btVector3 tangent(btCos(btRadians(angle)), 0, btSin(btRadians(angle)));
        for (int x = -80; x < 80; ++x)
            for (int y = -2; y < 2; ++y)
            {
                btVector3 a = tangent * btScalar(x * 5) + btVector3(0, y * 5, 0);
                btVector3 b = a + tangent * 5;
                btVector3 c = b + btVector3(0, 5, 0);
                btVector3 d = a + btVector3(0, 5, 0);
                mesh->addTriangle(a, reverse ? c : b, reverse ? b : c);
                mesh->addTriangle(a, reverse ? d : c, reverse ? c : d);
            }
        auto shape = std::make_unique<btBvhTriangleMeshShape>(mesh.get(), true);
        s.meshes.push_back(std::move(mesh));
        s.add(std::move(shape), btTransform::getIdentity());
    }
    for (int i = 0; i < 150; ++i)
    {
        s.tick();
        // A capsule's support radius along this plane normal.
        btScalar support = btScalar(0.35) + btScalar(0.55) * normal.z();
        require(s.position().dot(normal) >= support - btScalar(0.015),
                "fell through steep triangle mesh at " + std::to_string(double(angle)) + " degrees, frame " +
                    std::to_string(i) + ", x=" + std::to_string(double(s.position().x())) +
                    ", z=" + std::to_string(double(s.position().z())));
        require(!s.controller->onGround(), "steep mesh incorrectly counted as ground");
        s.noPenetration();
    }
    require(s.position().x() < -1, "gravity did not slide down steep mesh");
}

void walkableSlope()
{
    Scene s(btVector3(-2, 0, 2));
    s.floor();
    btScalar gradient = btTan(btRadians(btScalar(30)));
    s.quad(btVector3(0, -5, 0), btVector3(20, -5, 20 * gradient), btVector3(20, 5, 20 * gradient), btVector3(0, 5, 0));
    s.settle();
    s.controller->setWalkDirection(btVector3(btScalar(0.12), 0, 0));
    s.tick(90);
    require(s.position().x() > 8 && s.position().z() > 5, "could not ascend walkable slope");
    require(s.controller->onGround(), "lost support on walkable slope");
    s.controller->setWalkDirection(btVector3(-btScalar(0.12), 0, 0));
    s.tick(90);
    require(s.position().x() < -1 && s.position().z() < btScalar(1.05), "could not descend walkable slope");
    s.noPenetration();
}

void ridge()
{
    Scene s(btVector3(-2, 0, 2));
    s.floor();
    s.quad(btVector3(0, -5, 0), btVector3(2, -5, 5), btVector3(2, 5, 5), btVector3(0, 5, 0));
    s.quad(btVector3(2, -5, 5), btVector3(4, -5, 0), btVector3(4, 5, 0), btVector3(2, 5, 5));
    s.settle();
    s.controller->setWalkDirection(btVector3(btScalar(0.2), 0, 0));
    for (int i = 0; i < 200; ++i)
    {
        s.tick();
        require(s.position().x() < btScalar(0.25), "walked into/through steep ridge");
        require(s.position().z() < btScalar(1.35), "walk input climbed steep ridge");
        s.noPenetration();
    }
}

void stairs(bool lowCeiling)
{
    Scene s(btVector3(-2, 0, 1));
    s.floor();
    for (int i = 0; i < 5; ++i)
    {
        btScalar height = btScalar(0.2) * (i + 1);
        s.box(btVector3(btScalar(0.4) + btScalar(0.8) * i, 0, height / 2), btVector3(btScalar(0.4), 3, height / 2));
    }
    if (lowCeiling)
        s.quad(btVector3(-5, -5, btScalar(1.95)), btVector3(10, -5, btScalar(1.95)), btVector3(10, 5, btScalar(1.95)),
               btVector3(-5, 5, btScalar(1.95)));
    s.settle();
    s.controller->setWalkDirection(btVector3(btScalar(0.1), 0, 0));
    for (int i = 0; i < 100 && s.position().x() < btScalar(3.1); ++i)
    {
        s.tick();
        s.noPenetration();
        if (lowCeiling)
            require(s.position().z() + halfHeight < btScalar(1.965),
                    "stepped through low ceiling, frame " + std::to_string(i) + ", x=" +
                        std::to_string(double(s.position().x())) + ", z=" + std::to_string(double(s.position().z())));
    }
    if (lowCeiling)
        require(s.position().x() < btScalar(0.1), "climbed stairs without headroom");
    else
    {
        require(s.position().x() > btScalar(2.8) && s.position().z() > btScalar(1.65),
                "did not climb stairs, x=" + std::to_string(double(s.position().x())) +
                    ", z=" + std::to_string(double(s.position().z())));
        s.controller->setWalkDirection(btVector3(-btScalar(0.1), 0, 0));
        s.tick(80);
        require(s.position().x() < -1 && s.controller->onGround(), "did not descend stairs");
    }
}

void corner()
{
    Scene s(btVector3(-3, -3, 2));
    s.floor();
    s.quad(btVector3(0, -10, 0), btVector3(0, 10, 0), btVector3(0, 10, 5), btVector3(0, -10, 5));
    s.quad(btVector3(-10, 0, 0), btVector3(10, 0, 0), btVector3(10, 0, 5), btVector3(-10, 0, 5));
    s.settle();
    s.controller->setWalkDirection(btVector3(btScalar(0.3), btScalar(0.2), 0));
    s.tick(100);
    require(s.position().x() < -btScalar(0.34) && s.position().y() < -btScalar(0.34), "passed through corner");
    require(s.position().x() > -btScalar(0.42) && s.position().y() > -btScalar(0.42), "stopped before corner");
    s.noPenetration();
}

void fastWall(btScalar frameTime)
{
    Scene s(btVector3(-10, 0, 2));
    s.floor();
    s.quad(btVector3(0, -10, 0), btVector3(0, 10, 0), btVector3(0, 10, 10), btVector3(0, -10, 10));
    s.settle();
    s.controller->setWalkDirection(btVector3(100, 2, 0));
    s.tick(1, frameTime);
    require(s.position().x() < -btScalar(0.34), "tunnelled through thin wall");
    require(s.position().x() > -btScalar(0.42), "did not advance to wall hit fraction");
    require(s.position().y() > btScalar(1.95), "wall sliding lost tangential displacement");
    s.noPenetration();
}

void timedVelocityAndGravity()
{
    Scene s(btVector3(0, 0, 10));
    s.floor();
    s.controller->setVelocityForTimeInterval(btVector3(2, 0, 0), btScalar(0.1));
    s.tick(100);
    require(btFabs(s.position().x() - btScalar(0.2)) < btScalar(0.005), "timed velocity interval was not respected");
    require(s.controller->onGround(), "gravity stopped when timed input expired");
    s.controller->setVelocityForTimeInterval(btVector3(0, 0, 0), 1);
    s.controller->jump();
    s.tick();
    require(s.position().z() > btScalar(1.1), "zero timed input disabled jumping");
    s.tick(100);
    require(s.controller->onGround(), "zero timed input disabled gravity");
}

void filters()
{
    Scene s(btVector3(-2, 0, 2));
    s.floor();
    auto& trigger = s.box(btVector3(0, 0, 1), btVector3(btScalar(0.1), 4, 1));
    trigger.setCollisionFlags(trigger.getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
    s.box(btVector3(1, 0, 1), btVector3(btScalar(0.1), 4, 1), btBroadphaseProxy::SensorTrigger);
    s.box(btVector3(2, 0, 1), btVector3(btScalar(0.1), 4, 1), btBroadphaseProxy::StaticFilter,
          btBroadphaseProxy::DefaultFilter);
    auto& ignored = s.box(btVector3(3, 0, 1), btVector3(btScalar(0.1), 4, 1));
    s.ghost.setIgnoreCollisionCheck(&ignored, true);
    s.box(btVector3(4, 0, 2), btVector3(btScalar(0.1), 4, 2));
    s.settle();
    s.controller->setWalkDirection(btVector3(btScalar(0.1), 0, 0));
    s.tick(100);
    require(s.position().x() > btScalar(3.4) && s.position().x() < btScalar(3.56),
            "sweep filtering selected an ignored object or missed wall");
}

void penetrationRecovery()
{
    Scene s(btVector3(-btScalar(0.25), -btScalar(0.25), btScalar(0.8)));
    s.floor();
    s.box(btVector3(btScalar(0.1), -2, 2), btVector3(btScalar(0.1), 3, 2));
    s.box(btVector3(-2, btScalar(0.1), 2), btVector3(3, btScalar(0.1), 2));
    s.tick(10);
    s.noPenetration(btScalar(0.004));
    require(s.position().x() < -btScalar(0.34) && s.position().y() < -btScalar(0.34),
            "corner recovery chose wrong side");
    require(s.controller->onGround(), "recovery did not refresh ground");
}

void changedUp()
{
    Scene s(btVector3(0, 4, 0), btVector3(0, 1, 0));
    s.box(btVector3(0, -1, 0), btVector3(100, 1, 100));
    s.controller->setGravity(btVector3(0, -btScalar(29.4), 0));
    s.settle();
    require(btFabs(s.position().y() - halfHeight) < btScalar(0.04), "Y-up support used a hardcoded axis");
    s.controller->jump();
    s.tick();
    require(s.position().y() > btScalar(1.1), "Y-up jump used a hardcoded axis");
    s.tick(100);
    require(s.controller->onGround(), "Y-up landing failed");
    s.noPenetration();
}

void jumpHeightAndReset()
{
    Scene s(btVector3(0, 0, 2));
    s.floor();
    s.settle();
    btScalar start = s.position().z();
    s.controller->setMaxJumpHeight(btScalar(0.5));
    s.controller->jump(btVector3(0, 0, 20));
    btScalar apex = start;
    for (int i = 0; i < 100; ++i)
    {
        s.tick();
        apex = btMax(apex, s.position().z());
    }
    require(apex <= start + btScalar(0.505) && apex > start + btScalar(0.45), "max jump height ignored");
    s.controller->setAngularVelocity(btVector3(0, 0, 1));
    s.tick();
    btQuaternion rotation = s.ghost.getWorldTransform().getRotation();
    s.controller->reset(&s.world);
    s.controller->warp(btVector3(5, 0, 3));
    require(btFabs(rotation.dot(s.ghost.getWorldTransform().getRotation())) > btScalar(0.999),
            "warp reset orientation");
    require(!s.controller->canJump(), "warp retained stale ground state");
    s.tick(100);
    require(s.controller->onGround() && btFabs(s.position().x() - 5) < btScalar(0.01), "reset retained movement");
}

void noGhostCache()
{
    Scene s(btVector3(-5, 0, 2));
    s.broadphase.getOverlappingPairCache()->setInternalGhostPairCallback(nullptr);
    s.floor();
    s.quad(btVector3(0, -10, 0), btVector3(0, 10, 0), btVector3(0, 10, 5), btVector3(0, -10, 5));
    s.settle();
    s.controller->setWalkDirection(btVector3(20, 0, 0));
    s.tick();
    require(s.position().x() < -btScalar(0.34), "world sweep depended on ghost overlap cache");
}

void fastFall()
{
    Scene s(btVector3(0, 0, 100));
    s.floor();
    s.controller->setFallSpeed(10000);
    s.controller->setLinearVelocity(btVector3(0, 0, -1000));
    s.tick(1, btScalar(0.5));
    require(s.position().z() >= halfHeight - btScalar(0.01) && s.position().z() < 1,
            "high-speed fall passed through a thin floor");
    require(s.controller->onGround(), "high-speed landing did not establish support");
    s.noPenetration();
}

void transformedMesh(bool ceiling)
{
    btScalar angle = btRadians(ceiling ? btScalar(60) : btScalar(70));
    btTransform transform(btQuaternion(btVector3(0, 1, 0), angle), btVector3(3, 0, 5));
    btVector3 normal = transform.getBasis() * btVector3(0, 0, 1);
    btVector3 start = ceiling ? btVector3(3, 0, 1) : transform.getOrigin() + normal * 10;
    Scene s(start);
    auto& mesh = s.quad(btVector3(-300, -30, 0), btVector3(300, -30, 0), btVector3(300, 30, 0), btVector3(-300, 30, 0));
    mesh.setWorldTransform(transform);
    if (ceiling)
    {
        s.floor();
        s.settle();
        s.controller->jump(btVector3(0, 0, 25));
    }
    for (int i = 0; i < 100; ++i)
    {
        s.tick();
        btScalar distance = (s.position() - transform.getOrigin()).dot(normal);
        btScalar support = btScalar(0.35) + btScalar(0.55) * normal.z();
        require(ceiling ? distance <= -support + btScalar(0.015) : distance >= support - btScalar(0.015),
                "crossed rotated triangle mesh");
        s.noPenetration();
    }
}

void velocityModes()
{
    Scene s(btVector3(0, 0, 2));
    s.floor();
    s.settle();
    s.controller->setLinearVelocity(btVector3(2, 0, 3));
    s.tick(50);
    require(btFabs(s.position().x() - 4) < btScalar(0.01) && s.controller->onGround(),
            "persistent velocity did not use seconds or stopped gravity");
    s.controller->setLinearVelocity(btVector3(0, 0, 0));
    s.controller->setVelocityForTimeInterval(btVector3(10, 0, 0), 1);
    s.controller->setVelocityForTimeInterval(btVector3(2, 0, 0), btScalar(0.1));
    s.tick(25);
    require(btFabs(s.position().x() - btScalar(4.2)) < btScalar(0.01), "timed input accumulated old intervals");
    s.controller->setVelocityForTimeInterval(btVector3(100, 0, 0), -1);
    s.tick(25);
    require(btFabs(s.position().x() - btScalar(4.2)) < btScalar(0.01), "negative interval caused movement");
    s.controller->setWalkDirection(btVector3(0, 0, btScalar(0.2)));
    s.controller->setGravity(btVector3(0, 0, 0));
    btScalar height = s.position().z();
    s.tick();
    require(btFabs(s.position().z() - height - btScalar(0.2)) < btScalar(0.005),
            "explicit up displacement was discarded");
}
} // namespace

int main(int argc, char** argv)
{
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"flat ground and walk displacement", flatGround},
        {"triangle ceiling, front winding", [] { ceiling(false, true); }},
        {"triangle ceiling, reverse winding", [] { ceiling(true, true); }},
        {"triangle ceiling, world-sweep switch", [] { ceiling(true, false); }},
        {"60 degree slope", [] { fallingSteepMesh(60, false); }},
        {"70 degree slope, reverse winding", [] { fallingSteepMesh(70, true); }},
        {"85 degree slope", [] { fallingSteepMesh(85, false); }},
        {"walkable slope ascent and descent", walkableSlope},
        {"steep ridge blocks walking", ridge},
        {"stairs up and down", [] { stairs(false); }},
        {"stairs with insufficient headroom", [] { stairs(true); }},
        {"two triangle walls form a corner", corner},
        {"fast wall approach and sliding", [] { fastWall(dt); }},
        {"long frame wall approach", [] { fastWall(btScalar(0.5)); }},
        {"timed velocity and continued gravity", timedVelocityAndGravity},
        {"collision groups, triggers, ignored bodies", filters},
        {"penetration recovery in a corner", penetrationRecovery},
        {"Y-up gravity and jumping", changedUp},
        {"jump height, reset, warp orientation", jumpHeightAndReset},
        {"independent of ghost overlap cache", noGhostCache},
        {"high-speed fall onto thin mesh", fastFall},
        {"rotated steep triangle mesh", [] { transformedMesh(false); }},
        {"oblique triangle ceiling", [] { transformedMesh(true); }},
        {"persistent, replaced, negative, vertical input", velocityModes},
#ifdef BT_USE_DOUBLE_PRECISION
        {"oversized steep triangles in double precision", [] { fallingSteepMesh(70, false, true); }},
#endif
    };
    int failed = 0;
    int executed = 0;
    for (const auto& test : tests)
    {
        if (argc > 1 && test.first.find(argv[1]) == std::string::npos)
            continue;
        ++executed;
        try
        {
            test.second();
            std::cout << "PASS " << test.first << '\n';
        }
        catch (const std::exception& error)
        {
            ++failed;
            std::cerr << "FAIL " << test.first << ": " << error.what() << '\n';
        }
    }
    std::cout << executed - failed << '/' << executed << " passed\n";
    return failed ? 1 : 0;
}
