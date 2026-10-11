#pragma once

#include <algorithm>
#include <limits>
#include <concepts>

namespace net
{

template <std::floating_point S, std::unsigned_integral T, long long MinL = -1, long long MaxL = 1, long long DivisorL = 1>
struct BasicQuantized
{
    using ScalarType = S;
    using IntType = T;

    static constexpr S Min = static_cast<S>(MinL) / static_cast<S>(DivisorL);
    static constexpr S Max = static_cast<S>(MaxL) / static_cast<S>(DivisorL);

    static constexpr T max_int = std::numeric_limits<T>::max();
    static constexpr S range = Max - Min;
    static constexpr S scale = static_cast<S>(max_int) / range;
    static constexpr S inv_scale = range / static_cast<S>(max_int);

public:
    T value{0};

    BasicQuantized() = default;
    BasicQuantized(T value) : value(value) {}
    BasicQuantized(S fvalue) { Encode(fvalue); }

    void Encode(S fvalue) noexcept
    {
        fvalue = std::clamp(fvalue, Min, Max);

        S normalized = (fvalue - Min) * scale;
        value = static_cast<T>(normalized + static_cast<S>(0.5)); // round
    }

    S Decode() const noexcept { return Min + static_cast<S>(value) * inv_scale; }

    static constexpr S MaxError() noexcept { return inv_scale * static_cast<S>(0.5); }
};

template <typename S, typename T>
concept AnyBasicQuantized = requires(T t, S f)
{
    { T(f) };
    { t.Encode(f) };
    { t.Decode() } -> std::convertible_to<S>;
    // { t.value } -> std::unsigned_integral;
    { t.value };
};

template <std::unsigned_integral T, long long MinL = -1, long long MaxL = 1, long long DivisorL = 1>
using Quantized = BasicQuantized<float, T, MinL, MaxL, DivisorL>;

template <std::unsigned_integral T, long long MinL = -1, long long MaxL = 1, long long DivisorL = 1>
using Quantized64 = BasicQuantized<double, T, MinL, MaxL, DivisorL>;

template <typename T>
concept AnyQuantized32 = AnyBasicQuantized<float, T>;

template <typename T>
concept AnyQuantized64 = AnyBasicQuantized<double, T>;

template <typename T>
concept AnyQuantized = AnyQuantized32<T> || AnyQuantized64<T>;

} // namespace net