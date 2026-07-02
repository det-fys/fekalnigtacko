#pragma once

#include <deque>
#include <string>
#include <mutex>

namespace net
{

class LocalChannel
{
public:
    LocalChannel(size_t max_size);

    bool Send(std::string data);
    bool Poll(std::string& data);

private:
    std::deque<std::string> queue_;
    std::mutex mtx_;
    size_t max_size_;
};


}