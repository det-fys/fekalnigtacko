#pragma once

#include <cstdint>
#include <btBulletDynamicsCommon.h>
#include <span>

namespace collision
{

enum Material : uint8_t
{
    PM_STONE,
    PM_DIRT,
    PM_GRASS,
    PM_WOOD,
    PM_METAL,
    PM_GLASS,
    PM_PLASTIC,
    PM_FLESH,
};

struct ShapeInfo
{
    std::span<Material> triangle_materials; 
};

inline void SetShapeMaterial(btCollisionShape& shape, Material material)
{
    shape.setUserIndex(material);
}

inline Material GetShapeMaterial(const btCollisionShape& shape)
{
    return static_cast<Material>(shape.getUserIndex());
}

inline void SetShapeInfo(btCollisionShape& shape, const ShapeInfo* info)
{
    shape.setUserPointer(const_cast<ShapeInfo*>(info));
}

inline const ShapeInfo* GetShapeInfo(const btCollisionShape& shape)
{
    return reinterpret_cast<ShapeInfo*>(shape.getUserPointer());
}

}