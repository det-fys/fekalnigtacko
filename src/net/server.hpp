#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "utils/defs.hpp"

namespace net
{

using ConnId = uint32_t;

enum ServerInterfaceEventType
{
    SVE_NONE,
    SVE_EXIT,
    SVE_CONNECTED,
    SVE_MESSAGE,
    SVE_DISCONNECTED,
};

struct ServerInterfaceEvent
{
    ServerInterfaceEventType type;
    ConnId conn;
    std::string data;
};

class ServerInterface
{
public:
    virtual bool PollEvent(ServerInterfaceEvent& ev) = 0;
    virtual void Send(ConnId conn, std::string_view data) = 0;
    virtual void CloseConnection(ConnId conn) = 0;

    virtual ~ServerInterface() = default;
};

} // namespace net
