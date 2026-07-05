#pragma once

#include <cstddef>
#include <string>
#include "net/fixed_str.hpp"

namespace utils
{

inline bool IsAlphanumeric(const char* str, size_t len)
{
    const char* end = str + len;
    for (; str < end; ++str)
    {
        const char c = *str;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'))
            return false;
    }

    return true;
}

inline bool IsAlphanumeric(std::string_view str)
{
    return IsAlphanumeric(str.data(), str.length());
}

template <size_t N>
inline bool IsAlphanumeric(const net::FixedStr<N>& str)
{
    return IsAlphanumeric(str.str, str.len);
}

}