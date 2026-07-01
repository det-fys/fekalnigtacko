#pragma once

#ifndef EMSCRIPTEN

#include "client.hpp"
#include <memory>
#include <string>

namespace easywsclient
{
class WebSocket;
}

namespace net
{

class EasyWsClientWSClientInterface : public ClientInterface
{
public:
    EasyWsClientWSClientInterface(ClientInterfaceCallback& cb, const std::string& url);
    DELETE_COPY_MOVE(EasyWsClientWSClientInterface);

    virtual void Send(std::string_view data) override;
    virtual void Poll() override;

    virtual ~EasyWsClientWSClientInterface() override = default;

private:
    std::unique_ptr<easywsclient::WebSocket> ws_;
};

} // namespace net

#endif // EMSCRIPTEN