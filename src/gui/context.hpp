#pragma once

#include "font.hpp"
#include "gfx/vertex_array.hpp"
#include "gfx/draw_list.hpp"

namespace gui
{

struct GuiVertex
{
    glm::vec3 pos;
    uint32_t color;
    glm::vec2 uv;
};

struct GuiRange
{
    size_t start;
    size_t count;
    const gfx::Texture* texture;
};

struct Rect
{
    glm::vec2 min;
    glm::vec2 max;
};

class Context
{
public:
    Context(gfx::DrawList& dlist, std::shared_ptr<const Font> default_font);

    void Begin();

    glm::vec2 MeasureText(std::string_view text);
    void DrawText(std::string_view text, const glm::vec2& pos, uint32_t color = 0xFFFFFFFF, float scale = 1.0f);

    void Render();

    const std::shared_ptr<const Font>& GetFont() const { return font_; }
    
private:
    void BeginTexture(const gfx::Texture* texture);
    void PushRect(const glm::vec2& p0, const glm::vec2& uv0, const glm::vec2& p1, const glm::vec2& uv1, uint32_t color);

private:
    gfx::DrawList& dlist_;
    std::shared_ptr<const Font> font_;
    gfx::VertexArray va_;

    // building
    std::vector<GuiVertex> vertices_;
    std::vector<uint32_t> indices_;
    std::vector<GuiRange> ranges_;

    
};



}