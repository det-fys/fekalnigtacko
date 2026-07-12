#pragma once

#include "material_desc.hpp"
#include "texture.hpp"
#include "utils/defs.hpp"

namespace gfx
{

struct MaterialInfo
{
    MaterialProperties properties{};
    std::shared_ptr<const Texture> texture;
};

class Material
{
public:
    Material(const MaterialInfo& info);
    DELETE_COPY_MOVE(Material);

    const MaterialProperties& GetProperties() const { return properties_; }
    const std::shared_ptr<const Texture>& GetTexture() const { return texture_; }

    MaterialID GetID() const { return id_; }

    ~Material();

private:
    MaterialProperties properties_;
    std::shared_ptr<const Texture> texture_;

    MaterialID id_;
};


}