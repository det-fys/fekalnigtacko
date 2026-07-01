#include "client_local.hpp"

net::LocalClientInterface::LocalClientInterface(ClientInterfaceCallback& cb, std::shared_ptr<LocalChannelPair> chan)
    : ClientInterface(cb), chan_(std::move(chan))
{
    chan_->client_connected.store(true);
}

void net::LocalClientInterface::Send(std::string_view data)
{
    if (!connected_)
        return;
    
    chan_->client2server.Send(std::string(data));
}

void net::LocalClientInterface::Poll()
{
    bool server_connected = chan_->server_connected.load();

    if (server_connected && !connected_)
    {
        RaiseOnConnect();
        connected_ = true;
    }
    else if (!server_connected && connected_)
    {
        RaiseOnDisconnect();
        connected_ = false;
    }

    if (connected_)
    {
        std::string data;
        while (chan_->server2client.Poll(data))
        {
            RaiseOnMessage(data);
        }
    }
}

net::LocalClientInterface::~LocalClientInterface()
{
    chan_->client_connected.store(false);
}
