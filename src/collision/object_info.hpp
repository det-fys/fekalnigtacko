#pragma once

#include <cstdint>
#include <btBulletDynamicsCommon.h>

namespace game
{
    struct BulletInfo;
}

namespace collision
{

enum ObjectType : int
{
    OT_UNDEFINED,
    OT_MAP_OBJECT,
    OT_ENTITY,
};

using ObjectFlags = int;

enum ObjectFlag : ObjectFlags
{
    OF_DESTRUCTIBLE = 1,
    OF_NOTIFY_CONTACT = 2,
    OF_USABLE = 4,
    OF_DESTRUCTING = 8,
};

struct ContactInfo
{
    glm::vec3 pos;
    glm::vec3 normal;
    float impulse;
};

class ObjectCallback
{
public:
    ObjectCallback() = default;

    virtual void OnContact(const ContactInfo& info) {}
    virtual void OnBulletHit(const game::BulletInfo& bullet, const btCollisionObject* hit_object) {}

    virtual ~ObjectCallback() = default;
};

inline void SetObjectInfo(btCollisionObject* obj, ObjectType type, ObjectFlags flags, ObjectCallback* callback)
{
    obj->setUserIndex(static_cast<int>(type));
    obj->setUserIndex2(static_cast<int>(flags));
    obj->setUserPointer(callback);
}

inline void AddObjectFlags(btCollisionObject* obj, ObjectFlags flags)
{
    obj->setUserIndex2(static_cast<int>(static_cast<ObjectFlags>(obj->getUserIndex2())) | flags);
}

inline ObjectType GetObjectType(const btCollisionObject* obj)
{
    return static_cast<ObjectType>(obj->getUserIndex());
}

inline ObjectFlags GetObjectFlags(const btCollisionObject* obj)
{
    return static_cast<ObjectFlags>(obj->getUserIndex2());
}

inline ObjectCallback* GetObjectCallback(const btCollisionObject* obj)
{
    return static_cast<ObjectCallback*>(obj->getUserPointer());
}

// legacy
inline void GetObjectInfo(const btCollisionObject* obj, ObjectType& type, ObjectFlags& flags, ObjectCallback*& callback)
{
    type = GetObjectType(obj);
    flags = GetObjectFlags(obj);
    callback = GetObjectCallback(obj);
}

}