#include "vehicle.hpp"

#include "assets/cache.hpp"
#include "net/utils.hpp"
#include "player.hpp"
#include "player_input.hpp"

#include <iostream>

static std::shared_ptr<const assets::VehicleModel> LoadVehicleModelByName(const std::string& model_name)
{
    return assets::CacheManager::GetVehicleModel("data/" + model_name + ".veh");
}

game::Vehicle::Vehicle(World& world, std::string model_name, const glm::vec3& color)
    : Entity(world, net::ET_VEHICLE), model_name_(model_name), model_(LoadVehicleModelByName(model_name)),
      motion_(root_.local),
      color_(color)
{
    root_.local.position.z = 10.0f;

    // setup chassis rigidbody
    float mass = 1300.0f;

    btCollisionShape* shape = model_->GetModel()->GetColShape();
    if (!shape)
        throw std::runtime_error("Making vehicle with no shape");

    btVector3 local_inertia(0, 0, 0);
    shape->calculateLocalInertia(mass, local_inertia);

    btRigidBody::btRigidBodyConstructionInfo rb_info(mass, &motion_, shape, local_inertia);
    body_ = std::make_unique<btRigidBody>(rb_info);
    body_->setActivationState(DISABLE_DEACTIVATION);

    // setup vehicle
    btRaycastVehicle::btVehicleTuning tuning;
    vehicle_ = std::make_unique<btRaycastVehicle>(tuning, body_.get(), &world_.GetVehicleRaycaster());
    vehicle_->setCoordinateSystem(0, 2, 1);

    // setup wheels
    // btVector3 wheelDirectionCS0(0, -1, 0);
    // btVector3 wheelAxleCS(-1, 0, 0);
    btVector3 wheelDirectionCS0(0, 0, -1);
    btVector3 wheelAxleCS(1, 0, 0);

    wheel_z_offset_ = 0.5f;

    const auto& wheels = model_->GetWheels();

    if (wheels.size() > MAX_WHEELS)
        throw std::runtime_error("Max wheels exceeded");

    num_wheels_ = wheels.size();

    for (const auto& wheeldef : wheels)
    {
        float wheelRadius = wheeldef.radius;

        float friction = 3.0f; // 5.0f;
        float suspensionStiffness = 50.0f;
        // float suspensionDamping = 2.3f;
        // float suspensionCompression = 4.4f;
        float suspensionRestLength = 0.6f;
        float rollInfluence = 0.01f;

        float maxSuspensionForce = 100000.0f;
        float maxSuspensionTravelCm = 5000.0f;

        float k = 0.2;

        const bool is_front = !(wheeldef.type & assets::WHEEL_REAR);

        btVector3 wheel_pos(wheeldef.position.x, wheeldef.position.y, wheeldef.position.z + wheel_z_offset_);
        auto& wi = vehicle_->addWheel(wheel_pos, wheelDirectionCS0, wheelAxleCS, suspensionRestLength, wheelRadius,
                                      tuning, is_front);

        wi.m_suspensionStiffness = suspensionStiffness;

        wi.m_wheelsDampingCompression = k * 2.0 * btSqrt(suspensionStiffness); // vehicleTuning.suspensionCompression;
        wi.m_wheelsDampingRelaxation = k * 3.3 * btSqrt(suspensionStiffness);  // vehicleTuning.suspensionDamping;

        wi.m_frictionSlip = friction;
        // if (wi.m_bIsFrontWheel) wi.m_frictionSlip = vehicleTuning.friction * 1.4f;

        wi.m_rollInfluence = rollInfluence;
        wi.m_maxSuspensionForce = maxSuspensionForce;
        wi.m_maxSuspensionTravelCm = maxSuspensionTravelCm;
    }

    auto& bt_world = world_.GetBtWorld();
    bt_world.addRigidBody(body_.get());
    bt_world.addAction(vehicle_.get());
}

void game::Vehicle::Update()
{
    Super::Update();

    flags_ = 0;
    ProcessInput();
    UpdateWheels();

    sync_current_ = 1 - sync_current_;
    UpdateSyncState();
    SendUpdateMsg();
}

void game::Vehicle::SendInitData(Player& player, net::OutMessage& msg) const
{
    Super::SendInitData(player, msg);

    net::ModelName name(model_name_);
    msg.Write(name);
    net::WriteRGB(msg, color_); // primary color

    // write state against default
    static const VehicleSyncState default_state;
    size_t fields_pos = msg.Reserve<VehicleSyncFieldFlags>();
    auto fields = WriteState(msg, default_state);
    msg.WriteAt(fields_pos, fields);
}

void game::Vehicle::SetInput(VehicleInputType type, bool enable)
{
    if (enable)
        in_ |= (1 << type);
    else
        in_ &= ~(1 << type);
}

glm::vec3 game::Vehicle::GetPosition() const
{
    btVector3 pos = body_->getWorldTransform().getOrigin();
    return glm::vec3(pos.x(), pos.y(), pos.z());
}

void game::Vehicle::SetPosition(const glm::vec3& pos)
{
    auto t = body_->getWorldTransform();
    t.setOrigin(btVector3(pos.x, pos.y, pos.z));
    body_->setWorldTransform(t);
}

glm::quat game::Vehicle::GetRotation() const
{
    btQuaternion rot = body_->getWorldTransform().getRotation();
    return glm::quat(rot.w(), rot.x(), rot.y(), rot.z());
}

float game::Vehicle::GetSpeed() const
{
    return vehicle_->getCurrentSpeedKmHour();
}

void game::Vehicle::SetSteering(bool analog, float value)
{
    steering_analog_ = analog;
    target_steering_ = value;
}

game::Vehicle::~Vehicle()
{
    auto& bt_world = world_.GetBtWorld();
    bt_world.removeRigidBody(body_.get());
    bt_world.removeAction(vehicle_.get());
}

void game::Vehicle::ProcessInput()
{
    // TODO: totally fix

    float t_delta = 1.0f / 25.0f;

    // float steeringIncrement = .04 * 60;
    // float steeringClamp = .5;
    // float maxEngineForce = 7000;
    float maxEngineForce = 4500;
    float maxBreakingForce = 300;

    float speed = vehicle_->getCurrentSpeedKmHour();
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
    const bool in_right  = in_ & (1 << VIN_RIGHT);

    if (in_forward)
    {
        if (speed < -1)
            breakingForce = maxBreakingForce;
        else
            engineForce = maxEngineForce;
    }
    if (in_backward)
    {
        if (speed > 1)
            breakingForce = maxBreakingForce;
        else
            engineForce = -maxEngineForce / 2;
    }

    // idle breaking
    if (!in_forward && !in_backward)
    {
        breakingForce = maxBreakingForce * 0.05f;
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

    vehicle_->applyEngineForce(engineForce, 2);
    vehicle_->applyEngineForce(engineForce, 3);

    vehicle_->setBrake(breakingForce * 0.5, 0);
    vehicle_->setBrake(breakingForce * 0.5, 1);
    vehicle_->setBrake(breakingForce, 2);
    vehicle_->setBrake(breakingForce, 3);

    vehicle_->setSteeringValue(steering_, 0);
    vehicle_->setSteeringValue(steering_, 1);

    if (glm::abs(engineForce) > 0)
        flags_ |= VF_ACCELERATING;

    if (glm::abs(breakingForce) > 0)
        flags_ |= VF_BREAKING;
}

void game::Vehicle::UpdateWheels()
{
    for (size_t i = 0; i < num_wheels_; ++i)
    {
        auto& bt_wheel = vehicle_->getWheelInfo(i);
        wheels_[i].speed = -(bt_wheel.m_rotation - wheels_[i].rotation) * 25.0f;
        wheels_[i].rotation = bt_wheel.m_rotation;
        wheels_[i].z_offset = wheel_z_offset_ - bt_wheel.m_raycastInfo.m_suspensionLength;
    }
}

void game::Vehicle::UpdateSyncState()
{
    VehicleSyncState& state = sync_[sync_current_];

    state.flags = flags_;
    
    net::EncodePosition(root_.local.position, state.pos);
    net::EncodeRotation(root_.local.rotation, state.rot);

    state.steering.Encode(steering_);

    for (size_t i = 0; i < num_wheels_; ++i)
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

    if (curr.pos.x.value != base.pos.x.value ||
        curr.pos.y.value != base.pos.y.value ||
        curr.pos.z.value != base.pos.z.value)
    {
        fields |= VSF_POSITION;

        net::WriteDelta(msg, curr.pos.x, base.pos.x);
        net::WriteDelta(msg, curr.pos.y, base.pos.y);
        net::WriteDelta(msg, curr.pos.z, base.pos.z);
    }

    if (curr.rot.x.value != base.rot.x.value ||
        curr.rot.y.value != base.rot.y.value ||
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
    for (size_t i = 0; i < num_wheels_; ++i)
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

        for (size_t i = 0; i < num_wheels_; ++i)
        {
            net::WriteDelta(msg, curr.wheels[i].z_offset, base.wheels[i].z_offset);
            net::WriteDelta(msg, curr.wheels[i].speed, base.wheels[i].speed);
        }
    }

    return fields;
}

void game::Vehicle::SendUpdateMsg()
{
    auto msg = BeginEntMsg(net::EMSG_UPDATE);
    auto fields_pos = msg.Reserve<VehicleSyncFieldFlags>();
    auto fields = WriteState(msg, sync_[1 - sync_current_]);

    // TODO: allow this
    // if (fields == 0)
    // {
    //     DiscardMsg();
    //     return;
    // }

    msg.WriteAt(fields_pos, fields);
}
