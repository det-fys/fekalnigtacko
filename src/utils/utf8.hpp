#pragma once

#include <string_view>
#include <string>

inline uint32_t DecodeUTF8Codepoint(std::string_view& sv)
{
    if (sv.empty())
        return 0;

    const unsigned char* p = reinterpret_cast<const unsigned char*>(sv.data());

    if ((p[0] & 0b10000000) == 0)
    { // 1-byte sequence
        uint32_t cp = p[0];
        sv.remove_prefix(1);
        return cp;
    }

    uint32_t codepoint = 0;

    if ((p[0] & 0b11100000) == 0b11000000)
    { // 2-byte seq
        if (sv.size() < 2)
            return 0;
        codepoint = (p[0] & 0b00011111) << 6;
        codepoint |= (p[1] & 0b00111111);
        sv.remove_prefix(2);
    }
    else if ((p[0] & 0b11110000) == 0b11100000)
    { // 3-byte seq
        if (sv.size() < 3)
            return 0;
        codepoint = (p[0] & 0b00001111) << 12;
        codepoint |= (p[1] & 0b00111111) << 6;
        codepoint |= (p[2] & 0b00111111);
        sv.remove_prefix(3);
    }
    else if ((p[0] & 0b11111000) == 0b11110000)
    { // 4-byte seq
        if (sv.size() < 4)
            return 0;
        codepoint = (p[0] & 0b00000111) << 18;
        codepoint |= (p[1] & 0b00111111) << 12;
        codepoint |= (p[2] & 0b00111111) << 6;
        codepoint |= (p[3] & 0b00111111);
        sv.remove_prefix(4);
    }

    return codepoint;
}

inline void EncodeUTF8Codepoint(std::string& str, uint32_t cp)
{
    // Replace invalid codepoints with U+FFFD
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        cp = 0xFFFD;

    if (cp <= 0x7F)
    {
        str.push_back(static_cast<char>(cp));
    }
    else if (cp <= 0x7FF)
    {
        str.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        str.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else if (cp <= 0xFFFF)
    {
        str.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        str.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        str.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else
    {
        str.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        str.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        str.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        str.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}