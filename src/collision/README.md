# Kinematic character controller

`KinematicCharacterController` replaces the movement API of Bullet's
`btKinematicCharacterController`. It lives alongside the other collision code in
`src/collision/`, and `game::CharacterPhysicsController` now uses it. The vendored
Bullet implementation is unchanged, so both controllers can be built together.
This is a source-level replacement, not an ABI-compatible subclass of the old
concrete type. Existing movement calls and `btDynamicsWorld::addAction` work with
the new type.

## Why the original controller fails

These observations refer to the vendored
[`btKinematicCharacterController.cpp`](../../external/bullet3/src/BulletDynamics/Character/btKinematicCharacterController.cpp):

* `stepDown` passes `m_up, m_maxSlopeCosine` to its closest-hit callback. The
  callback discards every steeper normal before choosing a hit. A steep triangle
  therefore cannot stop a fall. This confuses **not walkable** with **not solid**,
  and directly explains passing through ridges while ordinary flat ground works.
* `stepUp` assigns `m_currentPosition = m_targetPosition` before sweeping. Its
  callback accepts normals facing approximately **down**, but its hit handling
  only interpolates when the normal points **up**. Even that interpolation uses
  two positions which are already identical. Ceiling hits therefore do not clamp
  the move. Recovery starts from the penetrated destination and may already be
  on the opposite side of a thin mesh.
* The default ghost sweep examines objects in the ghost's current overlap list.
  New objects along a longer move can be absent. `world->convexSweepTest` queries
  the swept bounds instead. This matters for separate mesh objects and large
  displacements; it does not by itself explain every miss on one large terrain.
* The forward hit-fraction position update is commented out. The controller
  projects movement without first advancing to the actual contact. Its sliding
  loop also does not retain multiple blocking planes at corners.
* Penetration recovery tolerates **0.2 world units** by default and then adds a
  fraction of each contact correction. Recovery depends on contact count and
  cannot reliably undo crossing a zero-thickness mesh. The forward pass also
  temporarily modifies a potentially shared shape's margin.
* `onGround()` tests zero vertical velocity/offset rather than support. A stopped
  jump can consequently look grounded. Timed movement's early return also stops
  gravity when the interval expires or its input is zero. Maximum jump height is
  stored but not enforced, and a disabled fallback contains a hard-coded Y axis.

Vehicles use a different collision/movement path, so their success does not
validate these character-specific sweep and position updates.

## Replacement flow

1. Read the ghost transform. Recover existing overlap using fresh world contact
   queries, then probe for ground. Ghost overlap manifolds are not required.
2. Subdivide integration into approximately 1/60-second segments, with a maximum
   of 128 segments for a hitch. Divide a walk displacement across those segments
   so `setWalkDirection` retains its per-update contract.
3. Sweep requested movement against **all** blocking normals. Advance to the
   hit fraction minus a 3 mm skin, then clip the remainder against accumulated
   contact planes. Stop conservatively if the iteration budget is exhausted.
4. For input movement, steep slopes also supply a horizontal blocking plane,
   preventing uphill wall climbing. Gravity uses the actual surface plane and
   can slide downhill. A walkable landing stops gravity-induced sliding.
5. Attempt a stair route only after blocked movement from ground or a previously
   verified stair edge. Sweep up, forward, and down, and select the route only if
   it improves forward progress. A rounded capsule edge additionally needs a
   swept check of a walkable tread ahead. Every actual movement still stops at
   its first blocking hit, including ceilings.
6. Snap down a step only from previous support. A separate short support query
   handles floor/wall junctions without moving through an intervening steep hit.
7. Integrate gravity regardless of input mode. Upward motion stops at ceilings;
   only walkable support enables `canJump()`.

Contact recovery solves the contact constraints together instead of adding the
same correction for every adjacent triangle. The default penetration tolerance
is 1 mm. Shape margins remain unchanged. Sweeps use zero allowed CCD penetration.

## API details

* The ghost and convex shape remain caller-owned and must outlive the controller.
  The ghost must use that shape and be registered in the collision world before
  updating. The game already handles this lifetime and registration.
* The supplied shape/transform must already have the requested up orientation.
  Construction leaves the transform alone. Subsequent `setUp`/nonzero
  `setGravity` calls rotate it from the previous up vector. Pass `(0,0,1)` for the
  game's `btCapsuleShapeZ`.
* `setWalkDirection` takes displacement per physics action, including when the
  outer world uses several fixed substeps. The game supplies its 25 Hz increment.
* `setVelocityForTimeInterval` takes units/second and **replaces** the previous
  interval. Negative intervals produce no commanded motion; gravity continues.
* `setLinearVelocity` sets persistent planar velocity plus initial signed
  vertical velocity. `getLinearVelocity` reports commanded velocity plus the
  current gravity/jump velocity, not velocity inferred from collision-limited
  displacement.
* `jump(v)`/`applyImpulse(v)` use the supplied signed world velocity, including
  lateral drift; zero uses the configured jump speed along up. They can force a
  jump while airborne, as in Bullet. Call `canJump()` to gate ordinary jumps.
  `setMaxJumpHeight(0)` means unlimited height.
* `onGround` is initially false until an update establishes support. `reset`
  clears motion; `warp` preserves rotation and velocity and invalidates support.
  Call both if teleporting should also cancel motion. World bounds refresh on
  the next `preStep`.
* `setMaxPenetrationDepth` controls recovery tolerance, not permission for sweeps
  to cross geometry. Its smaller default intentionally changes old behavior.
* `setUseGhostSweepTest` and `setUpInterpolate` remain callable compatibility
  switches. All movement uses world sweeps and respects hit fractions.
* Collision groups/masks, no-response triggers, ignored collision objects, and
  the protected `needsCollision` override are applied before choosing hits and
  during recovery.

The game now maps its foot position to the capsule bottom directly. The old
5 cm offset would otherwise leave the rendered feet above correctly resolved
ground contacts.

## Regression tests

Build without the renderer, Dawn, or other game dependencies from a compiler
developer shell:

```powershell
cmake -S src/collision/tests -B build/controller-tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DUSE_DOUBLE_PRECISION=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build/controller-tests --target character_controller_tests
ctest --test-dir build/controller-tests --output-on-failure
```

The policy setting accommodates the vendored Bullet CMake files with CMake 4.
Alternatively enable `BUILD_COLLISION_TESTS` in the main project. To build the
same scenes against Bullet's original controller, configure with
`-DBUILD_BASELINE_COLLISION_TESTS=ON`, build `character_controller_baseline`, and
run it directly. Its failures are expected; it is not registered as a CTest.
Pass a test-name substring to either executable to run selected scenes.

The suite runs through `btDiscreteDynamicsWorld::stepSimulation` and its action
callback. It covers flat-ground stability, walk displacement, both triangle
windings, flat and oblique ceilings, 60/70/85-degree slopes, triangulated slope
seams, walking up/down a 30-degree ramp, steep ridges, stairs and insufficient
headroom, corners, high-speed thin-wall/floor impacts, long frames, all movement
input modes, filtering, shallow overlap recovery, rotated meshes, Y-up gravity,
jump-height limits, reset/warp, and operation without a ghost pair cache.

## Practical limits

* The game uses double-precision Bullet. The tests also exercise single
  precision with reasonably sized triangles. Extremely large, thin triangles
  exposed Bullet GJK inaccuracies in single precision even with this controller;
  the oversized-triangle regression therefore runs in double precision. Keep
  double precision for the existing terrain, or tessellate oversized triangles
  before switching to single precision.
* Gravity and stair assistance are kinematic. This does not add dynamic-body
  pushing, moving-platform attachment, or rigid-body momentum conservation.
* A spawn or teleport already deeply inside a thin surface has no unique
  recoverable side. Recovery is bounded; it cannot reconstruct the path before
  that overlap or resolve a capsule squeezed into a space smaller than itself.
* Fast objects moving through a stationary character between physics updates
  still require suitable world timesteps/CCD. Character sweeps query the world's
  current transforms, not future obstacle motion.
* Angular movement uses Bullet's convex cast and bounded transform integration;
  the regression coverage focuses on upright capsules. General rotating convex
  shapes and unusual scales need their own scenes and tolerance tuning.
* Validation is headless. Client and server translation units compile with the
  replacement; a full game build and an in-game terrain playtest remain to be
  performed. The full CMake build in this session was blocked by sandbox access
  to the Python interpreter required by the existing Dawn configuration.
