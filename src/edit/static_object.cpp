#include "static_object.hpp"

#include <imgui.h>

#include "assets/asset_manager.hpp"
#include "im/utils.hpp"
#include "map_viewport.hpp"
#include "utils/files.hpp"

edit::StaticObject::StaticObject(Project& project, const std::string& model_name)
    : Object(project), model_name_(model_name)
{
    model_ = assets::AssetManager::GetInstance().Get<ModelView>(model_name);

    if (fs::FileExists("data/" + model_name + ".hm"))
    {
        // load heightmesh
        heightmesh_ = assets::AssetManager::GetInstance().Get<mg::HeightMesh>(model_name);
    }

    UpdateAABB();
}

void edit::StaticObject::Draw(const gfx::DrawContext& ctx)
{
    model_->Draw(ctx, GetTransform(), {});
}

static bool GetScreenPos(const edit::MapViewport& viewport, const glm::vec3& world_pos, glm::vec2& out_screen_pos)
{
    glm::vec4 clip_pos = viewport.GetViewProj() * glm::vec4(world_pos, 1.0f);
    if (clip_pos.w == 0.0f)
        return false;

    glm::vec3 ndc_pos = glm::vec3(clip_pos) / clip_pos.w;
    if (ndc_pos.z < -1.0f || ndc_pos.z > 1.0f)
        return false; // behind camera
    
    glm::vec2 anchor = ndc_pos * 0.5f + 0.5f;
    anchor.y = 1.0f - anchor.y;
    anchor *= viewport.GetCanvasSize();
    out_screen_pos = viewport.GetCanvasP0() + anchor;
    return true;
}

static void DrawLineWs(const edit::DrawOverlayContext& ctx, const glm::vec3& p0, const glm::vec3& p1, ImU32 color)
{
    glm::vec2 screen_p0, screen_p1;
    if (!GetScreenPos(ctx.viewport, p0, screen_p0))
        return;
    if (!GetScreenPos(ctx.viewport, p1, screen_p1))
        return;
    ctx.draw_list.AddLine(im::VecToImGui(screen_p0), im::VecToImGui(screen_p1), color);
}

void edit::StaticObject::DrawOverlay(const DrawOverlayContext& ctx)
{
    if (!IsSelected())
        return;

    // draw AABB
    auto& aabb = GetAABB();
    ImU32 color = 0xFF00FFFF;

    DrawLineWs(ctx, glm::vec3(aabb.min.x, aabb.min.y, aabb.min.z), glm::vec3(aabb.max.x, aabb.min.y, aabb.min.z),
               color);
    DrawLineWs(ctx, glm::vec3(aabb.max.x, aabb.min.y, aabb.min.z), glm::vec3(aabb.max.x, aabb.max.y, aabb.min.z),
               color);
    DrawLineWs(ctx, glm::vec3(aabb.max.x, aabb.max.y, aabb.min.z), glm::vec3(aabb.min.x, aabb.max.y, aabb.min.z),
               color);
    DrawLineWs(ctx, glm::vec3(aabb.min.x, aabb.max.y, aabb.min.z), glm::vec3(aabb.min.x, aabb.min.y, aabb.min.z),
               color);

    //auto& viewport = ctx.viewport;

    //// calc screen position
    //glm::vec4 world_pos = GetTransform() * glm::vec4(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f);
    //glm::vec4 clip_pos = viewport.GetViewProj() * world_pos;
    //if (clip_pos.w == 0.0f)
    //    return;

    //glm::vec3 ndc_pos = glm::vec3(clip_pos) / clip_pos.w;

    //if (ndc_pos.z < -1.0f || ndc_pos.z > 1.0f)
    //    return; // behind camera

    //glm::vec2 anchor = ndc_pos * 0.5f + 0.5f;
    //anchor.y = 1.0f - anchor.y;
    //anchor *= viewport.GetCanvasSize();

    //static const std::string text = "selected";

    //auto text_size = im::VecFromImGui(ImGui::CalcTextSize(text.c_str()));
    //glm::vec2 pos = viewport.GetCanvasP0() + anchor + text_size * glm::vec2(-0.5f, -1.0f);

    //ctx.draw_list.AddText(im::VecToImGui(pos), 0xFFFFFFFF, text.c_str());
}

void edit::StaticObject::SetTransform(const glm::mat4& trans)
{
    InvalidateChunks(GetAABB());
    Super::SetTransform(trans);
    UpdateAABB();
    InvalidateChunks(GetAABB());
}

void edit::StaticObject::UpdateAABB()
{
     SetAABB(TransformAABB(model_->GetModel()->GetAABB(), GetTransform()));
}
