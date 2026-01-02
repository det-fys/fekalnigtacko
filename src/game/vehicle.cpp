#include "vehicle.hpp"

#include "assets/cache.hpp"
#include "player.hpp"
#include "player_input.hpp"

static std::shared_ptr<const assets::VehicleModel> LoadVehicleModelByName(const std::string& model_name)
{
    return assets::CacheManager::GetVehicleModel("data/" + model_name + ".veh");
}

struct Shape
{
    btBoxShape box;
    btCompoundShape compound;

    Shape() : box(btVector3(1, 1, 1))
    {
        btTransform t(btQuaternion(0, 0, 0), btVector3(0, 0, 2));
        compound.addChildShape(t, &box);
    }
};

game::Vehicle::Vehicle(World& world, std::string model_name)
    : Entity(world, net::ET_VEHICLE), model_name_(model_name), model_(LoadVehicleModelByName(model_name)),
      motion_(root_.local)
{
    root_.local.position.z = 10.0f;

    // setup chassis rigidbody
    float mass = 300.0f;
    static Shape shape;

    btVector3 local_inertia(0, 0, 0);
    shape.compound.calculateLocalInertia(mass, local_inertia);

    btRigidBody::btRigidBodyConstructionInfo rb_info(mass, &motion_, &shape.compound, local_inertia);
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

    for (const auto& wheels = model_->GetWheels(); const auto& wheeldef : wheels)
    {
        float suspension_rest_length = 0.6f;

        float wheelRadius = .35f;

        float friction = 5.0f;
        float suspensionStiffness = 60.0f;
        //float suspensionDamping = 2.3f;
        //float suspensionCompression = 4.4f;
        float suspensionRestLength = 0.6f;
        float rollInfluence = 0.01f;

        float maxSuspensionForce = 100000.0f;
        float maxSuspensionTravelCm = 5000.0f;

        float k = 0.2;

        const bool is_front = !(wheeldef.type & assets::WHEEL_REAR);

        btVector3 wheel_pos(wheeldef.position.x, wheeldef.position.y, wheeldef.position.z);
        auto& wi = vehicle_->addWheel(wheel_pos, wheelDirectionCS0, wheelAxleCS, suspension_rest_length,
                                         wheelRadius, tuning, is_front);

        wi.m_suspensionStiffness = suspensionStiffness;

        wi.m_wheelsDampingCompression = k * 2.0 * btSqrt(suspensionStiffness); //vehicleTuning.suspensionCompression;
        wi.m_wheelsDampingRelaxation = k * 3.3 * btSqrt(suspensionStiffness);//vehicleTuning.suspensionDamping;
        
        wi.m_frictionSlip = friction;
        //if (wi.m_bIsFrontWheel) wi.m_frictionSlip = vehicleTuning.friction * 1.4f;
        
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

    ProcessInput();

    for (int j = 0; j < vehicle_->getNumWheels(); j++)
    {
        auto& wheel = vehicle_->getWheelInfo(j);
        float sus_length = wheel.m_raycastInfo.m_suspensionLength;
        // TODO: sync wheels
    }

    SendUpdateMsg();
}

void game::Vehicle::SendInitData(Player& player, net::OutMessage& msg) const
{
    net::ModelName name(model_name_);
    msg.Write(name);
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

    float steeringIncrement = .04 * 60;
    // float steeringClamp = .5;
    float maxEngineForce = 7000;
    float maxBreakingForce = 300;

    float engineForce = 0;
    float breakingForce = 0;

    // process input
    PlayerInputFlags in = GetController() ? GetController()->GetInput() : 0;

    if (in & IN_DEBUG1)
    {
        auto t = body_->getWorldTransform();
        t.setOrigin(btVector3(0, 0, 5));
        body_->setWorldTransform(t);
    }

    float speed = vehicle_->getCurrentSpeedKmHour();

    float maxsc = .5f;
    float minsc = .08f;
    float sl = 130.f;

    float steeringClamp = std::max(minsc, (1.f - (std::abs(speed) / sl)) * maxsc);
    // steeringClamp = .5f;

    float t_delta = 1.0f / 25.0f;

    if (in & IN_FORWARD)
    {
        if (speed < -1)
            breakingForce = maxBreakingForce;
        else
            engineForce = maxEngineForce;
    }
    if (in & IN_BACKWARD)
    {
        if (speed > 1)
            breakingForce = maxBreakingForce;
        else
            engineForce = -maxEngineForce / 2;
    }

    // idle breaking
    // if (in & (IN_FORWARD | IN_BACKWARD) == 0)
    // {
    //     breakingForce = maxBreakingForce * 0.5f;
    // }

    if (in & IN_LEFT)
    {
        if (steering_ < steeringClamp)
            steering_ += steeringIncrement * t_delta;
    }
    else
    {
        if (in & IN_RIGHT)
        {
            if (steering_ > -steeringClamp)
                steering_ -= steeringIncrement * t_delta;
        }
        else
        {
            if (steering_ < -steeringIncrement * t_delta)
                steering_ += steeringIncrement * t_delta;
            else
            {
                if (steering_ > steeringIncrement * t_delta)
                    steering_ -= steeringIncrement * t_delta;
                else
                {
                    steering_ = 0.0f;
                }
            }
        }
    }

    vehicle_->applyEngineForce(engineForce, 2);
    vehicle_->applyEngineForce(engineForce, 3);

    vehicle_->setBrake(breakingForce * 0.5, 0);
    vehicle_->setBrake(breakingForce * 0.5, 1);
    vehicle_->setBrake(breakingForce, 2);
    vehicle_->setBrake(breakingForce, 3);

    vehicle_->setSteeringValue(steering_, 0);
    vehicle_->setSteeringValue(steering_, 1);
}

void game::Vehicle::SendUpdateMsg()
{
    const auto& trans = root_.local;

    auto umsg = BeginEntMsg(net::EMSG_UPDATE);

    // write position
    umsg.Write<net::PositionQ>(trans.position.x);
    umsg.Write<net::PositionQ>(trans.position.y);
    umsg.Write<net::PositionQ>(trans.position.z);

    // write angles
    glm::vec3 angles = glm::eulerAngles(trans.rotation);
    umsg.Write<net::AngleQ>(angles.x);
    umsg.Write<net::AngleQ>(angles.y);
    umsg.Write<net::AngleQ>(angles.z);

    // TEMP wheels
    // TODO: REMOVE
    for (size_t i =0; i < vehicle_->getNumWheels(); ++i)
    {
        auto& wheel = vehicle_->getWheelInfo(i);
        vehicle_->updateWheelTransformsWS(wheel);

        Transform trans;
        trans.SetBtTransform(wheel.m_worldTransform);

        // write position
        umsg.Write<net::PositionQ>(trans.position.x);
        umsg.Write<net::PositionQ>(trans.position.y);
        umsg.Write<net::PositionQ>(trans.position.z);

        // write angles
        glm::vec3 angles = glm::eulerAngles(trans.rotation);
        umsg.Write<net::AngleQ>(angles.x);
        umsg.Write<net::AngleQ>(angles.y);
        umsg.Write<net::AngleQ>(angles.z);
    }
}
