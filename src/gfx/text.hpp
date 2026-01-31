#pragma once

#include "font.hpp"
#include "vertex_array.hpp"

namespace gfx
{

class Text
{
public:
    Text(std::shared_ptr<const Font> font, uint32_t color);

    void SetText(const char* text);
    void SetText(const std::string& text) { SetText(text.c_str()); }

    const std::shared_ptr<const Font>& GetFont() const { return font_; }
    const VertexArray& GetVA() const { return va_; }
    const glm::vec2& GetSize() const { return size_; }

private:
    std::shared_ptr<const Font> font_;
    VertexArray va_;
    uint32_t color_;
    glm::vec2 size_; // size of the text in pixels
};

} // namespace gfx