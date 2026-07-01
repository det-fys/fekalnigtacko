#include "destroyed_object.hpp"
#include "utils/random.hpp"

game::DestroyedObject::DestroyedObject(World& world, std::unique_ptr<MapObjectCollision> col)
    : Super(world, col->GetModel()->GetAssetName()), col_(std::move(col))
{
    auto destr_snd_str = col_->GetModel()->GetParam("destr_snd");
    if (destr_snd_str)
    {
        Schedule(1, [this, destr_snd_str] {
            PlaySound(*destr_snd_str, RandomFloat(0.8f, 1.2f), RandomFloat(0.8f, 1.2f));
        });
    }

    // remove after 30s
    Schedule(30000, [this] {
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
