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
        wheel.m_wheelsDampingRelaxation = damping_factor * 3.3 * btSqrt(wheel.m_suspensionStiffness) * 2.0;
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

    body_->setAngularFactor(btVector3(1.0f, 0.8f, 1.0f));
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

    auto& tc = engine_.torque_curve;
    tc[0] = {850.0f, 65.0f};
    tc[1] = {1500.0f, 115.0f};
    tc[2] = {2000.0f, 185.0f};
    tc[3] = {2500.0f, 210.0f};
    tc[4] = {3500.0f, 210.0f};
    tc[5] = {4500.0f, 205.0f};
    tc[6] = {5000.0f, 195.4f};
    tc[7] = {5500.0f, 165.0f};
    tc[8] = {6000.0f, 130.0f};
    tc[9] = {6500.0f, 0.0f};
    engine_.num_torque_curve_points = 10;

    engine_.idle_rpm = 800.0f;
    engine_.max_rpm = 6000.0f;

    engine_.inertia = 0.22f;

    //for (auto& point : tc)
    //{
    //    constexpr float torque_scale = 0.6f;
    //    point.torque *= torque_scale;
    //}

    auto& gears = transmission_.gear_ratios;
    gears[0] = -3.55f;
    gears[1] = 0.0f;
    gears[2] = 3.91f;
    gears[3] = 2.16f;
    gears[4] = 1.35f;
    gears[5] = 0.97f;
    gears[6] = 0.77f;
    transmission_.num_gears = 7;

    // auto& gears = transmission_.gear_ratios;
    //gears[0] = -3.2f;
    //gears[1] = 0.0f;
    //gears[2] = 3.6f;
    //gears[3] = 2.19f;
    //gears[4] = 1.41f;
    //gears[5] = 1.0f;
    //gears[6] = 0.83f;
    //gears[7] = 0.69f;
    //transmission_.num_gears = 8;

    transmission_.final_drive_ratio = 3.56f;
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

void VehiclePhysics::Shift(int dir)
{
    gear_ = glm::clamp(gear_ + dir, transmission_.GetMinGear(), transmission_.GetMaxGear());

    // release clutch when shifting
    clutch_engagement_ = 0.0f;
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
    
    float effective_throttle = throttle;
    if (clutch_engagement_ < 1.0f && engine_rpm_ > 3500.0f)
    {
        effective_throttle = 0.0f;
    }

    const float ratio = transmission_.GetRatio(gear_);

    // 1. Determine driven wheel contact fraction.
    float driven_wheels = 0.0f;
    float grounded_wheels = 0.0f;
    float total_engine_factor = 0.0f;

    for (size_t i = 0; i < wheel_tuning_.size(); ++i)
    {
        if (wheel_tuning_[i].engine_factor <= 0.0001f)
            continue;

        driven_wheels += 1.0f;
        
        total_engine_factor += wheel_tuning_[i].engine_factor;

        if (vehicle_->getWheelInfo(i).m_raycastInfo.m_isInContact)
        {
            grounded_wheels += 1.0f;
        }
    }

    float target_contact = driven_wheels > 0.0f ? grounded_wheels / driven_wheels : 0.0f;

    // 2. Smooth contact transitions.
    float contact_response = target_contact > wheel_contact_factor_ ? 8.0f : 12.0f;
    float contact_alpha = 1.0f - std::exp(-contact_response * dt);
    wheel_contact_factor_ = glm::mix(wheel_contact_factor_, target_contact, contact_alpha);

    // 3. Free-running engine simulation.
    engine_.rpm = engine_rpm_;
    engine_.UpdateFreeRunning(dt, effective_throttle);

    float free_rpm = engine_.rpm;

    // 4. Calculate transmission-coupled engine RPM.
    const float speed_to_rpm = 60.0f / (2.0f * glm::pi<float>()) / driven_wheel_radius_;
    const float wheel_rpm = current_speed_ * speed_to_rpm;
    const float coupled_rpm = std::abs(wheel_rpm * ratio);

    // Neutral has no clutch coupling.
    const float engagement = std::abs(ratio) > 0.0001f ? clutch_engagement_ * wheel_contact_factor_ : 0.0f;

    // 5. Synchronize the engine with the drivetrain.
    constexpr float coupling_strength = 30.0f;

    const float coupling_alpha = 1.0f - std::exp(-coupling_strength * engagement * dt);

    engine_rpm_ = glm::mix(free_rpm, std::max(engine_.idle_rpm, coupled_rpm), coupling_alpha);

    // 6. Calculate output torque.
    float engine_torque = engine_.GetTorqueAtRPM(engine_rpm_) * effective_throttle;

    // Simple rev limiter.
    if (engine_rpm_ >= engine_.max_rpm)
        engine_torque = 0.0f;

    float wheel_torque = engine_torque * ratio * engagement;
    float engine_force = wheel_torque / driven_wheel_radius_;

    // 7. Apply forces.
    for (size_t i = 0; i < wheel_tuning_.size(); ++i)
    {
        const auto& tuning = wheel_tuning_[i];
        auto& wheel = vehicle_->getWheelInfo(i);

        // Always write values, including zero.
        wheel.m_engineForce = tuning.engine_factor * engine_force / total_engine_factor;
        wheel.m_brake = tuning.braking_factor * braking_ * max_braking_force_;
        wheel.m_steering = tuning.steering_factor * steering_;
    }

    // shift
    bool throttle_active = throttle > 0.0f;
    bool change_gear = grounded_wheels > 0.01f && shift_cooldown_ == 0.0f &&
                       ((throttle_active && gear_ == 0) ||
                        (engine_rpm_ < 2200.0f && !(throttle_active && (reverse ? gear_ == -1 : gear_ == 1))) ||
                        (engine_rpm_ > 5200.0f &&
                         !(reverse ? gear_ == transmission_.GetMinGear() : gear_ == transmission_.GetMaxGear())));

    if (change_gear)
    {
        gear_ = 0;
        shift_cooldown_ = 0.25f;
        clutch_engagement_ = 0.0f;
    }

    MoveToward(shift_cooldown_, 0.0f, dt);

    if (shift_cooldown_ == 0.0f && gear_ == 0 && throttle > 0.0f)
    {
        // find best gear for current speed
        int best_gear = 0;

        float target_rpm = (1500.0f + 5000.0f) * 0.5f;
        float best_rpm_diff = std::numeric_limits<float>::max();

        auto range = reverse ? glm::ivec2(transmission_.GetMinGear(), -1) : glm::ivec2(1, transmission_.GetMaxGear());

        for (int i = range[0]; i <= range[1]; ++i)
        {
            float rpm = glm::abs(current_speed_ * speed_to_rpm * transmission_.GetRatio(i));
            float rpm_diff = std::abs(rpm - target_rpm);

            if (rpm_diff < best_rpm_diff)
            {
                best_rpm_diff = rpm_diff;
                best_gear = i;
            }
        }

        gear_ = best_gear;
        clutch_engagement_ = 0.0f;
    }

    // engage clutch
    if (gear_ != 0)
    {
        MoveToward(clutch_engagement_, 1.0f, dt * 3.0f);
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
