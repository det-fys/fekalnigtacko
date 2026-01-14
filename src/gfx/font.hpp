#pragma once

#include <array>
#include <map>
#include <memory>

#include <glm/glm.hpp>

#include "texture.hpp"

namespace gfx
{

using Codepoint = uint32_t;

template <class T>
class CodepointMap
{
    static constexpr size_t MAX_ARRAY_CODEPOINT = 128;

    struct ArrayItem
    {
        T data;
        bool used = false;
    };

public:
    CodepointMap() = default;

    T& Alloc(Codepoint codepoint)
    {
        if (codepoint < MAX_ARRAY_CODEPOINT)
        {
            array_[codepoint].used = true;
            return array_[codepoint].data;
        }
        else
        {
            return map_[codepoint]; // Use map for larger codepoints
        }
    }

    const T* Get(Codepoint codepoint) const
    {
        if (codepoint < MAX_ARRAY_CODEPOINT)
        {
            if (array_[codepoint].used)
            {
                return &array_[codepoint].data;
            }
        }
        else
        {
            auto it = map_.find(codepoint);
            if (it != map_.end())
            {
                return &it->second;
            }
        }

        return nullptr; // Not found
    }

private:
    std::array<ArrayItem, MAX_ARRAY_CODEPOINT> array_; // For common ASCII codepoints
    std::map<Codepoint, T> map_;
};

struct FontGlyphData
{
    glm::vec2 uv0;
    glm::vec2 uv1;
    glm::vec2 offset;
    glm::vec2 size;
    float advance = 0.0f; // Advance width for the codepoint
};

class Font
{
public:
    Font() = default;
    static std::shared_ptr<const Font> LoadFromFile(const std::string& path);

    const FontGlyphData* GetCodepointGlyph(Codepoint codepoint) const
    {
        const FontGlyphData* data = glyphs_.Get(codepoint);
        return data ? data : glyphs_.Get(0); // try return missing codepoint glyph if missing
    }

    const std::shared_ptr<const Texture>& GetTexture() const { return texture_; }

    float GetLineHeight() const { return line_height_; }

private:
    std::shared_ptr<const Texture> texture_; // Texture atlas for the font
    CodepointMap<FontGlyphData> glyphs_;

    float line_height_ = 0.0f; // Line height in pixels, used for text layout
};

} // namespace gfx