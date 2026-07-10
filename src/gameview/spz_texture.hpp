#pragma once

#include "gfx/texture.hpp"
#include <string_view>

namespace game::view
{

class SpzTexture
{
public:
    SpzTexture();

    void Render(std::string_view text, uint32_t color_mount, uint32_t color_bg, uint32_t color_fg);

    std::shared_ptr<const gfx::Texture> GetTexture() { return tex_; }

private:


private:
    std::shared_ptr<gfx::Texture> tex_;

};


}