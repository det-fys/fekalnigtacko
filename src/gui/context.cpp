#include "context.hpp"

gui::Context::Context(gfx::DrawList& dlist, std::shared_ptr<const Font> default_font) :
    dlist_(dlist),
    font_(std::move(default_font)),
    va_(gfx::VA_POSITION | gfx::VA_UV | gfx::VA_COLOR, gfx::VF_CREATE_EBO | gfx::VF_DYNAMIC)
{

}

void gui::Context::Begin()
{
    vertices_.clear();
    indices_.clear();
    ranges_.clear();
}

static uint32_t DecodeUTF8Codepoint(const char*& p, const char* end)
{
    if (p == end)
        return 0;

    if ((*p & 0b10000000) == 0)
    { // 1-byte sequence
        return *p++;
    }

    uint32_t codepoint = 0;

    if ((*p & 0b11100000) == 0b11000000)
    { // 2-byte seq
        codepoint = (*p++ & 0b00011111) << 6;
        if (p == end)
            return 0;
        codepoint |= (*p++ & 0b00111111);
    }
    else if ((*p & 0b11110000) == 0b11100000)
    { // 3-byte seq
        codepoint = (*p++ & 0b00001111) << 12;
        if (p == end)
            return 0;
        codepoint |= (*p++ & 0b00111111) << 6;
        if (p == end)
            return 0;
        codepoint |= (*p++ & 0b00111111);
    }
    else if ((*p & 0b11111000) == 0b11110000)
    { // 4-byte seq
        codepoint = (*p++ & 0b00000111) << 18;
        if (p == end)
            return 0;
        codepoint |= (*p++ & 0b00111111) << 12;
        if (p == end)
            return 0;
        codepoint |= (*p++ & 0b00111111) << 6;
        if (p == end)
            return 0;
        codepoint |= (*p++ & 0b00111111);
    }

    return codepoint;
}

glm::vec2 gui::Context::MeasureText(std::string_view text)
{
    glm::vec2 size(0.0f);

    const char* p = text.data();
    const char* end = p + text.size();

    if (p == end) // empty string
        return size;

    uint32_t cp = 0;
    const float line_height = font_->GetLineHeight();
    float space_size = line_height * 0.3f;

    glm::vec2 cursor(0.0f);

    while (cp = DecodeUTF8Codepoint(p, end))
    {
        if (cp == ' ')
        {
            cursor.x += space_size; // Move cursor for space
            continue;
        }
        else if (cp == '\n')
        {
            cursor.x = 0.0f;
            cursor.y += line_height;
            continue;
        }
        else if (cp == '^')
        {
            if (!(cp = DecodeUTF8Codepoint(p, end)))
                break;

            if (cp == 'r')
                continue;

            // parse color
            for (size_t i = 0; i < 3; ++i)
            {
                if (!(cp = DecodeUTF8Codepoint(p, end)))
                    break;
            }
        }

        const FontGlyphData* glyph = font_->GetCodepointGlyph(cp);

        if (!glyph)
            continue; // Dont even have "missing" glyph, font is shit

        cursor.x += glyph->advance;
        size.x = glm::max(size.x, cursor.x);
    }

    size.y = cursor.y + line_height;

    return size;
}

void gui::Context::DrawText(std::string_view text, const glm::vec2& pos, uint32_t color, float scale)
{
    const char* p = text.data();
    const char* end = p + text.size();

    if (p == end) // empty string
        return;

    BeginTexture(font_->GetTexture().get());

    uint32_t cp = 0;
    const float line_height = font_->GetLineHeight();
    float space_size = line_height * 0.3f;

    glm::vec2 cursor = pos;

    uint32_t curr_color = color;

    while (cp = DecodeUTF8Codepoint(p, end))
    {
        if (cp == ' ')
        {
            cursor.x += space_size; // Move cursor for space
            continue;
        }
        else if (cp == '\n')
        {
            cursor.x = 0.0f;
            cursor.y += line_height;
            continue;
        }
        else if (cp == '^')
        {
            if (!(cp = DecodeUTF8Codepoint(p, end)))
                break;

            if (cp == 'r') // reset color
            {
                curr_color = color;
                continue;
            }

            curr_color = 0;

            // parse color
            for (size_t i = 0; i < 3; ++i)
            {
                curr_color >>= 8;
                uint32_t ch;
                if (cp >= '0' && cp <= '9')
                    ch = cp - '0';
                else if (cp >= 'a' && cp <= 'f')
                    ch = cp - 'a' + 10;
                else
                    break;

                curr_color |= (ch << 16);
                curr_color |= (ch << 20);

                if (!(cp = DecodeUTF8Codepoint(p, end)))
                    break;
            }

            curr_color |= (color & 0xFF000000); // preserve alpha
        }

        const FontGlyphData* glyph = font_->GetCodepointGlyph(cp);

        if (!glyph)
            continue; // Dont even have "missing" glyph, font is shit

        glm::vec2 p0 = cursor + glyph->offset;
        glm::vec2 p1 = p0 + glyph->size * scale;

        PushRect(p0, glyph->uv0, p1, glyph->uv1, curr_color);
        cursor.x += glyph->advance * scale;
    }
}

void gui::Context::Render()
{
    va_.SetVBOData(vertices_.data(), vertices_.size() * sizeof(vertices_[0]));
    va_.SetIndices(indices_.data(), indices_.size());

    static const gfx::HudPosition pos;

    for (const auto& range : ranges_)
    {
        gfx::DrawHudCmd hudcmd;
        hudcmd.va = &va_;
        hudcmd.pos = &pos;
        hudcmd.texture = range.texture;
        hudcmd.first = range.start;
        hudcmd.count = range.count;
        dlist_.AddHUD(hudcmd);
    }
}

void gui::Context::BeginTexture(const gfx::Texture* texture)
{
    if (!ranges_.empty() && ranges_.back().texture == texture)
        return;

    auto& range = ranges_.emplace_back();
    range.start = indices_.size();
    range.count = 0;
    range.texture = texture;
}

void gui::Context::PushRect(const glm::vec2& p0, const glm::vec2& uv0, const glm::vec2& p1, const glm::vec2& uv1, uint32_t color)
{
    uint32_t base_index = vertices_.size();

    vertices_.emplace_back(glm::vec3(p0.x, p0.y, 0.0f), color, uv0);
    vertices_.emplace_back(glm::vec3(p1.x, p0.y, 0.0f), color, glm::vec2(uv1.x, uv0.y));
    vertices_.emplace_back(glm::vec3(p1.x, p1.y, 0.0f), color, uv1);
    vertices_.emplace_back(glm::vec3(p0.x, p1.y, 0.0f), color, glm::vec2(uv0.x, uv1.y));

    indices_.push_back(base_index + 0);
    indices_.push_back(base_index + 1);
    indices_.push_back(base_index + 2);
    indices_.push_back(base_index + 0);
    indices_.push_back(base_index + 2);
    indices_.push_back(base_index + 3);

    ranges_.back().count += 2; // 2 tris
}
