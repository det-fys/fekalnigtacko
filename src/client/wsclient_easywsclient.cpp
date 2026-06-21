#ifndef EMSCRIPTEN

#include "wsclient.hpp"

#include <memory>
#include <stdexcept>

#include <easywsclient.hpp>

#ifdef _WIN32
#define NOMINMAX
#pragma comment(lib, "ws2_32")
// #pragma comment(lib, "winmm.lib")
#include <WinSock2.h>
#include <windows.h>
// #include <chrono>
// #include <thread>
#endif

using namespace easywsclient; 

static std::unique_ptr<WebSocket> s_ws;
static bool s_connected = false;

static WsConnectCallback s_on_connect;
static WsMessageCallback s_on_message;
static WsDisconnectCallback s_on_disconnect;

WsClient::WsClient()
{
#ifdef _WIN32
    INT rc;
    WSADATA wsaData;

    rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (rc)
    {
        throw std::runtime_error("WSA init failed");
    }
#endif

}

bool WsClient::Connect(const std::string& endpoint)
{
    s_ws = std::unique_ptr<WebSocket>(WebSocket::from_url(endpoint));

    if (!s_ws)
        return false;

    return true;
}

void WsClient::Send(std::span<const char> data)
{
    static std::vector<uint8_t> data_u8;
    data_u8.resize(data.size_bytes());
    memcpy(data_u8.data(), data.data(), data.size_bytes());
    s_ws->sendBinary(data_u8);
}

void WsClient::Poll()
{
    if (!s_ws)
        return;

    s_ws->poll();

    auto ws_state = s_ws->getReadyState();
    if (ws_state == WebSocket::OPEN && !s_connected)
    {
        s_connected = true;
        if (s_on_connect)
            s_on_connect();
    }
    else if (ws_state != WebSocket::OPEN && s_connected)
    {
        s_connected = false;
        if (s_on_disconnect)
            s_on_disconnect();
    }

    s_ws->dispatchBinary([&](const std::vector<uint8_t>& data_u8) {
        if (s_on_message)
        {
            std::span<const char> data(reinterpret_cast<const char*>(data_u8.data()), data_u8.size());
            s_on_message(data);
        }
    });

    if (!s_connected)
    {
        s_ws.reset();
    }
}

void WsClient::Disconnect()
{
    s_ws->close();
}

void WsClient::SetOnConnect(WsConnectCallback cb)
{
    s_on_connect = std::move(cb);
}

void WsClient::SetOnMessage(WsMessageCallback cb)
{
    s_on_message = std::move(cb);
}

void WsClient::SetOnDisconnect(WsDisconnectCallback cb)
{
    s_on_disconnect = std::move(cb);
}

WsClient::~WsClient()
{
    s_ws.reset();

#ifdef _WIN32
    WSACleanup();
#endif
}

#endif // EMSCRIPTEN