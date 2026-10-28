#include "static_object.hpp"

#include <imgui.h>

#include "assets/asset_manager.hpp"
#include "im/utils.hpp"
#include "map_viewport.hpp"
#include "utils/files.hpp"

edit::StaticObject::StaticObject(Project& project, const FarMatrix& trans, uint32_t model_id)
    : Object(project, trans), model_id_(model_id)
{
    const auto& mgmodel = project.GetResourceSet().GetModels().GetByIndex(model_id_);
    model_ = assets::AssetManager::GetInstance().Get<ModelView>(mgmodel.model->GetAssetName());

    UpdateAABB();
    MaybeInvalidateChunks();
}

void edit::StaticObject::Draw(const gfx::DrawContext& ctx)
{
    //model_->Draw(ctx, GetTransform(), {});
}

void edit::StaticObject::DrawOverlay(const DrawOverlayContext& ctx)
{
    Super::DrawOverlay(ctx);
}

void edit::StaticObject::SetTransform(const FarMatrix& trans)
{
    MaybeInvalidateChunks();

    Super::SetTransform(trans);
    UpdateAABB();

    MaybeInvalidateChunks();
}

void edit::StaticObject::Clone(const FarMatrix& trans)
{
    GetProject().AddStaticObject(trans, model_id_);
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
    InvalidateChunks(GetAABB());
}
