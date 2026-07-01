#pragma once

#include <atomic>

#include "local_channel.hpp"

namespace net
{

struct LocalChannelPair
{
    LocalChannel client2server;
    LocalChannel server2client;

    std::atomic_bool client_connected;
    std::atomic_bool server_connected;
};

}