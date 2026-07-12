#include "material.hpp"

#include "renderer.hpp"

gfx::Material::Material(const MaterialInfo& info) : properties_(info.properties), texture_(info.texture)
{
    MaterialDescriptor desc{};
    desc.properties = properties_;
    desc.texture = texture_ ? texture_->GetID() : 0;

    id_ = Renderer::GetInstance().CreateMaterial(desc);
}

gfx::Material::~Material()
{
    Renderer::GetInstance().ReleaseMaterial(id_);
}
