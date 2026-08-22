#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility> // for std::swap

namespace mg
{

struct Triangle
{
    std::array<uint32_t, 3> verts;

    Triangle() = default;
    Triangle(uint32_t a, uint32_t b, uint32_t c) : verts{a, b, c} {}

    uint32_t& operator[](size_t idx) { return verts[idx]; }
    const uint32_t& operator[](size_t idx) const { return verts[idx]; }

    void Sort()
    {
        if (verts[0] > verts[1])
            std::swap(verts[0], verts[1]);
        if (verts[1] > verts[2])
            std::swap(verts[1], verts[2]);
        if (verts[0] > verts[1])
            std::swap(verts[0], verts[1]);
    }
};

// inline bool operator==(const Triangle& lhs, const Triangle& rhs)
//{
//     return lhs[0] == rhs[0] && lhs[1] == rhs[1] && lhs[2] == rhs[2];
// }

inline auto operator<=>(const Triangle& lhs, const Triangle& rhs)
{
    return std::tie(lhs[0], lhs[1], lhs[2]) <=> std::tie(rhs[0], rhs[1], rhs[2]);
}

} // namespace mg
