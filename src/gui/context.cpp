#include "context.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>

#include "assets/asset_manager.hpp"
#include "utils/utf8.hpp"
#include "utils/cvars.hpp"

CVAR_CL(float, ui_scale, CV_SAVE, 1.0f, 0.1f, 2.0f);

static gfx::MeshDescriptor GetMeshDesc()
{
    gfx::MeshDescriptor desc{};
    desc.attributes = gfx::MESH_VERTEX_ATTR_POSITION | gfx::MESH_VERTEX_ATTR_COLOR | gfx::MESH_VERTEX_ATTR_UV0;
    desc.dynamic = true;
    desc.use_index_buffer = true;
    return desc;
}

gui::Context::Context(std::shared_ptr<const Font> default_font) : mesh_(GetMeshDesc()), font_(std::move(default_font))
{
    white_tex_ = assets::AssetManager::GetInstance().Get<gfx::Texture>("white");
}

void gui::Context::Begin(const glm::vec2& viewport_size)
{
    vert_pos_.clear();
    vert_colors_.clear();
    vert_uvs_.clear();
    tris_.clear();
    ranges_.clear();

    clip_rects_.clear();

    viewport_size_ = viewport_size;
    scale_ = ui_scale.Get();
    viewport_size_virtual_ = viewport_size_ / scale_;
}

void gui::Context::PushClipRect(const glm::vec2& p0, const glm::vec2& p1)
{
    Rect rect{p0, p1};

    if (!clip_rects_.empty())
    {
        const auto& clip_rect = clip_rects_.back();
        rect.min = glm::max(rect.min, clip_rect.min);
        rect.max = glm::min(rect.max, clip_rect.max);
    }

    clip_rects_.push_back(rect);
}

void gui::Context::PopClipRect()
{
    if (!clip_rects_.empty())
        clip_rects_.pop_back();
}

void gui::Context::DrawRect(const glm::vec2& p0, const glm::vec2& p1, uint32_t color, gfx::TextureID texture)
{
    BeginTexture(texture ? texture : white_tex_->GetID());
    PushRect(p0, glm::vec2(0.0f), p1, glm::vec2(1.0f), color);
}

void gui::Context::DrawRectUV(const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& uv0, const glm::vec2& uv1,
                              uint32_t color, gfx::TextureID texture)
{
    BeginTexture(texture ? texture : white_tex_.get()->GetID());
    PushRect(p0, uv0, p1, uv1, color);
}

void gui::Context::BeginGlyphs(const Font* font)
{
    if (!font)
        font = font_.get();

    BeginTexture(font->GetTexture()->GetID());
    current_font_ = font;
}

void gui::Context::DrawGlyph(glm::vec2& cursor, uint32_t cp, uint32_t color, float scale)
{
    if (cp == ' ')
    {
        cursor.x += current_font_->GetSpaceSize() * scale;
        return;
    }

    const FontGlyphData* glyph = current_font_->GetCodepointGlyph(cp);

    if (!glyph)
    {
        return;
    }

    glm::vec2 p0 = cursor + glyph->offset * scale;
    glm::vec2 p1 = p0 + glyph->size * scale;

    PushRect(p0, glyph->uv0, p1, glyph->uv1, color);
    
    cursor.x += glyph->advance * scale;
}

float gui::Context::MeasureGlyph(uint32_t cp, const Font* font) const
{
    if (!font)
    {
        font = font_.get();
    }

    if (cp == ' ')
    {
        return font->GetSpaceSize();
    }

    const FontGlyphData* glyph = font->GetCodepointGlyph(cp);

    if (!glyph)
    {
        return 0.0f;
    }

    return glyph->advance;
}

glm::vec2 gui::Context::MeasureText(std::string_view text, const Font* font)
{
    if (!font)
    {
        font = font_.get();
    }

    glm::vec2 size(0.0f);

    if (text.empty())
        return size;

    uint32_t cp = 0;
    const float line_height = font->GetLineHeight();

    glm::vec2 cursor(0.0f);

    while ((cp = DecodeUTF8Codepoint(text)))
    {
        if (cp == '\n')
        {
            size.x = glm::max(size.x, cursor.x);
            cursor.x = 0.0f;
            cursor.y += line_height;
            continue;
        }
        else if (cp == '^')
        {
            if (!(cp = DecodeUTF8Codepoint(text)))
                break;

            if (cp == 'r')
                continue;

            // parse color
            for (size_t i = 0; i < 3; ++i)
            {
                if (!(cp = DecodeUTF8Codepoint(text)))
                    break;
            }
        }

        cursor.x += MeasureGlyph(cp, font);
    }

    size.x = glm::max(size.x, cursor.x);
    size.y = cursor.y + line_height;

    return size;
}

void gui::Context::DrawText(std::string_view text, const glm::vec2& pos, uint32_t color, float scale)
{
    if (text.empty())
        return;

    BeginGlyphs(font_.get());

    uint32_t cp = 0;
    const float line_height = font_->GetLineHeight() * scale;

    glm::vec2 cursor = pos;

    if (scale == 1.0f)
        cursor = glm::floor(cursor);

    uint32_t curr_color = color;

    while ((cp = DecodeUTF8Codepoint(text)))
    {
        if (cp == '\n')
        {
            cursor.x = 0.0f;
            cursor.y += line_height;
            continue;
        }
        else if (cp == '^')
        {
            if (!(cp = DecodeUTF8Codepoint(text)))
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
                    ch = 0;

                curr_color |= (ch << 16);
                curr_color |= (ch << 20);

                if (!(cp = DecodeUTF8Codepoint(text)))
                    break;
            }

            curr_color |= (color & 0xFF000000); // preserve alpha
        }

        DrawGlyph(cursor, cp, curr_color, scale);
    }
}

void gui::Context::DrawTextAligned(std::string_view text, const glm::vec2& pos, const glm::vec2& align, uint32_t color, float scale)
{
    auto size = MeasureText(text) * scale;
    DrawText(text, pos + size * align, color, scale);
}

void gui::Context::Render(gfx::DrawList& dlist)
{
    const glm::mat3* matrix_ptr = nullptr;
    if (glm::epsilonNotEqual(scale_, 1.0f, 0.01f))
    {
        matrix_ = glm::mat3(1.0f);
        matrix_[0][0] = scale_;
        matrix_[1][1] = scale_;
        matrix_ptr = &matrix_;
    }

    gfx::MeshVertexData vertex_data{};
    vertex_data.count = vert_pos_.size();
    vertex_data.position = vert_pos_;
    vertex_data.color = vert_colors_;
    vertex_data.uv0 = vert_uvs_;
    mesh_.SetVertexData(vertex_data);

    gfx::MeshTriangleData tri_data{};
    tri_data.triangles = tris_;
    mesh_.SetTriangleData(tri_data);

    for (const auto& range : ranges_)
    {
        gfx::DrawHudCmd hudcmd{};
        hudcmd.mesh = mesh_.GetID();
        hudcmd.texture = range.texture;
        hudcmd.tri_offset = range.start;
        hudcmd.tri_count = range.count;
        hudcmd.matrix = matrix_ptr;
        dlist.AddHUD(hudcmd);
    }
}

void gui::Context::BeginTexture(gfx::TextureID texture)
{
    if (!ranges_.empty())
    {
        if (ranges_.back().texture == texture)
            return;

        if (ranges_.back().count == 0)
            ranges_.pop_back();
    } 

    auto& range = ranges_.emplace_back();
    range.start = tris_.size();
    range.count = 0;
    range.texture = texture;
}

void gui::Context::PushRect(const glm::vec2& p0, const glm::vec2& uv0, const glm::vec2& p1, const glm::vec2& uv1,
                            uint32_t color)
{
    if (!clip_rects_.empty())
    {
        const auto& clip_rect = clip_rects_.back();

        // is completely outside?
        if (p1.x < clip_rect.min.x || p0.x > clip_rect.max.x || p1.y < clip_rect.min.y || p0.y > clip_rect.max.y)
        {
            return;
        }

        // needs clip?
        if (p0.x < clip_rect.min.x || p1.x > clip_rect.max.x || p0.y < clip_rect.min.y || p1.y > clip_rect.max.y)
        {
            auto new_p0 = p0;
            auto new_p1 = p1;
            auto new_uv0 = uv0;
            auto new_uv1 = uv1;

            auto uv_matters = ranges_.back().texture != white_tex_.get()->GetID();

            if (new_p0.x < clip_rect.min.x)
            {
                if (uv_matters)
                {
                    float t = (clip_rect.min.x - new_p0.x) / (new_p1.x - new_p0.x);
                    new_uv0.x = glm::mix(uv0.x, uv1.x, t);
                }
                new_p0.x = clip_rect.min.x;
            }

            if (new_p1.x > clip_rect.max.x)
            {
                if (uv_matters)
                {
                    float t = (clip_rect.max.x - new_p0.x) / (new_p1.x - new_p0.x);
                    new_uv1.x = glm::mix(uv0.x, uv1.x, t);
                }
                new_p1.x = clip_rect.max.x;
            }

            if (new_p0.y < clip_rect.min.y)
            {
                if (uv_matters)
                {
                    float t = (clip_rect.min.y - new_p0.y) / (new_p1.y - new_p0.y);
                    new_uv0.y = glm::mix(uv0.y, uv1.y, t);
                }
                new_p0.y = clip_rect.min.y;
            }

            if (new_p1.y > clip_rect.max.y)
            {
                if (uv_matters)
                {
                    float t = (clip_rect.max.y - new_p0.y) / (new_p1.y - new_p0.y);
                    new_uv1.y = glm::mix(uv0.y, uv1.y, t);
                }
                new_p1.y = clip_rect.max.y;
            }

            PushRectNoClip(new_p0, new_uv0, new_p1, new_uv1, color);
            return;
        }
    }

    PushRectNoClip(p0, uv0, p1, uv1, color);
}

void gui::Context::PushRectNoClip(const glm::vec2& p0, const glm::vec2& uv0, const glm::vec2& p1, const glm::vec2& uv1,
                                  uint32_t color)
{
    uint32_t base_index = vert_pos_.size();

    vert_pos_.emplace_back(p0.x, p0.y, 0.0f);
    vert_pos_.emplace_back(p1.x, p0.y, 0.0f);
    vert_pos_.emplace_back(p1.x, p1.y, 0.0f);
    vert_pos_.emplace_back(p0.x, p1.y, 0.0f);

    vert_colors_.emplace_back(color);
    vert_colors_.emplace_back(color);
    vert_colors_.emplace_back(color);
    vert_colors_.emplace_back(color);

    vert_uvs_.emplace_back(uv0);
    vert_uvs_.emplace_back(uv1.x, uv0.y);
    vert_uvs_.emplace_back(uv1);
    vert_uvs_.emplace_back(uv0.x, uv1.y);

    tris_.emplace_back(base_index + 0, base_index + 1, base_index + 2);
    tris_.emplace_back(base_index + 0, base_index + 2, base_index + 3);

    ranges_.back().count += 2; // 2 tris
}
