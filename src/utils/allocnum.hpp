#pragma once

#include <concepts>

namespace utils
{

template <std::unsigned_integral TNum, typename TMap>
TNum AllocNum(const TMap& map, TNum& num)
{
    constexpr auto MAX_NUM = std::numeric_limits<TNum>().max();
    if (ents_.size() >= MAX_NUM - 2) // 0 & MAX reserved
        return 0;

    // this is stupid but whatever
    do
    {
        ++num;
    } while (num == 0 || num == MAX_ENTNUM || ents_.find(num) != ents_.end());

    return num;
}

}