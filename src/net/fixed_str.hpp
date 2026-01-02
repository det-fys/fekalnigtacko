#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace net
{

template <size_t N>
struct FixedStr
{
    size_t len = 0;
    char str[N];

    FixedStr() = default;

    FixedStr(const std::string& stdstr)
    {
        *this = stdstr;
    }
    
    FixedStr& operator=(const std::string& stdstr)
    {
        size_t putsize = std::min(N, stdstr.size());
        len = putsize;
        memcpy(str, stdstr.data(), putsize);
    }

    size_t MaxLen() const { return N; }
    
    operator std::string() { return std::string(str, len); }
    operator std::string_view() { return std::string_view(str, len); }
};

template <size_t N>
using FixedStrLen = std::conditional_t<(N > 255), uint16_t, uint8_t>;


} // namespace net