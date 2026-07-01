#pragma once

#include <deque>
#include <string>
#include <mutex>

namespace net
{

class LocalChannel
{
public:
    LocalChannel() = default;

    void Send(std::string data);
    bool Poll(std::string& data);

private:
    std::deque<std::string> queue_;
    std::mutex mtx_;

};


}