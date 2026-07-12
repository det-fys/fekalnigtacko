#include "deform_texture.hpp"

#include "renderer.hpp"

gfx::DeformTexture::DeformTexture(const DeformTextureDescriptor& desc)
{
    id_ = Renderer::GetInstance().CreateDeformTexture(desc);
}

void gfx::DeformTexture::SetData(std::span<const glm::i8vec3> data)
{
    Renderer::GetInstance().SetDeformTextureData(id_, data);
}

gfx::DeformTexture::~DeformTexture()
{
    Renderer::GetInstance().ReleaseDeformTexture(id_);
}
