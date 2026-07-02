#include "local_channel.hpp"

net::LocalChannel::LocalChannel(size_t max_size) : max_size_(max_size) {}

bool net::LocalChannel::Send(std::string data)
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (queue_.size() >= max_size_)
    {
        return false;
    }

    queue_.push_back(std::move(data));
    return true;
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
