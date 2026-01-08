#pragma once

#include <cstdint>
#include <concepts>
#include <stdexcept>
#include <cstring>

#include "fixed_str.hpp"
#include "quantized.hpp"

namespace net
{

class InMessage
{
public:
    InMessage() : ptr_(nullptr), end_(nullptr) {}
    InMessage(const char* ptr, size_t len) : ptr_(ptr), end_(ptr_ + len) {}

    const char* Ptr() const { return ptr_; } 
    const char* End() const { return end_; }

    bool CheckAvail(size_t n)
    {
        return ptr_ + n <= end_;
    }

    bool Eof() const
    {
        return ptr_ < end_;
    }

    bool Read(char* dest, size_t n)
    {
        if (!CheckAvail(n))
            return false;
        
        memcpy(dest, ptr_, n);
        ptr_ += n;
        return true;
    }

    template <std::integral T>
    bool Read(T& value)
    {
        if (!CheckAvail(sizeof(T)))
            return false;

        value = *(reinterpret_cast<const T*>(ptr_));
        ptr_ += sizeof(T);
        return true;
    }

    template <typename T> requires (std::is_enum_v<T> && std::integral<std::underlying_type_t<T>>)
    bool Read(T& value)
    {
        std::underlying_type_t<T> und;
        if (!Read(und))
            return false;
        
        value = static_cast<T>(und);
        return true;
    }

    template <size_t N>
    bool Read(FixedStr<N>& str)
    {
        FixedStrLen<N> len;
 
        if (!Read(len) || len > N || !Read(str.str, len))
            return false;

        str.len = len;
        return true;
    }

    template <AnyQuantized T>
    bool Read(T& quant)
    {
        return Read(quant.value);

        //T value;
        //if (!Read(value))
        //    return false;

        //quant.value = value;
        //return true;
    }

    template <AnyQuantized T>
    bool Read(float& f)
    {
        T q;
        if (!Read(q))
            return false;

        f = q.Decode();
        return true;
    }

    bool ReadVarInt(int64_t& value)
    {
        uint64_t uvalue = 0;

        uint8_t p;
        if (!Read(p))
            return false;

        bool next = p & 0b10000000;
        bool negative = p & 0b01000000;
        uvalue |= p & 0b00111111;

        size_t shift = 6;

        while (next)
        {
            if (!Read(p))
                return false;

            next = p & 0b10000000;
            uvalue |= static_cast<uint64_t>(p & 0b01111111) << shift;
            shift += 7;

            
        }

        value = static_cast<int64_t>(uvalue);
        if (negative)
            value = -value;
        
        return true;
    }

    bool SubMessage(InMessage& sub, size_t n)
    {
        if (!CheckAvail(n))
            return false;

        sub.ptr_ = ptr_;
        ptr_ += n;
        sub.end_ = ptr_;
        return true;
    }


private:
    const char* ptr_;
    const char* end_;
};

} // namespace net
