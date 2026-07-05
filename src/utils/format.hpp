#pragma once

#include <string>
#include <cstdint>

inline std::string FormatBalance(int64_t balance)
{
    auto str = std::to_string(balance / 100);
    str += ",";

    str += ('0' + ((balance / 10) % 10));
    str += ('0' + (balance % 10));

    str += " Kč";

    return str;
}
