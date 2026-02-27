#include "destroyed_object.hpp"

game::DestroyedObject::DestroyedObject(World& world, std::unique_ptr<MapObjectCollision> col)
    : Super(world, col->GetModel()->GetName()), col_(std::move(col))
{
    // remove after 30s
    Schedule(30000, [this]()
    {
       Remove();
    });
}

void game::DestroyedObject::UpdatePreSync()
{
    if (!col_)
        return;

    // sync transform with the physics body
    col_->GetModelTransform(root_.local);

    root_.UpdateMatrix();
}
