#include "local_channel.hpp"

void net::LocalChannel::Send(std::string data)
{
    std::lock_guard<std::mutex> lock(mtx_);
    queue_.push_back(std::move(data));
}

bool net::LocalChannel::Poll(std::string& data)
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (queue_.empty())
        return false;

    data = std::move(queue_.front());
    queue_.pop_front();
    return true;
}
