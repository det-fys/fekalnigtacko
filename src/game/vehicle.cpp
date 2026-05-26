#include "vehicle.hpp"

#include "assets/cache.hpp"
#include "net/utils.hpp"
#include "player.hpp"
#include "player_input.hpp"
#include "utils/random.hpp"

#include <iostream>

static std::shared_ptr<const assets::VehicleModel> LoadVehicleModelByName(const std::string& model_name)
{
    return assets::CacheManager::GetVehicleModel("data/" + model_name + ".veh");
}

game::Vehicle::Vehicle(World& world, const VehicleTuning& tuning)
    : Entity(world, net::ET_VEHICLE), tuning_(tuning), model_(LoadVehicleModelByName(tuning.model)),
      tuninglist_(VehicleTuningList::LoadFromFile("data/" + tuning.model + ".tun"))
{
    root_.local.position.z = 10.0f;

    wheels_.resize(model_->GetWheels().size());

    ApplyTuning(tuning);

    // init deform
    gfx::DeformGridInfo info{};
    info.min = glm::vec3(-1.0f, -2.5f, 0.10f);
    info.max = glm::vec3(1.0f, 2.0f, 1.8f);
    info.res = glm::ivec3(8, 16, 8);
    info.max_offset = 0.1f;
    deformgrid_ = std::make_unique<DeformGrid>(info);

    Update();
}

void game::Vehicle::Update()
{
    Super::Update();

    root_.UpdateMatrix();

    flags_ = 0;
    UpdateCrash();
    ProcessInput();
    UpdateWheels();
    UpdateLights();

    sync_current_ = 1 - sync_current_;
    UpdateSyncState();
    SendUpdateMsg();
}

void game::Vehicle::SendInitData(Player& player, net::OutMessage& msg) const
{
    Super::SendInitData(player, msg);

    msg.Write(net::ModelName(tuning_.model));
    WriteTuning(msg);

    // write state against default
    static const VehicleSyncState default_state;
    size_t fields_pos = msg.Reserve<VehicleSyncFieldFlags>();
    auto fields = WriteState(msg, default_state);
    msg.WriteAt(fields_pos, fields);

    WriteDeformSync(msg);
}

void game::Vehicle::OnContact(const collision::ContactInfo& info)
{
    Super::OnContact(info);

    crash_intensity_ += info.impulse;

    if (info.impulse < 1000.0f)
        return;

    if (window_health_ > 0.0f)
    {
        window_health_ -= info.impulse;

        if (window_health_ <= 0.0f) // just broken
        {
            PlaySound("breakwindow", 1.0f, 1.0f);
        }
    }

    if (window_health_ <= 0.0f)
    {
        Deform(info.pos, -glm::normalize(info.normal) * 0.1f, 1.0f);
    }
}

void game::Vehicle::SetInput(VehicleInputType type, bool enable)
{
    if (enable)
        in_ |= (1 << type);
    else
        in_ &= ~(1 << type);
}

void game::Vehicle::SetPosition(const glm::vec3& pos)
{
    auto& body = physics_->GetBtBody();

    auto t = body.getWorldTransform();
    t.setOrigin(btVector3(pos.x, pos.y, pos.z));
    body.setWorldTransform(t);
}

float game::Vehicle::GetSpeed() const
{
    return physics_->GetBtVehicle().getCurrentSpeedKmHour();
}

void game::Vehicle::SetSteering(bool analog, float value)
{
    steering_analog_ = analog;
    target_steering_ = value;
}

void game::Vehicle::SetTuning(const VehicleTuning& tuning)
{
    ApplyTuning(tuning);

    // send to clients
    auto msg = BeginEntMsg(net::EMSG_TUNING);
    WriteTuning(msg);
}

void game::Vehicle::ProcessInput()
{
    // TODO: totally fix

    // std::string nt = "";

    if (in_)
    {
        // nt += "in ";
        //  body_->setActivationState(ACTIVE_TAG);
        physics_->GetBtBody().activate();
    }
    else
    {
        // nt += "no in";
    }

    // nt += std::to_string(body_->getActivationState());
    // SetNametag(nt);

    float t_delta = 1.0f / 25.0f;

    // float steeringIncrement = .04 * 60;
    // float steeringClamp = .5;
    // float maxEngineForce = 7000;
    float maxEngineForce = tuning_ctx_.engine_force;
    float maxBreakingForce = tuning_ctx_.braking_force;

    float speed = GetSpeed();
    if (glm::abs(speed) > 200.0f)
        maxEngineForce = 100.0f;

    float engineForce = 0;
    float breakingForce = 0;

    float maxsc = .5f;
    float minsc = .08f;
    float sl = 130.f;

    float steeringClamp = std::max(minsc, (1.f - (std::abs(speed) / sl)) * maxsc);
    // steeringClamp = .5f;
    float steeringSpeed = steeringClamp * 5.0f;
    float steeringInc = steeringSpeed * t_delta;

    const bool in_forward = in_ & (1 << VIN_FORWARD);
    const bool in_backward = in_ & (1 << VIN_BACKWARD);
    const bool in_left = in_ & (1 << VIN_LEFT);
    const bool in_right = in_ & (1 << VIN_RIGHT);

    bool active_braking = false;
    bool active_gas = false;

    if (in_forward)
    {
        if (speed < -1)
        {
            breakingForce = maxBreakingForce;
            active_braking = true;
        }
        else
        {
            engineForce = maxEngineForce;
            active_gas = true;
        }
    }
    if (in_backward)
    {
        if (speed > 1)
        {
            breakingForce = maxBreakingForce;
            active_braking = true;
        }
        else
        {
            engineForce = -maxEngineForce / 2;
            active_gas = true;
        }
    }

    // idle breaking
    if (!active_braking && !active_gas)
    {
        breakingForce = 20.0f;
    }

    if (!steering_analog_)
    {
        if (in_left)
        {
            if (steering_ < steeringClamp)
                steering_ += steeringInc;
        }
        else
        {
            if (in_right)
            {
                if (steering_ > -steeringClamp)
                    steering_ -= steeringInc;
            }
            else
            {
                if (steering_ < -steeringInc)
                    steering_ += steeringInc;
                else
                {
                    if (steering_ > steeringInc)
                        steering_ -= steeringInc;
                    else
                    {
                        steering_ = 0.0f;
                    }
                }
            }
        }
    }
    else
    {
        if (steering_ < target_steering_)
        {
            steering_ += steeringInc;
            if (steering_ > target_steering_)
                steering_ = target_steering_;
        }
        else if (steering_ > target_steering_)
        {
            steering_ -= steeringInc;
            if (steering_ < target_steering_)
                steering_ = target_steering_;
        }

        if (steering_ > steeringClamp)
            steering_ = steeringClamp;
        else if (steering_ < -steeringClamp)
            steering_ = -steeringClamp;
    }

    auto& vehicle = physics_->GetBtVehicle();

    for (size_t i = 0; i < wheels_.size(); ++i)
    {
        float engine_factor = tuning_ctx_.wheels[i].engine_factor;
        if (engine_factor > 0.0001f)
        {
            vehicle.applyEngineForce(engine_factor * engineForce, i);
        }
        
        float braking_factor = tuning_ctx_.wheels[i].braking_factor;
        if (braking_factor > 0.0001f)
        {
            vehicle.setBrake(braking_factor * breakingForce, i);
        }

        float steering_factor = tuning_ctx_.wheels[i].steering_factor;
        if (std::abs(steering_factor) > 0.0001f)
        {
            vehicle.setSteeringValue(steering_factor * steering_, i);
        }
    }

    if (active_gas)
        flags_ |= VF_ACCELERATING;

    if (active_braking)
        flags_ |= VF_BRAKING;
        
    if (active_gas && engineForce < 0.0f)
        flags_ |= VF_REVERSING;

    const bool can_roll = wheels_on_ground_ <= (wheels_.size() / 2);

    if (can_roll)
        ++can_roll_frames_;
    else
        can_roll_frames_ = 0;


    const bool roll_left = in_left || (steering_analog_ && target_steering_ < -0.1f);
    const bool roll_right = in_right || (steering_analog_ && target_steering_ > 0.1f);

    // check if airborne and apply roll if right/left pressed
    if (can_roll_frames_ >= 50 && (roll_left || roll_right))
    {
        btVector3 ang_vel = physics_->GetBtBody().getAngularVelocity();

        const float max_vel = 1.0f;

        btTransform trans = physics_->GetBtBody().getWorldTransform();
        btQuaternion quat = trans.getRotation();
        glm::quat rot_quat(quat.getW(), quat.getX(), quat.getY(), quat.getZ());
        glm::vec3 local_up_world = rot_quat * glm::vec3(0.0f, 0.0f, 1.0f);
        float roll_factor = glm::clamp(1.0f - local_up_world.z, 0.0f, 1.0f);
        glm::vec3 local_roll(0.0f, 0.5f * roll_factor, 0.0f);
        glm::vec3 local_ang_vel = glm::inverse(rot_quat) * glm::vec3(ang_vel.x(), ang_vel.y(), ang_vel.z());

        if (glm::abs(local_ang_vel.y) < max_vel)
        {
            glm::vec3 world_roll = rot_quat * local_roll;
            glm::vec3 new_ang_vel = glm::vec3(ang_vel.x(), ang_vel.y(), ang_vel.z());

            if (roll_left)
                new_ang_vel -= world_roll;
            
                if (roll_right)
                new_ang_vel += world_roll;

            ang_vel = btVector3(new_ang_vel.x, new_ang_vel.y, new_ang_vel.z);
            physics_->GetBtBody().setAngularVelocity(ang_vel);
        }

    }
}

void game::Vehicle::UpdateCrash()
{
    if (window_health_ <= 0.0f)
        flags_ |= VF_BROKENWINDOWS;

    if (no_crash_frames_)
    {
        --no_crash_frames_;
    }
    else
    {
        if (crash_intensity_ > 1000.0f)
        {
            float volume = RandomFloat(0.9f, 1.2f);
            float pitch = RandomFloat(1.0f, 1.3f);

            if (crash_intensity_ > 12000.0f)
            {
                volume *= 1.7f;
                pitch *= 0.8f;
            }
            if (crash_intensity_ > 4000.0f)
            {
                volume *= 1.3f;
                pitch *= 0.8f;
            }
            else
            {
                volume *= 0.8f;
                pitch *= 1.2f;
            }

            PlaySound("crash", volume, pitch);
            no_crash_frames_ = 7 + rand() % 10;
        }
    }

    crash_intensity_ = 0.0f;
}

void game::Vehicle::UpdateWheels()
{
    auto& vehicle = physics_->GetBtVehicle();

    wheels_on_ground_ = 0;

    for (size_t i = 0; i < wheels_.size(); ++i)
    {
        auto& bt_wheel = vehicle.getWheelInfo(i);
        wheels_[i].speed = -(bt_wheel.m_rotation - wheels_[i].rotation) * 25.0f;
        wheels_[i].rotation = bt_wheel.m_rotation;
        wheels_[i].z_offset = tuning_ctx_.wheels[i].z_offset - bt_wheel.m_raycastInfo.m_suspensionLength;
        
        if (bt_wheel.m_raycastInfo.m_isInContact)
            ++wheels_on_ground_;
    }
}

void game::Vehicle::UpdateLights()
{
    if (lights_on_)
        flags_ |= VF_LIGHTS_ON;

    // TODO: orange lights
}

void game::Vehicle::UpdateSyncState()
{
    VehicleSyncState& state = sync_[sync_current_];

    state.flags = flags_;

    net::EncodePosition(root_.local.position, state.pos);
    net::EncodeRotation(root_.local.rotation, state.rot);

    state.steering.Encode(steering_);

    for (size_t i = 0; i < wheels_.size(); ++i)
    {
        auto& wheel = wheels_[i];
        state.wheels[i].z_offset.Encode(wheel.z_offset);
        state.wheels[i].speed.Encode(wheel.speed);
    }
}

game::VehicleSyncFieldFlags game::Vehicle::WriteState(net::OutMessage& msg, const VehicleSyncState& base) const
{
    VehicleSyncFieldFlags fields = 0;
    const VehicleSyncState& curr = sync_[sync_current_];

    if (curr.flags != base.flags)
    {
        fields |= VSF_FLAGS;
        msg.Write(curr.flags);
    }

    if (curr.pos.x.value != base.pos.x.value || curr.pos.y.value != base.pos.y.value ||
        curr.pos.z.value != base.pos.z.value)
    {
        fields |= VSF_POSITION;

        net::WriteDelta(msg, curr.pos.x, base.pos.x);
        net::WriteDelta(msg, curr.pos.y, base.pos.y);
        net::WriteDelta(msg, curr.pos.z, base.pos.z);
    }

    if (curr.rot.x.value != base.rot.x.value || curr.rot.y.value != base.rot.y.value ||
        curr.rot.z.value != base.rot.z.value)
    {
        fields |= VSF_ROTATION;

        net::WriteDelta(msg, curr.rot.x, base.rot.x);
        net::WriteDelta(msg, curr.rot.y, base.rot.y);
        net::WriteDelta(msg, curr.rot.z, base.rot.z);
    }

    if (curr.steering.value != base.steering.value)
    {
        fields |= VSF_STEERING;

        net::WriteDelta(msg, curr.steering, base.steering);
    }

    bool wheels_changed = false;
    for (size_t i = 0; i < wheels_.size(); ++i)
    {
        if (curr.wheels[i].z_offset.value != base.wheels[i].z_offset.value ||
            curr.wheels[i].speed.value != base.wheels[i].speed.value)
        {
            wheels_changed = true;
            break;
        }
    }

    if (wheels_changed)
    {
        fields |= VSF_WHEELS;

        for (size_t i = 0; i < wheels_.size(); ++i)
        {
            net::WriteDelta(msg, curr.wheels[i].z_offset, base.wheels[i].z_offset);
            net::WriteDelta(msg, curr.wheels[i].speed, base.wheels[i].speed);
        }
    }

    return fields;
}

void game::Vehicle::SendUpdateMsg()
{
    auto msg = BeginUpdateMsg();
    auto fields_pos = msg.Reserve<VehicleSyncFieldFlags>();
    auto fields = WriteState(msg, sync_[1 - sync_current_]);

    if (fields == 0)
    {
        DiscardUpdateMsg();
        return;
    }

    msg.WriteAt(fields_pos, fields);
}

void game::Vehicle::WriteDeformSync(net::OutMessage& msg) const
{
    const auto texels = deformgrid_->GetData();

    auto numtexels_pos = msg.Reserve<net::NumTexels>();
    net::NumTexels numtexels = 0;

    size_t last = 0;

    for (size_t i = 0; i < texels.size(); ++i)
    {
        if (texels[i] == glm::i8vec3(0))
            continue; // unchanged, no write

        auto diff = i - last;
        msg.WriteVarInt(diff);

        for (size_t j = 0; j < 3; ++j)
        {
            msg.Write(texels[i][j]);
        }

        last = i;
        ++numtexels;
    }

    msg.WriteAt(numtexels_pos, numtexels);
}

void game::Vehicle::Deform(const glm::vec3& pos, const glm::vec3& deform, float radius)
{
    net::PositionQ pos_q;
    net::PositionQ deform_q;
    net::EncodePosition(pos, pos_q);
    net::EncodePosition(deform, deform_q);

    SendDeformMsg(pos_q, deform_q);

    // defeorm locally
    glm::vec3 new_pos, new_deform;
    net::DecodePosition(pos_q, new_pos);
    net::DecodePosition(deform_q, new_deform);

    deformgrid_->ApplyImpulse(new_pos, new_deform, 0.3f);
}

void game::Vehicle::SendDeformMsg(const net::PositionQ& pos, const net::PositionQ& deform)
{
    auto msg = BeginEntMsg(net::EMSG_DEFORM);
    net::WritePositionQ(msg, pos);
    net::WritePositionQ(msg, deform);
}

void game::Vehicle::ApplyTuning(const VehicleTuning& tuning)
{
    tuning_ = tuning;

    tuning_ctx_ = VehicleTuningContext{};

    // setup wheels
    const auto& model_wheels = model_->GetWheels();
    tuning_ctx_.wheels.resize(model_wheels.size());
    for (size_t i = 0; i < model_wheels.size(); ++i)
    {
        tuning_ctx_.wheels[i].front = !(model_wheels[i].type & assets::WHEEL_REAR);
    }

    // apply tunning to ctx
    for (const auto& func : tuninglist_->default_funcs)
    {
        func(tuning_ctx_);
    }

    for (const auto& group : tuninglist_->groups)
    {
        auto group_it = tuning_.parts.find(group.id);
        if (group_it == tuning_.parts.end())
            continue;

        const auto& part_name = group_it->second;
        const auto& part = group.parts.at(part_name);

        for (const auto& func : part.funcs)
        {
            func(tuning_ctx_);
        }
    }

    // (re)create physics
    physics_.reset();
    physics_ = std::make_unique<VehiclePhysics>(world_, root_.local, *this, *model_, tuning_ctx_);
    OnPhysicsChanged();
}

void game::Vehicle::WriteTuning(net::OutMessage& msg) const
{
    // write colors
    for (const auto& color : tuning_ctx_.colors)
    {
        net::WriteRGB(msg, color);
    }

    // write wheel models
    for (const auto& wheel : tuning_ctx_.wheels)
    {
        msg.Write(net::ModelName(wheel.modelname));
    }
}

// PHYSICS

game::VehiclePhysics::VehiclePhysics(collision::DynamicsWorld& world, Transform& transform,
                                     collision::ObjectCallback& obj_cb, const assets::VehicleModel& model,
                                     const VehicleTuningContext& tuning)
    : world_(world), motion_(transform)
{
    
    // setup chassis rigidbody
    btCollisionShape* shape = model.GetModel()->GetColShape();
    if (!shape)
        throw std::runtime_error("Making vehicle with no shape");

    btVector3 local_inertia(0, 0, 0);
    shape->calculateLocalInertia(tuning.mass, local_inertia);

    btRigidBody::btRigidBodyConstructionInfo rb_info(tuning.mass, &motion_, shape, local_inertia);
    body_ = std::make_unique<btRigidBody>(rb_info);
    // body_->setActivationState(DISABLE_DEACTIVATION);

    collision::SetObjectInfo(body_.get(), collision::OT_ENTITY, collision::OF_NOTIFY_CONTACT | collision::OF_DESTRUCTING, &obj_cb);

    // setup vehicle
    btRaycastVehicle::btVehicleTuning bt_tuning;
    vehicle_ = std::make_unique<collision::RaycastVehicle>(bt_tuning, body_.get(), &world_.GetVehicleRaycaster());
    vehicle_->setCoordinateSystem(0, 2, 1);

    // setup wheels
    // btVector3 wheelDirectionCS0(0, -1, 0);
    // btVector3 wheelAxleCS(-1, 0, 0);
    btVector3 wheelDirectionCS0(0, 0, -1);
    btVector3 wheelAxleCS(1, 0, 0);

    const auto& model_wheels = model.GetWheels();

    size_t num_wheels = model_wheels.size();

    for (size_t i = 0; i < num_wheels; ++i)
    {
        const auto& wheel_mdl = model_wheels[i];
        const auto& wheel_tun = tuning.wheels[i];

        // float wheelRadius = wheel_tun.radius;

        // float friction = wheel_tun.friction
        // float suspensionStiffness = 50.0f;
        // // float suspensionDamping = 2.3f;
        // // float suspensionCompression = 4.4f;
        // float suspensionRestLength = 0.3f;
        // float rollInfluence = 0.3f;

        // float maxSuspensionForce = 90000.0f;
        // float maxSuspensionTravelCm = 15.0f;

        float k = 0.2f;
        
        // if (!is_front)
        //     friction *= 1.5f;

        btVector3 wheel_pos(wheel_mdl.position.x, wheel_mdl.position.y, wheel_mdl.position.z + wheel_tun.z_offset);

        auto& wi = vehicle_->addWheel(wheel_pos, wheelDirectionCS0, wheelAxleCS, wheel_tun.suspension_rest_length, wheel_tun.radius,
                                      bt_tuning, wheel_tun.front);

        wi.m_suspensionStiffness = wheel_tun.suspension_stiffness;

        wi.m_wheelsDampingCompression = k * 2.0 * btSqrt(wi.m_suspensionStiffness); // vehicleTuning.suspensionCompression;
        wi.m_wheelsDampingRelaxation = k * 3.3 * btSqrt(wi.m_suspensionStiffness);  // vehicleTuning.suspensionDamping;

        wi.m_frictionSlip = wheel_tun.friction;

        wi.m_rollInfluence = wheel_tun.roll_influence;
        wi.m_maxSuspensionForce = wheel_tun.suspension_max_force;
        wi.m_maxSuspensionTravelCm = wheel_tun.suspension_travel * 100.0f;
    }

    auto& bt_world = world_.GetBtWorld();
    bt_world.addRigidBody(body_.get(), btBroadphaseProxy::DefaultFilter, btBroadphaseProxy::AllFilter);
    bt_world.addAction(vehicle_.get());

}

game::VehiclePhysics::~VehiclePhysics()
{
    auto& bt_world = world_.GetBtWorld();
    bt_world.removeRigidBody(body_.get());
    bt_world.removeAction(vehicle_.get());
}
