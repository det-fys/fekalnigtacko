#pragma once

#include <string>
#include <span>
#include <functional>

using WsConnectCallback = std::function<void()>;
using WsMessageCallback = std::function<void(std::span<const char> data)>;
using WsDisconnectCallback = std::function<void()>;

class WsClient
{
public:
    WsClient();

    bool Connect(const std::string& endpoint);
    void Send(std::span<const char> data);
    void Poll();
    void Disconnect();

    void SetOnConnect(WsConnectCallback cb);
    void SetOnMessage(WsMessageCallback cb);
    void SetOnDisconnect(WsDisconnectCallback cb);

    ~WsClient();
};

