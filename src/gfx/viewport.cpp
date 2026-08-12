#include "viewport.hpp"

#include "renderer.hpp"

gfx::Viewport::Viewport()
{
    id_ = Renderer::GetInstance().CreateViewport();
}

void gfx::Viewport::Draw(Scene& scene, const CameraParams& camera, const glm::u32vec2& size)
{
    Renderer::GetInstance().DrawViewport(id_, scene, camera, size);
}

gfx::ViewportTextureHandle gfx::Viewport::GetNativeHandle() const
{
    return Renderer::GetInstance().GetViewportNativeHandle(id_);
}

gfx::Viewport::~Viewport()
{
    Renderer::GetInstance().ReleaseViewport(id_);
}
