#ifdef EMSCRIPTEN

#include "client_ws_emscripten.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>

static net::EmscriptenWSClientInterface* GetInterfaceFromUd(void* ud)
{
    return static_cast<net::EmscriptenWSClientInterface*>(ud);
}

net::EmscriptenWSClientInterface::EmscriptenWSClientInterface(ClientInterfaceCallback& cb, const std::string& url)
    : ClientInterface(cb), ws_(0)
{
    if (!emscripten_websocket_is_supported())
    {
        throw std::runtime_error("Emscripten WebSocket not supported!");
    }

    EmscriptenWebSocketCreateAttributes ws_attrs = {url.c_str(), NULL, EM_TRUE};

    ws_ = emscripten_websocket_new(&ws_attrs);
    if (ws_ <= 0)
    {
        ws_ = 0;
        throw std::runtime_error("Failed to create Emscripten WebSocket");
    }

    emscripten_websocket_set_onopen_callback(ws_, this, [](int type, const EmscriptenWebSocketOpenEvent *ev, void *ud) {
        GetInterfaceFromUd(ud)->RaiseOnConnect();
        return EM_TRUE;
    });

    emscripten_websocket_set_onmessage_callback(ws_, this, [](int type, const EmscriptenWebSocketMessageEvent *ev, void *ud) {
        GetInterfaceFromUd(ud)->RaiseOnMessage(std::string_view((const char*)ev->data, ev->numBytes));
        return EM_TRUE;
    });

    emscripten_websocket_set_onclose_callback(ws_, this, [](int type, const EmscriptenWebSocketCloseEvent *ev, void *ud) {
        GetInterfaceFromUd(ud)->RaiseOnDisconnect();
        return EM_TRUE;
    });

    emscripten_websocket_set_onerror_callback(ws_, this, [](int type, const EmscriptenWebSocketErrorEvent *ev, void *ud) {
        GetInterfaceFromUd(ud)->RaiseOnDisconnect();
        return EM_TRUE;
    });
}

void net::EmscriptenWSClientInterface::Send(std::string_view data)
{
    if (!ws_)
    {
        throw std::runtime_error("Not connected");
    }

    emscripten_websocket_send_binary(ws_, (void*)data.data(), data.size());
}

void net::EmscriptenWSClientInterface::Poll()
{
    // Nothing to do here, Emscripten handles the polling internally
}

net::EmscriptenWSClientInterface::~EmscriptenWSClientInterface()
{
    if (ws_)
    {
        emscripten_websocket_close(ws_, 1000, "Normal Closure");
        emscripten_websocket_delete(ws_);
        ws_ = 0;
    }
}

#endif // EMSCRIPTEN
