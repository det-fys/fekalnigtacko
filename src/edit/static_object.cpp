#include "static_object.hpp"

#include <imgui.h>

#include "assets/asset_manager.hpp"
#include "im/utils.hpp"
#include "map_viewport.hpp"

edit::StaticObject::StaticObject(const std::string& model_name)
    : StaticObject(assets::AssetManager::GetInstance().Get<ModelView>(model_name))
{
}

edit::StaticObject::StaticObject(std::shared_ptr<const ModelView> model) : model_(std::move(model)) {}

void edit::StaticObject::Draw(const gfx::DrawContext& ctx)
{
    model_->Draw(ctx, GetTransform(), {});
}

void edit::StaticObject::DrawOverlay(const DrawOverlayContext& ctx)
{
    if (!IsSelected())
        return;

    auto& viewport = ctx.viewport;

    // calc screen position
    glm::vec4 world_pos = GetTransform() * glm::vec4(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f);
    glm::vec4 clip_pos = viewport.GetViewProj() * world_pos;
    if (clip_pos.w == 0.0f)
        return;

    glm::vec3 ndc_pos = glm::vec3(clip_pos) / clip_pos.w;

    if (ndc_pos.z < -1.0f || ndc_pos.z > 1.0f)
        return; // behind camera

    glm::vec2 anchor = ndc_pos * 0.5f + 0.5f;
    anchor.y = 1.0f - anchor.y;
    anchor *= viewport.GetCanvasSize();

    static const std::string text = "selected";

    auto text_size = im::VecFromImGui(ImGui::CalcTextSize(text.c_str()));
    glm::vec2 pos = viewport.GetCanvasP0() + anchor + text_size * glm::vec2(-0.5f, -1.0f);

    ctx.draw_list.AddText(im::VecToImGui(pos), 0xFFFFFFFF, text.c_str());
}
