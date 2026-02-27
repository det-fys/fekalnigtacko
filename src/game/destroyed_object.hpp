#pragma once

#include "simple_entity.hpp"
#include "mapinstance.hpp"

namespace game
{

class DestroyedObject : public SimpleEntity
{
public:
    using Super = SimpleEntity;

    DestroyedObject(World& world, std::unique_ptr<MapObjectCollision> col);

    virtual void UpdatePreSync() override;

private:
    std::unique_ptr<MapObjectCollision> col_;
};

} // namespace game