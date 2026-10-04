#pragma once

#include <cstdint>
#include <btBulletDynamicsCommon.h>
#include <span>

namespace collision
{

enum Material : uint8_t
{
    PM_NONE,
    PM_STONE,
    PM_DIRT,
    PM_GRASS,
    PM_WOOD,
    PM_METAL,
    PM_GLASS,
    PM_PLASTIC,
    PM_FLESH,
};

inline Material GetMaterialByName(const std::string& name)
{
    if (name == "stone")
        return collision::PM_STONE;
    else if (name == "dirt")
        return collision::PM_DIRT;
    else if (name == "grass")
        return collision::PM_GRASS;
    else if (name == "wood")
        return collision::PM_WOOD;
    else if (name == "metal")
        return collision::PM_METAL;
    else if (name == "glass")
        return collision::PM_GLASS;
    else if (name == "flesh")
        return collision::PM_FLESH;
    else if (name == "car") // TODO: make new material for cars
        return collision::PM_METAL;
    else if (name == "carwindow")
        return collision::PM_NONE;
    else if (name == "none")
        return collision::PM_NONE;
    else
        return collision::PM_STONE;
}

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