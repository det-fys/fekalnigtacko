#pragma once

#include <vector>

template <class T>
inline void BufferPut(std::vector<char>& buffer, const T& val)
{
    const char* data = reinterpret_cast<const char*>(&val);
    buffer.insert(buffer.end(), data, data + sizeof(T));
}
