#include "vehicle_sim.hpp"

#include <stdexcept>
#include <algorithm>
#include <cmath>

#include "utils/math.hpp"

namespace game
{

VehiclePhysics::VehiclePhysics(collision::DynamicsWorld& world, FarTransform& transform,
                               const assets::VehicleModel& model, const VehicleTuningContext& tuning,
                               collision::ObjectCallback* obj_cb)
    : world_(world), motion_(transform)
{
    SetTuning(tuning);

    btCollisionShape* shape = model.GetModel()->GetColShape();
    if (!shape)
        throw std::runtime_error("Making vehicle with no shape");

    btVector3 local_inertia(0, 0, 0);
    shape->calculateLocalInertia(tuning.mass, local_inertia);

    btRigidBody::btRigidBodyConstructionInfo rb_info(tuning.mass, &motion_, shape, local_inertia);
    body_ = std::make_unique<btRigidBody>(rb_info);
    collision::SetObjectInfo(body_.get(), collision::OT_ENTITY,
                             collision::OF_NOTIFY_CONTACT | collision::OF_DESTRUCTING | collision::OF_EXPLOSION_DAMAGE,
                             obj_cb);

    btRaycastVehicle::btVehicleTuning bt_tuning;
    vehicle_ = std::make_unique<collision::RaycastVehicle>(bt_tuning, body_.get(), &world_.GetVehicleRaycaster());
    vehicle_->setCoordinateSystem(0, 2, 1);

    btVector3 wheel_direction(0, 0, -1);
    btVector3 wheel_axle(1, 0, 0);
    const auto& model_wheels = model.GetWheels();
    wheel_states_.resize(model_wheels.size());
    for (size_t i = 0; i < model_wheels.size(); ++i)
    {
        const auto& wheel_model = model_wheels[i];
        const auto& wheel_tuning = tuning.wheels[i];
        btVector3 wheel_pos(wheel_model.position.x, wheel_model.position.y,
                            wheel_model.position.z + wheel_tuning.z_offset);
        auto& wheel = vehicle_->addWheel(wheel_pos, wheel_direction, wheel_axle, wheel_tuning.suspension_rest_length,
                                         wheel_tuning.radius, bt_tuning, wheel_tuning.front);
        constexpr float damping_factor = 0.2f;
        wheel.m_suspensionStiffness = wheel_tuning.suspension_stiffness;
        wheel.m_wheelsDampingCompression = damping_factor * 2.0 * btSqrt(wheel.m_suspensionStiffness);
        wheel.m_wheelsDampingRelaxation = damping_factor * 3.3 * btSqrt(wheel.m_suspensionStiffness);
        wheel.m_frictionSlip = wheel_tuning.friction;
        wheel.m_rollInfluence = wheel_tuning.roll_influence;
        wheel.m_maxSuspensionForce = wheel_tuning.suspension_max_force;
        wheel.m_maxSuspensionTravelCm = wheel_tuning.suspension_travel * 100.0f;
    }

    auto& bt_world = world_.GetBtWorld();
    bt_world.addRigidBody(body_.get(), collision::OG_DEFAULT, ~collision::OG_PROJECTILE);
    bt_world.addAction(vehicle_.get());
    action_enabled_ = true;

    auto col_mesh = model.GetModel()->GetColMesh();
    if (col_mesh)
    {
        bullet_hitbox_ = std::make_unique<btCollisionObject>();
        bullet_hitbox_->setCollisionShape(col_mesh->GetShape());
        collision::SetObjectInfo(bullet_hitbox_.get(), collision::OT_ENTITY, 0, obj_cb);
        bt_world.addCollisionObject(bullet_hitbox_.get(), collision::OG_DEFAULT, collision::OG_PROJECTILE);
        UpdateBulletHitboxTransform();
    }
}

void VehiclePhysics::SetTuning(const VehicleTuningContext& tuning)
{
    engine_force_ = tuning.engine_force;
    braking_force_ = tuning.braking_force;
    steering_speed_ = tuning.steering;

    wheel_tuning_.clear();
    wheel_tuning_.reserve(tuning.wheels.size());
    
    for (const auto& wheel : tuning.wheels)
    {
        wheel_tuning_.push_back({wheel.engine_factor, wheel.braking_factor, wheel.steering_factor, wheel.z_offset});
    }
}

void VehiclePhysics::Update()
{
    UpdateBulletHitboxTransform();
    UpdateWheels();
}

void VehiclePhysics::ApplyImpulse(const glm::vec3& impulse, const glm::dvec3& world_position)
{
    body_->activate();
    const btVector3 bullet_impulse(impulse.x, impulse.y, impulse.z);
    const btVector3 relative_position(world_position.x, world_position.y, world_position.z);
    body_->applyImpulse(bullet_impulse, relative_position - body_->getCenterOfMassPosition());
}

void VehiclePhysics::SetPosition(const glm::dvec3& position)
{
    auto transform = body_->getWorldTransform();
    transform.setOrigin(btVector3(position.x, position.y, position.z));
    body_->setWorldTransform(transform);
}

glm::vec3 VehiclePhysics::GetVelocity() const
{
    const auto velocity = body_->getLinearVelocity();
    return glm::vec3(velocity.x(), velocity.y(), velocity.z());
}

void VehiclePhysics::ProcessInput(const VehicleSimulationInput& input)
{
    accelerating_ = false;
    braking_ = false;
    reversing_ = false;

    if (!action_enabled_)
        return;

    if (input.any_input)
        body_->activate();

    constexpr float t_delta = 1.0f / 25.0f;
    float max_engine_force = engine_force_;
    const float max_braking_force = braking_force_;
    const float speed = vehicle_->getCurrentSpeedKmHour();
    if (glm::abs(speed) > 200.0f)
        max_engine_force = 100.0f;

    float engine_force = 0.0f;
    float braking_force = 0.0f;

    constexpr float max_steering_clamp = 0.5f;
    constexpr float min_steering_clamp = 0.08f;
    constexpr float steering_limit_speed = 130.0f;
    const float steering_clamp = std::max(min_steering_clamp,
        (1.0f - (std::abs(speed) / steering_limit_speed)) * max_steering_clamp);
    float steering_speed = steering_clamp * steering_speed_;
    if (input.analog_steering)
        steering_speed *= 3.0f;
    const float steering_increment = steering_speed * t_delta;
    const float steering_decrement = steering_increment * 2.0f;

    bool active_braking = false;
    bool active_gas = false;
    if (input.forward)
    {
        if (speed < -1.0f)
        {
            braking_force = max_braking_force;
            active_braking = true;
        }
        else
        {
            engine_force = max_engine_force;
            active_gas = true;
        }
    }
    if (input.backward)
    {
        if (speed > 1.0f)
        {
            braking_force = max_braking_force;
            active_braking = true;
        }
        else
        {
            engine_force = -max_engine_force / 2.0f;
            active_gas = true;
        }
    }

    if (!active_braking && !active_gas)
        braking_force = 20.0f;

    if (!input.analog_steering)
    {
        if (input.left)
        {
            if (steering_ < steering_clamp)
                steering_ += steering_increment;
        }
        else if (input.right)
        {
            if (steering_ > -steering_clamp)
                steering_ -= steering_increment;
        }
        else if (steering_ < -steering_increment)
        {
            steering_ += steering_increment;
        }
        else if (steering_ > steering_increment)
        {
            steering_ -= steering_increment;
        }
        else
        {
            steering_ = 0.0f;
        }
    }
    else
    {
        const float target_steering = glm::clamp(input.target_steering, -steering_clamp, steering_clamp);
        MoveToward(steering_, target_steering,
                   glm::abs(target_steering) < glm::abs(steering_) ? steering_increment : steering_decrement);
    }

    for (size_t i = 0; i < wheel_tuning_.size(); ++i)
    {
        const auto& wheel_tuning = wheel_tuning_[i];
        if (wheel_tuning.engine_factor > 0.0001f)
            vehicle_->applyEngineForce(wheel_tuning.engine_factor * engine_force, i);
        if (wheel_tuning.braking_factor > 0.0001f)
            vehicle_->setBrake(wheel_tuning.braking_factor * braking_force, i);
        if (std::abs(wheel_tuning.steering_factor) > 0.0001f)
            vehicle_->setSteeringValue(wheel_tuning.steering_factor * steering_, i);
    }

    accelerating_ = active_gas;
    braking_ = active_braking;
    reversing_ = active_gas && engine_force < 0.0f;

    const bool can_roll = wheels_on_ground_ <= (wheel_tuning_.size() / 2);
    if (can_roll)
        ++can_roll_frames_;
    else
        can_roll_frames_ = 0;

    const bool roll_left = input.left || (input.analog_steering && input.target_steering < -0.1f);
    const bool roll_right = input.right || (input.analog_steering && input.target_steering > 0.1f);
    if (can_roll_frames_ >= 50 && (roll_left || roll_right))
    {
        btVector3 angular_velocity = body_->getAngularVelocity();
        constexpr float max_velocity = 1.0f;
        const btQuaternion quat = body_->getWorldTransform().getRotation();
        const glm::quat rotation(quat.getW(), quat.getX(), quat.getY(), quat.getZ());
        const glm::vec3 local_up_world = rotation * glm::vec3(0.0f, 0.0f, 1.0f);
        const float roll_factor = glm::clamp(1.0f - local_up_world.z, 0.0f, 1.0f);
        const glm::vec3 local_roll(0.0f, 0.5f * roll_factor, 0.0f);
        const glm::vec3 local_angular_velocity = glm::inverse(rotation) *
            glm::vec3(angular_velocity.x(), angular_velocity.y(), angular_velocity.z());
        if (glm::abs(local_angular_velocity.y) < max_velocity)
        {
            const glm::vec3 world_roll = rotation * local_roll;
            glm::vec3 new_angular_velocity(angular_velocity.x(), angular_velocity.y(), angular_velocity.z());
            if (roll_left)
                new_angular_velocity -= world_roll;
            if (roll_right)
                new_angular_velocity += world_roll;
            body_->setAngularVelocity(btVector3(new_angular_velocity.x, new_angular_velocity.y,
                                                new_angular_velocity.z));
        }
    }
}

void VehiclePhysics::DisableAction()
{
    if (!action_enabled_)
        return;
    world_.GetBtWorld().removeAction(vehicle_.get());
    action_enabled_ = false;
}

VehiclePhysics::~VehiclePhysics()
{
    auto& bt_world = world_.GetBtWorld();
    bt_world.removeRigidBody(body_.get());
    DisableAction();
    if (bullet_hitbox_)
        bt_world.removeCollisionObject(bullet_hitbox_.get());
}

void VehiclePhysics::UpdateBulletHitboxTransform()
{
    if (bullet_hitbox_)
        bullet_hitbox_->setWorldTransform(body_->getWorldTransform());
}

void VehiclePhysics::UpdateWheels()
{
    wheels_on_ground_ = 0;
    
    if (!action_enabled_)
    {
        for (auto& wheel : wheel_states_)
            wheel.touching_ground = false;
        return;
    }

    for (size_t i = 0; i < wheel_states_.size(); ++i)
    {
        auto& bullet_wheel = vehicle_->getWheelInfo(i);
        auto& wheel = wheel_states_[i];

        wheel.speed = -(static_cast<float>(bullet_wheel.m_rotation) - wheel.rotation) * 25.0f;
        bullet_wheel.m_rotation = glm::mod(bullet_wheel.m_rotation, glm::two_pi<btScalar>());
        wheel.rotation = bullet_wheel.m_rotation;
        wheel.z_offset = wheel_tuning_[i].z_offset - bullet_wheel.m_raycastInfo.m_suspensionLength;
        wheel.touching_ground = bullet_wheel.m_raycastInfo.m_isInContact;
        
        if (wheel.touching_ground)
            ++wheels_on_ground_;
    }
}


} // namespace game
