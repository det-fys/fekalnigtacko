#include "font.hpp"

#include "assets/cache.hpp"
#include "assets/cmdfile.hpp"

std::shared_ptr<const gfx::Font> gfx::Font::LoadFromFile(const std::string& path)
{
    auto font = std::make_shared<Font>();

    glm::vec2 size(1.0f);

    auto PxToUv = [&](const glm::vec2& pos_px) { return pos_px / size; };

    assets::LoadCMDFile(path, [&](const std::string& cmd, std::istringstream& iss) {
        if (cmd == "c")
        {
            Codepoint cp = 0;
            glm::vec2 c_pos(0.0f);
            glm::vec2 c_size(0.0f);
            glm::vec3 c_offset(0.0f);
            float xadv = 0.0f;

            iss >> cp >> c_pos.x >> c_pos.y >> c_size.x >> c_size.y >> c_offset.x >> c_offset.y >> xadv;

            auto& glyph = font->glyphs_.Alloc(cp);
            glyph.uv0 = PxToUv(c_pos);
            glyph.uv1 = PxToUv(c_pos + c_size);
            glyph.offset = c_offset;
            glyph.size = c_size;
            glyph.advance = xadv;
        }
        else if (cmd == "texture")
        {
            std::string tex_name;
            iss >> tex_name >> size.x >> size.y;

            font->texture_ = assets::CacheManager::GetTexture("data/" + tex_name + ".png");
        }
        else if (cmd == "size")
        {
            iss >> font->line_height_;
        }
    });

    return font;
}
