#include "server.hpp"

sv::Server::Server(uint16_t port) : ws_(port) {}

void sv::Server::Run()
{
    bool exit = false;
    while (!exit)
    {
        PollWSEvents();
    }
}

void sv::Server::Send(Client& client, std::string msg)
{
    ws_.Send(client.GetConnId(), std::move(msg));
}

void sv::Server::PollWSEvents()
{
    WSEvent event;
    while (ws_.PollEvent(event))
    {
        switch (event.type)
        {
        case WSE_CONNECTED:
            HandleWSConnect(event.conn);
            break;

        case WSE_MESSAGE:
            HandleWSMessage(event.conn, event.data);
            break;

        case WSE_DISCONNECTED:
            HandleWSDisconnect(event.conn);
            break;

        case WSE_EXIT:
            exit_ = true;
            break;

        default:
            break;
        }
    }
}

void sv::Server::HandleWSConnect(WSConnId conn) 
{
    clients_[conn] = std::make_unique<Client>();
}

void sv::Server::HandleWSMessage(WSConnId conn, const std::string& data)
{
    net::InMessage msg(data.data(), data.size());
    if (!clients_.at(conn)->ProcessMessage(msg))
    {
        // TODO: disconnect
    }    
}

void sv::Server::HandleWSDisconnect(WSConnId conn)
{
    clients_.erase(conn);
}

void sv::Server::Update()
{
    // update game


    // update players
    for (const auto& [conn, client] : clients_)
    {
        client->Update();
    }
}
