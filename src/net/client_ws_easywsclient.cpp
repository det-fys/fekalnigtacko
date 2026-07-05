#ifndef EMSCRIPTEN

#include "client_ws_easywsclient.hpp"

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

static bool winsock_init = false;

net::EasyWsClientWSClientInterface::EasyWsClientWSClientInterface(ClientInterfaceCallback& cb, const std::string& url)
    : ClientInterface(cb)
{
#ifdef _WIN32
    if (!winsock_init)
    {
        INT rc;
        WSADATA wsaData;

        rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (rc)
        {
            throw std::runtime_error("WSA init failed");
        }
        winsock_init = true;
    }

#endif

    ws_ = std::unique_ptr<easywsclient::WebSocket>(easywsclient::WebSocket::from_url(url));

    if (ws_)
    {
        RaiseOnConnect();
    }
    else
    {
        RaiseOnDisconnect();
    }
}

void net::EasyWsClientWSClientInterface::Send(std::string_view data)
{
    if (!ws_)
        return;

    static std::vector<uint8_t> data_u8;
    data_u8.resize(data.size());
    memcpy(data_u8.data(), data.data(), data.size());
    ws_->sendBinary(data_u8);
}

void net::EasyWsClientWSClientInterface::Poll()
{
    if (!ws_)
        return;

    ws_->poll();

    ws_->dispatchBinary([&](const std::vector<uint8_t>& data_u8) {
        std::string_view data(reinterpret_cast<const char*>(data_u8.data()), data_u8.size());
        RaiseOnMessage(data);
    });

    // check disconnected
    auto ws_state = ws_->getReadyState();
    if (ws_state != easywsclient::WebSocket::OPEN)
    {
        ws_.reset();
        RaiseOnDisconnect();
        return;
    }
}

#endif // EMSCRIPTEN
