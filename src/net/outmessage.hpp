#pragma once

#include <concepts>
#include <vector>
#include <span>
#include <cstring>

#include "fixed_str.hpp"
#include "quantized.hpp"

namespace net
{

class OutMessage
{
public:
    OutMessage(std::vector<char>& buffer) : buffer_(buffer) { }

    template <std::integral T>
    size_t Reserve()
    {
        size_t pos = buffer_.size();
        buffer_.resize(pos + sizeof(T));
        return pos;
    }

    template <std::integral T>
    void WriteAt(size_t pos, T value)
    {
        *(reinterpret_cast<T*>(&buffer_[pos])) = value;
    }

    template <std::integral T>
    void Write(T value)
    {
        WriteAt(Reserve<T>(), value);
    }

    template <typename T> requires (std::is_enum_v<T> && std::integral<std::underlying_type_t<T>>)
    void Write(T value)
    {
        Write(static_cast<std::underlying_type_t<T>>(value));
    }

    void Write(const char* str, size_t n)
    {
        if (n == 0)
            return;

        size_t pos = buffer_.size();
        buffer_.resize(pos + n);
        memcpy(&buffer_[pos], str, n);
    }

    void Write(std::span<const char> data)
    {
        Write(data.data(), data.size());
    }

    template <size_t N>
    void Write(const FixedStr<N>& str)
    {
        Write(static_cast<FixedStrLen<N>>(str.len));
        Write(str.str, str.len);
    }

    // template <typename T, long long MinL, long long MaxL>
    // void Write(Quantized<T, MinL, MaxL> quant)
    // {
    //     Write(quant.value);
    // }

    template <AnyQuantized T>
    void Write(float f)
    {
        T q;
        q.Encode(f);
        Write(q.value);
    }

    // template <AnyQuantized T>
    // void Write(T quant)
    // {
    //     Write(quant.value);
    // }

    void WriteVarInt(int64_t value)
    {
        const bool negative = value < 0;
        uint64_t uvalue = static_cast<uint64_t>(negative ? -value : value);

        char p[10];
        p[0] = negative ? 0b11000000 : 0b10000000;
        p[0] |= uvalue & 0b00111111;
        uvalue >>= 6;
        
        size_t i;
        for (i = 1; uvalue; ++i)
        {
            p[i] = 0b10000000 | (uvalue & 0b01111111);
            uvalue >>= 7;
        }

        p[i - 1] &= 0b01111111; // end mark

        Write(p, i);
    }

private:
    std::vector<char>& buffer_;
};

} // namespace net
