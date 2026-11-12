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
    steering_speed_ = tuning.steering;

    wheel_tuning_.clear();
    wheel_tuning_.reserve(tuning.wheels.size());
    
    for (const auto& wheel : tuning.wheels)
    {
        wheel_tuning_.push_back({wheel.engine_factor, wheel.braking_factor, wheel.steering_factor, wheel.z_offset});
    }

    max_braking_force_ = tuning.braking_force;

    float torque_scale = 0.6f;

    auto& tc = engine_.torque_curve;
    tc[0] = {  800.f, 110.f * torque_scale };
    tc[1] = { 1500.f, 160.f * torque_scale };
    tc[2] = { 2500.f, 215.f * torque_scale };
    tc[3] = { 3500.f, 245.f * torque_scale };
    tc[4] = { 4500.f, 260.f * torque_scale };
    tc[5] = { 5500.f, 250.f * torque_scale };
    tc[6] = { 6500.f, 225.f * torque_scale };
    tc[7] = { 7000.f, 190.f };
    engine_.num_torque_curve_points = 9;

    auto& gears = transmission_.gear_ratios;
    gears[0] = -3.2f;
    gears[1] = 0.0f;
    gears[2] = 3.6f;
    gears[3] = 2.19f;
    gears[4] = 1.41f;
    gears[5] = 1.0f;
    gears[6] = 0.83f;
    gears[7] = 0.69f;
    transmission_.num_gears = 8;

    transmission_.final_drive_ratio = 3.6f;


}

void VehiclePhysics::Update(float dt)
{
    UpdateBulletHitboxTransform();
    UpdateInput(dt);
    UpdateDrivetrain(dt);
    UpdateWheels(dt);
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

void VehiclePhysics::UpdateInput(float dt)
{
    accelerating_ = false;
    braking_ = false;
    reversing_ = false;

    if (!action_enabled_)
        return;

    if (input_.any_input)
        body_->activate();

    constexpr float t_delta = 1.0f / 25.0f;
    // float max_engine_force = engine_force_;
    // const float max_braking_force = braking_force_;
    const float speed = vehicle_->getCurrentSpeedKmHour();
    current_speed_ = speed * (1000.0f / 3600.0f); // [m/s]
    // if (glm::abs(speed) > 200.0f)
        // max_engine_force = 100.0f;

    // float engine_force = 0.0f;
    // float braking_force = 0.0f;

    constexpr float max_steering_clamp = 0.5f;
    constexpr float min_steering_clamp = 0.08f;
    constexpr float steering_limit_speed = 130.0f;
    const float steering_clamp = std::max(min_steering_clamp,
        (1.0f - (std::abs(speed) / steering_limit_speed)) * max_steering_clamp);
    float steering_speed = steering_clamp * steering_speed_;
    if (input_.analog_steering)
        steering_speed *= 3.0f;
    const float steering_increment = steering_speed * t_delta;
    const float steering_decrement = steering_increment * 2.0f;

    // bool active_braking = false;
    // bool active_gas = false;

    float target_throttle = 0.0f;
    float target_brake = 0.0f;

    if (input_.forward)
    {
        if (speed < -1.0f)
            target_brake = 1.0f;
        else
            target_throttle = 1.0f;
    }
    if (input_.backward)
    {
        if (speed > 1.0f)
            target_brake = 1.0f;
        else
            target_throttle = -1.0f;
    }

    const bool active_braking = target_brake > 0.0f;

    if (target_throttle == 0.0f && target_brake == 0.0f)
        target_brake = 0.3f;

    if (!input_.analog_steering)
    {
        if (input_.left)
        {
            if (steering_ < steering_clamp)
                steering_ += steering_increment;
        }
        else if (input_.right)
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
        const float target_steering = glm::clamp(input_.target_steering, -steering_clamp, steering_clamp);
        MoveToward(steering_, target_steering,
                   glm::abs(target_steering) < glm::abs(steering_) ? steering_increment : steering_decrement);
    }

    accelerating_ = target_throttle != 0.0f;
    braking_ = active_braking;
    reversing_ = target_throttle < 0.0f;

    throttle_and_dir_ = target_throttle;
    brake_ = target_brake;

    const bool can_roll = wheels_on_ground_ <= (wheel_tuning_.size() / 2);
    if (can_roll)
        ++can_roll_frames_;
    else
        can_roll_frames_ = 0;

    const bool roll_left = input_.left || (input_.analog_steering && input_.target_steering < -0.1f);
    const bool roll_right = input_.right || (input_.analog_steering && input_.target_steering > 0.1f);
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

void VehiclePhysics::UpdateDrivetrain(float dt)
{
    const bool reverse = throttle_and_dir_ < 0.0f;
    const float throttle = glm::abs(throttle_and_dir_);

    const auto speed_ms = current_speed_;

    // calculate current rpm
    const auto wheel_rpm = speed_ms / driven_wheel_radius_ * 60.0f / (2.0f * glm::pi<float>());
    const auto transmission_ratio = transmission_.GetRatio(gear_);
    const auto target_engine_rpm = wheel_rpm * transmission_ratio;

    engine_rpm_ = glm::max(800.0f, target_engine_rpm);

    const auto engine_torque = engine_.GetTorqueAtRPM(engine_rpm_);
    const auto engine_force = engine_torque * transmission_ratio / driven_wheel_radius_ * throttle;

    // apply forces

    for (size_t i = 0; i < wheel_tuning_.size(); ++i)
    {
        const auto& wheel_tuning = wheel_tuning_[i];
        auto& wheel = vehicle_->getWheelInfo(i);
        
        if (wheel_tuning.engine_factor > 0.0001f)
            wheel.m_engineForce = wheel_tuning.engine_factor * engine_force;
        
        if (wheel_tuning.braking_factor > 0.0001f)
            wheel.m_brake = wheel_tuning.braking_factor * braking_ * max_braking_force_;
        
        if (std::abs(wheel_tuning.steering_factor) > 0.0001f)
            wheel.m_steering = wheel_tuning.steering_factor * steering_;
    }


}

void VehiclePhysics::UpdateWheels(float dt)
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

        wheel.speed = -(static_cast<float>(bullet_wheel.m_rotation) - wheel.rotation) / dt;
        bullet_wheel.m_rotation = glm::mod(bullet_wheel.m_rotation, glm::two_pi<btScalar>());
        wheel.rotation = bullet_wheel.m_rotation;
        wheel.z_offset = wheel_tuning_[i].z_offset - bullet_wheel.m_raycastInfo.m_suspensionLength;
        wheel.touching_ground = bullet_wheel.m_raycastInfo.m_isInContact;
        
        if (wheel.touching_ground)
            ++wheels_on_ground_;
    }
}

} // namespace game
