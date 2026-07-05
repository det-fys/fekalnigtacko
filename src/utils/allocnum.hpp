#pragma once

#include <concepts>
#include <limits>

namespace utils
{

template <std::unsigned_integral TNum, typename TMap>
TNum AllocNum(const TMap& map, TNum& num)
{
    constexpr auto MAX_NUM = std::numeric_limits<TNum>().max();
    if (map.size() >= MAX_NUM - 2) // 0 & MAX reserved
        return 0;

    // this is stupid but whatever
    do
    {
        ++num;
    } while (num == 0 || num == MAX_NUM || map.find(num) != map.end());

    return num;
}

}