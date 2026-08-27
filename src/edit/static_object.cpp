#include "static_object.hpp"

#include <imgui.h>

#include "assets/asset_manager.hpp"
#include "im/utils.hpp"
#include "map_viewport.hpp"
#include "utils/files.hpp"

edit::StaticObject::StaticObject(Project& project, const glm::mat4& trans, const std::string& model_name)
    : Object(project, trans), model_name_(model_name)
{
    model_ = assets::AssetManager::GetInstance().Get<ModelView>(model_name);

    if (fs::FileExists("data/" + model_name + ".hm"))
    {
        // load heightmesh
        heightmesh_ = assets::AssetManager::GetInstance().Get<mg::HeightMesh>(model_name);
    }

    UpdateAABB();
    MaybeInvalidateChunks();
}

void edit::StaticObject::Draw(const gfx::DrawContext& ctx)
{
    model_->Draw(ctx, GetTransform(), {});
}

void edit::StaticObject::DrawOverlay(const DrawOverlayContext& ctx)
{
    Super::DrawOverlay(ctx);
}

void edit::StaticObject::SetTransform(const glm::mat4& trans)
{
    MaybeInvalidateChunks();

    Super::SetTransform(trans);
    UpdateAABB();

    MaybeInvalidateChunks();
}

void edit::StaticObject::Clone(const glm::mat4& trans)
{
    GetProject().AddStaticObject(trans, model_name_);
}

void edit::StaticObject::Delete()
{
    MaybeInvalidateChunks();
}

void edit::StaticObject::UpdateAABB()
{
     SetAABB(TransformAABB(model_->GetModel()->GetAABB(), GetTransform()));
}

void edit::StaticObject::MaybeInvalidateChunks()
{
    bool invalidate_needed = (bool)heightmesh_;

    if (invalidate_needed)
        InvalidateChunks(GetAABB());
}
