#pragma once

#include <vector>

#include "font.hpp"
#include "gfx/draw_list.hpp"
#include "gfx/mesh.hpp"

namespace gui
{

struct GuiRange
{
    size_t start;
    size_t count;
    gfx::TextureID texture;
};

struct Rect
{
    glm::vec2 min;
    glm::vec2 max;
};

class Context
{
public:
    Context(std::shared_ptr<const Font> default_font);

    void Begin(const glm::vec2& viewport_size);

    void PushClipRect(const glm::vec2& p0, const glm::vec2& p1);
    void PopClipRect();

    void DrawRect(const glm::vec2& p0, const glm::vec2& p1, uint32_t color, gfx::TextureID texture = 0);
    void DrawRectUV(const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& uv0, const glm::vec2& uv1,
                    uint32_t color, gfx::TextureID texture);

    void BeginGlyphs(const Font* font = nullptr);
    void DrawGlyph(glm::vec2& cursor, uint32_t cp, uint32_t color, float scale);
    float MeasureGlyph(uint32_t cp, const Font* font = nullptr) const;

    glm::vec2 MeasureText(std::string_view text, const Font* font = nullptr);
    void DrawText(std::string_view text, const glm::vec2& pos, uint32_t color = 0xFFFFFFFF, float scale = 1.0f);
    void DrawTextAligned(std::string_view text, const glm::vec2& pos, const glm::vec2& align,
                         uint32_t color = 0xFFFFFFFF, float scale = 1.0f);

    void Render(gfx::DrawList& dlist);

    const std::shared_ptr<const Font>& GetFont() const { return font_; }

    const glm::vec2& GetViewportSize() const { return viewport_size_virtual_; }
    const glm::vec2& GetRealViewportSize() const { return viewport_size_; }
    float GetScale() const { return scale_; }

private:
    void BeginTexture(gfx::TextureID texture);
    void PushRect(const glm::vec2& p0, const glm::vec2& uv0, const glm::vec2& p1, const glm::vec2& uv1, uint32_t color);
    void PushRectNoClip(const glm::vec2& p0, const glm::vec2& uv0, const glm::vec2& p1, const glm::vec2& uv1,
                        uint32_t color);

private:
    // generated mesh
    gfx::Mesh mesh_;

    // assets
    std::shared_ptr<const Font> font_;
    std::shared_ptr<const gfx::Texture> white_tex_;

    // building
    //std::vector<GuiVertex> vertices_;
    std::vector<glm::vec3> vert_pos_;
    std::vector<uint32_t> vert_colors_;
    std::vector<glm::vec2> vert_uvs_;
    std::vector<gfx::MeshTriangle> tris_;
    std::vector<GuiRange> ranges_;

    // drawing
    std::vector<Rect> clip_rects_;

    const Font* current_font_ = nullptr;

    glm::vec2 viewport_size_ = glm::vec2(1.0f);
    glm::vec2 viewport_size_virtual_ = glm::vec2(1.0f); // scale applied

    // render
    float scale_ = 1.0f;
    glm::mat3 matrix_{1.0f};
};

} // namespace gui
