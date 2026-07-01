#pragma once

#include <string_view>
#include <functional>

#include "utils/defs.hpp"

namespace net
{

class ClientInterfaceCallback
{
public:
    virtual void OnClientConnect() = 0;
    virtual void OnClientMessage(std::string_view data) = 0;
    virtual void OnClientDisconnect() = 0;
};

class ClientInterface
{
public:
    ClientInterface(ClientInterfaceCallback& cb);

    virtual void Send(std::string_view data) = 0;
    virtual void Poll() = 0;

    virtual ~ClientInterface() = default;

protected:
    void RaiseOnConnect();
    void RaiseOnMessage(std::string_view data);
    void RaiseOnDisconnect();

private:
    ClientInterfaceCallback& cb_;

};

};