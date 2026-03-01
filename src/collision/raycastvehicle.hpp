#include <btBulletDynamicsCommon.h>

namespace collision
{

class RaycastVehicle : public btRaycastVehicle
{
public:
    RaycastVehicle(const btVehicleTuning& tuning, btRigidBody* chassis, btVehicleRaycaster* raycaster) : 
        btRaycastVehicle(tuning, chassis, raycaster)
    {
    }

    virtual void updateAction(btCollisionWorld* collisionWorld, btScalar step) override
    {
        // only update if not sleeping
        if (getRigidBody()->isActive())
            btRaycastVehicle::updateAction(collisionWorld, step);
    }
};

}