#include "server_local.hpp"

static net::ConnId LOCAL_CONN_ID = 1;

net::LocalServerInterface::LocalServerInterface(std::shared_ptr<LocalChannelPair> chan) : chan_(std::move(chan))
{
    chan_->server_connected.store(true);
    connected_ = true;
}

bool net::LocalServerInterface::PollEvent(ServerInterfaceEvent& ev)
{
    if (!connected_)
    {
        ev.type = SVE_EXIT;
        return true;
    }

    auto client_connected = chan_->client_connected.load();
    if (!client_connected_ && client_connected)
    {
        client_connected_ = true;
        ev.type = SVE_CONNECTED;
        ev.conn = LOCAL_CONN_ID;
        ev.data = {};
        return true;
    }

    if (client_connected_ && !client_connected)
    {
        client_connected_ = false;
        CloseConnection(LOCAL_CONN_ID);
        ev.type = SVE_DISCONNECTED;
        ev.conn = LOCAL_CONN_ID;
        ev.data = {};
        return true;
    }

    if (client_connected_)
    {
        std::string data;
        if (chan_->client2server.Poll(data))
        {
            ev.type = SVE_MESSAGE;
            ev.conn = LOCAL_CONN_ID;
            ev.data = std::move(data);
            return true;
        }
    }

    return false;
}

void net::LocalServerInterface::Send(ConnId conn, std::string_view data)
{
    if (!connected_ || !client_connected_ || conn != LOCAL_CONN_ID)
        return;

    chan_->server2client.Send(std::string(data));
}

void net::LocalServerInterface::CloseConnection(ConnId conn)
{
    if (!connected_)
        return;

    chan_->server_connected.store(false);
    connected_ = false;
}

net::LocalServerInterface::~LocalServerInterface()
{
    CloseConnection(LOCAL_CONN_ID);
}
