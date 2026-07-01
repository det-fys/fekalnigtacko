#include "client.hpp"

net::ClientInterface::ClientInterface(ClientInterfaceCallback& cb) : cb_(cb) {}

void net::ClientInterface::RaiseOnConnect()
{
    cb_.OnClientConnect();
}

void net::ClientInterface::RaiseOnMessage(std::string_view data)
{
    cb_.OnClientMessage(data);
}

void net::ClientInterface::RaiseOnDisconnect()
{
    cb_.OnClientDisconnect();
}
