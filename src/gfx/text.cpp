#include "text.hpp"

#include "utils/bufferput.hpp"

gfx::Text::Text(std::shared_ptr<const Font> font, uint32_t color)
    : font_(std::move(font)), va_(VA_POSITION | VA_UV | VA_COLOR, VF_CREATE_EBO | VF_DYNAMIC), color_(color)
{
}

static uint32_t DecodeUTF8Codepoint(const char*& p)
{
    if (!*p)
        return 0;

    if ((*p & 0b10000000) == 0)
    { // 1-byte sequence
        return *p++;
    }

    uint32_t codepoint = 0;

    if ((*p & 0b11100000) == 0b11000000)
    { // 2-byte seq
        codepoint = (*p++ & 0b00011111) << 6;
        if (!*p)
            return 0;
        codepoint |= (*p++ & 0b00111111);
    }
    else if ((*p & 0b11110000) == 0b11100000)
    { // 3-byte seq
        codepoint = (*p++ & 0b00001111) << 12;
        if (!*p)
            return 0;
        codepoint |= (*p++ & 0b00111111) << 6;
        if (!*p)
            return 0;
        codepoint |= (*p++ & 0b00111111);
    }
    else if ((*p & 0b11111000) == 0b11110000)
    { // 4-byte seq
        codepoint = (*p++ & 0b00000111) << 18;
        if (!*p)
            return 0;
        codepoint |= (*p++ & 0b00111111) << 12;
        if (!*p)
            return 0;
        codepoint |= (*p++ & 0b00111111) << 6;
        if (!*p)
            return 0;
        codepoint |= (*p++ & 0b00111111);
    }

    return codepoint;
}


struct TextVertex
{
    glm::vec3 pos;
    uint32_t color;
    glm::vec2 uv;
};

static void PutVertex(std::vector<TextVertex>& buf, const glm::vec3 pos, const glm::vec2& uv, uint32_t color)
{
    //BufferPut(buf, pos);
    //BufferPut(buf, color);
    //BufferPut(buf, uv);
    buf.emplace_back(TextVertex{pos, color, uv});
}

void gfx::Text::SetText(const char* text)
{
    static std::vector<TextVertex> vertices;
    static std::vector<uint32_t> indices;
    vertices.clear();
    indices.clear();

    float space_size = font_->GetLineHeight() * 0.3f;

    glm::vec2 cursor(0.0f, 0.0f);

    uint32_t cp = 0;
    const char* p = text;

    uint32_t color = color_;

    while (cp = DecodeUTF8Codepoint(p))
    {
        if (cp == ' ')
        {
            cursor.x += space_size; // Move cursor for space
            continue;
        }
        else if (cp == '^')
        {
            if (!(cp = DecodeUTF8Codepoint(p)))
                break;

            if (cp == 'r') // reset color
            {
                color = color_;
                continue;
            }

            color = 0;

            // parse color
            for (size_t i = 0; i < 3; ++i)
            {
                color >>= 8;
                uint32_t ch;
                if (cp >= '0' && cp <= '9')
                    ch = cp - '0';
                else if (cp >= 'a' && cp <= 'f')
                    ch = cp - 'a' + 10;
                else
                    break;

                color |= (ch << 16);
                color |= (ch << 20);

                if (!(cp = DecodeUTF8Codepoint(p)))
                    break;
            }

            color |= 0xFF000000; // alpha=1

            //if (cp != ';')
            //    break;

            //continue;
        }

        const FontGlyphData* glyph = font_->GetCodepointGlyph(cp);

        if (!glyph)
            continue; // Dont even have "missing" glyph, font is shit

        glm::vec2 p0 = cursor + glyph->offset;
        glm::vec2 p1 = p0 + glyph->size;

        uint32_t base_index = vertices.size();

        PutVertex(vertices, glm::vec3(p0.x, -p0.y, 0.0f), glyph->uv0, color);
        PutVertex(vertices, glm::vec3(p1.x, -p0.y, 0.0f), glm::vec2(glyph->uv1.x, glyph->uv0.y), color);
        PutVertex(vertices, glm::vec3(p1.x, -p1.y, 0.0f), glyph->uv1, color);
        PutVertex(vertices, glm::vec3(p0.x, -p1.y, 0.0f), glm::vec2(glyph->uv0.x, glyph->uv1.y), color);

        indices.push_back(base_index + 0);
        indices.push_back(base_index + 1);
        indices.push_back(base_index + 2);
        indices.push_back(base_index + 0);
        indices.push_back(base_index + 2);
        indices.push_back(base_index + 3);

        cursor.x += glyph->advance;
    }

    va_.SetVBOData(vertices.data(), vertices.size() * sizeof(TextVertex));
    va_.SetIndices(indices.data(), indices.size());
}
