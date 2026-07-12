#include "spz_texture.hpp"

#include <vector>
#include <tuple>
#include <span>

#include <glm/glm.hpp>

#include "utils/image.hpp"

constexpr size_t SPZ_ATLAS_WIDTH = 256;
constexpr size_t SPZ_ATLAS_HEIGHT = 256;

constexpr size_t SPZ_TEXTURE_WIDTH = 192;
constexpr size_t SPZ_TEXTURE_HEIGHT = 64;

static const std::vector<uint32_t>& GetSpzAtlasData()
{
    static std::vector<uint32_t> data;

    if (data.empty())
    {
        auto img = LoadImage("data/spz.png");

        if (img.width != SPZ_ATLAS_WIDTH || img.height != SPZ_ATLAS_HEIGHT)
        {
            throw std::runtime_error("Invalid spz atlas dimensions!");
        }

        auto data_view = std::span<const uint32_t>{reinterpret_cast<const uint32_t*>(img.data.data()), SPZ_ATLAS_WIDTH * SPZ_ATLAS_HEIGHT};
        data.assign(data_view.begin(), data_view.end());
    }

    return data;
}

game::view::SpzTexture::SpzTexture()
{
    gfx::TextureDescriptor texture_desc{};
    texture_desc.width = SPZ_TEXTURE_WIDTH;
    texture_desc.height = SPZ_TEXTURE_HEIGHT;
    texture_desc.filter = gfx::TEXTURE_FILTER_NEAREST;
    texture_desc.mipmaps = gfx::TEXTURE_MIPMAP_TYPE_NONE;
    texture_ = std::make_shared<gfx::Texture>(texture_desc);

    gfx::MaterialInfo material_info{};
    material_info.texture = texture_;
    material_.emplace(material_info);

    Render("", 0, 0, 0);
}

static void CopyImg(int atlas_x, int atlas_y, int width, int height, std::vector<uint32_t>& dst, int dst_x, int dst_y,
                    uint32_t tint = 0xFFFFFFFF)
{
    auto& atlas = GetSpzAtlasData();

    if (atlas.empty() || dst.empty() || width <= 0 || height <= 0)
        return;

    // tint channels
    uint8_t tt_a = static_cast<uint8_t>(tint >> 24);
    uint8_t tt_r = static_cast<uint8_t>(tint >> 16);
    uint8_t tt_g = static_cast<uint8_t>(tint >> 8);
    uint8_t tt_b = static_cast<uint8_t>(tint);

    for (int y = 0; y < height; ++y)
    {
        int ay = atlas_y + y;
        int dy = dst_y + y;
        if (ay < 0 || dy < 0)
            continue;
        for (int x = 0; x < width; ++x)
        {
            int ax = atlas_x + x;
            int dx = dst_x + x;
            if (ax < 0 || dx < 0)
                continue;

            size_t aidx = static_cast<size_t>(ay) * SPZ_ATLAS_WIDTH + static_cast<size_t>(ax);
            size_t didx = static_cast<size_t>(dy) * SPZ_TEXTURE_WIDTH + static_cast<size_t>(dx);
            if (aidx >= atlas.size() || didx >= dst.size())
                continue;

            uint32_t src = atlas[aidx];
            uint8_t sa = static_cast<uint8_t>(src >> 24);
            uint8_t sr = static_cast<uint8_t>(src >> 16);
            uint8_t sg = static_cast<uint8_t>(src >> 8);
            uint8_t sb = static_cast<uint8_t>(src);

            // Apply tint (multiply channels and alpha by tint components)
            if (tint != 0xFFFFFFFF)
            {
                sr = static_cast<uint8_t>((static_cast<int>(sr) * tt_r + 127) / 255);
                sg = static_cast<uint8_t>((static_cast<int>(sg) * tt_g + 127) / 255);
                sb = static_cast<uint8_t>((static_cast<int>(sb) * tt_b + 127) / 255);
                sa = static_cast<uint8_t>((static_cast<int>(sa) * tt_a + 127) / 255);
            }

            if (sa == 0)
                continue; // fully transparent after tint
            if (sa == 255)
            {
                dst[didx] = (static_cast<uint32_t>(sa) << 24) | (static_cast<uint32_t>(sr) << 16) |
                            (static_cast<uint32_t>(sg) << 8) | static_cast<uint32_t>(sb);
                continue;
            }

            uint32_t dstpix = dst[didx];
            uint8_t da = static_cast<uint8_t>(dstpix >> 24);
            uint8_t dr = static_cast<uint8_t>(dstpix >> 16);
            uint8_t dg = static_cast<uint8_t>(dstpix >> 8);
            uint8_t db = static_cast<uint8_t>(dstpix);

            int inva = 255 - sa;

            auto blend_chan = [&](int s, int d) -> uint8_t {
                return static_cast<uint8_t>((s * sa + d * inva + 127) / 255);
            };

            uint8_t orr = blend_chan(sr, dr);
            uint8_t org = blend_chan(sg, dg);
            uint8_t orb = blend_chan(sb, db);
            uint8_t oa = static_cast<uint8_t>(sa + (da * inva + 127) / 255);

            dst[didx] = (static_cast<uint32_t>(oa) << 24) | (static_cast<uint32_t>(orr) << 16) |
                        (static_cast<uint32_t>(org) << 8) | static_cast<uint32_t>(orb);
        }
    }
}

static void CopyChar(char c, std::vector<uint32_t>& dst, int center_x, int offset_x, int offset_y, uint32_t color)
{
    int char_num = (c >= '0' && c <= '9') ? (c - '0') : ((c >= 'A' && c <= 'Z') ? (c - 'A' + 10) : 0);
    int col = char_num % 8;
    int row = char_num / 8;

    static constexpr int padding_x = 5;
    static constexpr int padding_y = 5;

    static constexpr int size_x = 32 - (padding_x * 2);
    static constexpr int size_y = 32 - (padding_y * 2);

    int src_x = col * 32 + padding_x;
    int src_y = row * 32 + padding_y;

    int dst_x = offset_x + center_x - size_x / 2;
    int dst_y = offset_y;
    
    CopyImg(src_x, src_y, size_x, size_y, dst, dst_x, dst_y, color);
}

void game::view::SpzTexture::Render(std::string_view text, uint32_t color_mount, uint32_t color_bg, uint32_t color_fg)
{
    std::vector<uint32_t> data;
    data.resize(SPZ_TEXTURE_WIDTH * SPZ_TEXTURE_HEIGHT, color_mount);

    // plate
    CopyImg(7, 162, 160, 28, data, 16, 16, color_bg);

    if (text.size() > 8)
        text = text.substr(0, 8);

    // 1X1 1111
    if (text.size() == 7)
    {
        // circles
        CopyImg(179, 163, 11, 26, data, 86, 17);

        int cursor_x = 25;
        for (const auto& c : text.substr(0, 3))
        {
            CopyChar(c, data, cursor_x, 16, 19, color_fg);
            cursor_x += 16;
        }

        cursor_x = 95;
        for (const auto& c : text.substr(3, 4))
        {
            CopyChar(c, data, cursor_x, 16, 19, color_fg);
            cursor_x += 16;
        }
    }
    else if (text.size() == 8) // custom
    {
        // circles
        CopyImg(179, 163, 11, 26, data, 79, 17);

        int cursor_x = 22;
        for (const auto& c : text.substr(0, 3))
        {
            CopyChar(c, data, cursor_x, 16, 19, color_fg);
            cursor_x += 16;
        }

        cursor_x = 82;
        for (const auto& c : text.substr(3, 5))
        {
            CopyChar(c, data, cursor_x, 16, 19, color_fg);
            cursor_x += 16;
        }
    }
    else // ??
    {
        int cursor_x = 10;
        for (const auto& c : text)
        {
            CopyChar(c, data, cursor_x, 16, 19, color_fg);
            cursor_x += 16;
        }
    }

    texture_->SetData({reinterpret_cast<const uint8_t*>(data.data()), data.size() * sizeof(data[0])});
}
