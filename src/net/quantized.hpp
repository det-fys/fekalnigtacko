#pragma once

#include <algorithm>
#include <limits>
#include <concepts>

namespace net
{

template <std::unsigned_integral T, long long MinL = -1, long long MaxL = 1, long long DivisorL = 1>
struct Quantized
{
    static constexpr float Min = static_cast<float>(MinL) / static_cast<float>(DivisorL);
    static constexpr float Max = static_cast<float>(MaxL) / static_cast<float>(DivisorL);

    static constexpr T max_int = std::numeric_limits<T>::max();
    static constexpr float range = Max - Min;
    static constexpr float scale = static_cast<float>(max_int) / range;
    static constexpr float inv_scale = range / static_cast<float>(max_int);

public:
    T value;

    Quantized() = default;
    Quantized(T value) : value(value) {}
    Quantized(float fvalue) { Encode(value); }

    void Encode(float fvalue) noexcept
    {
        fvalue = std::clamp(fvalue, Min, Max);

        float normalized = (fvalue - Min) * scale;
        value = static_cast<T>(normalized + 0.5f); // round
    }

    float Decode() const noexcept { return Min + static_cast<float>(value) * inv_scale; }

    static constexpr float MaxError() noexcept { return inv_scale * 0.5f; }
};

template <typename T>
concept AnyQuantized = requires(T t, float f)
{
    { T(f) };
    { t.Encode(f) };
    { t.Decode() } -> std::convertible_to<float>;
    // { t.value } -> std::unsigned_integral;
    { t.value };
};

} // namespace net