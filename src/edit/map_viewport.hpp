#pragma once

#include "im/scene_view.hpp"
#include "im/viewport.hpp"
#include "map_edit_context.hpp"
#include <ImGuizmo.h>
#include <imgui.h>

namespace edit
{

class MapViewport : public im::Viewport
{
public:
    using Super = im::Viewport;

    MapViewport(std::string title, MapEditContext& context);

    const glm::mat4& GetProj() const { return proj_; }
    const glm::mat4& GetView() const { return view_; }
    const glm::mat4& GetViewProj() const { return view_proj_; }

    const gfx::Frustum GetFrustum() const { return frustum_; }

protected:
    virtual void Draw(ImDrawList& draw_list) override;

    void SetCameraParams(const gfx::CameraParams& cam) { cam_ = cam; }

    void SetOverlayMatrices(const glm::mat4& view, const glm::mat4& proj)
    {
        view_ = view;
        proj_ = proj;
        view_proj_ = proj * view;
        frustum_ = gfx::Frustum(view_proj_);
    }

    MapEditContext& GetContext() { return context_; }

private:
    MapEditContext& context_;

    im::SceneView scene_view_;

    gfx::CameraParams cam_{};
    glm::mat4 proj_{1.0f};
    glm::mat4 view_{1.0f};
    glm::mat4 view_proj_{1.0f};
    gfx::Frustum frustum_;
};

} // namespace edit
