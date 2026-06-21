#ifdef EMSCRIPTEN

#include "wsclient.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>

static EMSCRIPTEN_WEBSOCKET_T s_ws = 0;
static bool s_connected = false;

static WsConnectCallback s_on_connect;
static WsMessageCallback s_on_message;
static WsDisconnectCallback s_on_disconnect;

static EM_BOOL OnWSOpen(int type, const EmscriptenWebSocketOpenEvent *ev, void *ud)
{
    if (!s_connected)
    {
        s_connected = true;
        if (s_on_connect)
            s_on_connect();
    }

    return EM_TRUE;
}

static EM_BOOL OnWSMessage(int type, const EmscriptenWebSocketMessageEvent *ev, void *ud)
{
    if (ev->isText)
        return EM_TRUE;

    if (s_on_message)
    {
        std::span<const char> data(reinterpret_cast<char*>(ev->data), ev->numBytes);
        s_on_message(data);
    }

    return EM_TRUE;
}

static EM_BOOL OnWSClose(int type, const EmscriptenWebSocketCloseEvent *ev, void *ud)
{
    if (s_connected)
    {
        s_connected = false;
        if (s_on_disconnect)
        {
            s_on_disconnect();
        }
        
    }

    return EM_TRUE;
}

static EM_BOOL OnWSError(int type, const EmscriptenWebSocketErrorEvent *ev, void *ud)
{
    s_connected = false;
    if (s_on_disconnect)
    {
        s_on_disconnect();
    }
        
    return EM_TRUE;
}

WsClient::WsClient()
{
    if (!emscripten_websocket_is_supported())
    {
        throw std::runtime_error("EMSCRIPTEN WS NOT SUPPORTED");
    }
}

bool WsClient::Connect(const std::string& endpoint)
{
    if (s_ws)
    {
        emscripten_websocket_delete(s_ws);
        s_ws = 0;
    }

    EmscriptenWebSocketCreateAttributes ws_attrs = {
        endpoint.c_str(),
        NULL,
        EM_TRUE
    };

    s_ws = emscripten_websocket_new(&ws_attrs);
    emscripten_websocket_set_onopen_callback(s_ws, NULL, OnWSOpen);
    emscripten_websocket_set_onmessage_callback(s_ws, NULL, OnWSMessage);
    emscripten_websocket_set_onclose_callback(s_ws, NULL, OnWSClose);
    emscripten_websocket_set_onerror_callback(s_ws, NULL, OnWSError);

    return true;
}

void WsClient::Send(std::span<const char> data)
{
    emscripten_websocket_send_binary(s_ws, (void*)data.data(), static_cast<uint32_t>(data.size()));
}

void WsClient::Poll()
{
}

void WsClient::Disconnect()
{
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
}

#endif // EMSCRIPTEN