#include "server.hpp"

#include <chrono>
#include <thread>

#include <iostream>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#pragma comment(lib, "winmm.lib")
#endif

sv::Server::Server(ServerInfo info) : interface_(std::move(info.iface)), game_(std::move(info.game)) {}

void sv::Server::Run()
{
    using namespace std::chrono_literals;

    auto t_start = std::chrono::steady_clock::now();
    auto t_next = t_start;
    auto t_prev = t_start;

#ifdef _WIN32
    timeBeginPeriod(1);
#endif

#ifndef NDEBUG
    std::cout << "Running DEBUG build!" << std::endl;
#else
    std::cout << "Running RELEASE build!" << std::endl;
#endif

    while (!exit_)
    {
        auto t_now = std::chrono::steady_clock::now();
        while (t_now > t_next && !exit_)
        {
            time_ += 40;
            PollWSEvents();
            Update();
            // std::cout << "Time: " << time_ << " ms, Clients: " << clients_.size() << std::endl;
            t_next += 40ms;
            t_now = std::chrono::steady_clock::now();
        }

        while (t_now < t_next && !exit_)
        {
            std::this_thread::sleep_for(t_next - t_now);

            if (clients_.empty())
            {
                std::this_thread::sleep_for(1000ms); // sleep extra if no clients
            }
            t_now = std::chrono::steady_clock::now();
        }
    }

    std::cout << "Server shut down" << std::endl;
}

void sv::Server::Send(Client& client, std::string msg)
{
    interface_->Send(client.GetConnId(), std::move(msg));
}

void sv::Server::Disconnect(Client& client)
{
    interface_->CloseConnection(client.GetConnId());
}

void sv::Server::SetPlayerConnected(db::PlayerId player_id, bool connected)
{
    if (connected)
    {
        connected_players_.insert(player_id);
    }
    else
    {
        connected_players_.erase(player_id);
    }

}

bool sv::Server::IsPlayerConnected(db::PlayerId player_id) const
{
    return connected_players_.contains(player_id);
}

void sv::Server::PollWSEvents()
{
    net::ServerInterfaceEvent event;
    while (interface_->PollEvent(event))
    {
        switch (event.type)
        {
        case net::SVE_CONNECTED:
            HandleWSConnect(event.conn);
            break;

        case net::SVE_MESSAGE:
            HandleWSMessage(event.conn, event.data);
            break;

        case net::SVE_DISCONNECTED:
            HandleWSDisconnect(event.conn);
            break;

        case net::SVE_EXIT:
            exit_ = true;
            return;

        default:
            break;
        }
    }
}

void sv::Server::HandleWSConnect(net::ConnId conn) 
{
    clients_[conn] = std::make_unique<Client>(*this, conn);
}

void sv::Server::HandleWSMessage(net::ConnId conn, const std::string& data)
{
    net::InMessage msg(data.data(), data.size());
    auto& client = clients_.at(conn);
    if (!client->ProcessMessage(msg))
    {
        client->Disconnect();
    }    
}

void sv::Server::HandleWSDisconnect(net::ConnId conn)
{
    clients_.erase(conn);
}

void sv::Server::Update()
{
    // update game
    game_->Update();

    // update players
    for (const auto& [conn, client] : clients_)
    {
        client->Update();
    }

    game_->FinishFrame();
}
