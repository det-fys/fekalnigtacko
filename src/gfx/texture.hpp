#pragma once

#include "renderer.hpp"
#include "utils/defs.hpp"
#include "assets/asset_manager.hpp"

namespace gfx
{

class Texture : public assets::Asset
{
public:
    Texture(const TextureDescriptor& desc);
    DELETE_COPY_MOVE(Texture);

    static std::shared_ptr<Texture> Load(const std::string& name);
    static std::shared_ptr<Texture> LoadFromFile(const std::string& filename);

    void SetData(std::span<const uint8_t> data);

    TextureID GetID() const { return id_; }

    ~Texture();

private:
    TextureID id_;
};

} // namespace gfx
