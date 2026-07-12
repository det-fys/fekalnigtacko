#pragma once

#include "gfx/material.hpp"
#include <string_view>
#include <optional>

namespace game::view
{

class SpzTexture
{
public:
    SpzTexture();

    void Render(std::string_view text, uint32_t color_mount, uint32_t color_bg, uint32_t color_fg);

    gfx::MaterialID GetMaterialID() const { return material_->GetID(); }

private:
    std::shared_ptr<gfx::Texture> texture_;
    std::optional<gfx::Material> material_;

};


}