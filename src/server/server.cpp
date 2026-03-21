#include "server.hpp"

#include <chrono>
#include <thread>

#include <iostream>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#pragma comment(lib, "winmm.lib")
#endif

sv::Server::Server(uint16_t port) : ws_(port) {}

void sv::Server::Run()
{
    using namespace std::chrono_literals;

    srand(time(NULL));

    auto t_start = std::chrono::steady_clock::now();
    auto t_next = t_start;
    auto t_prev = t_start;

#ifdef _WIN32
    timeBeginPeriod(1);
#endif

#ifndef NDEBUG
    std::cout << "Running DEBUG build!" << std::endl;
#endif

    bool exit = false;
    while (!exit)
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
}

void sv::Server::Send(Client& client, std::string msg)
{
    ws_.Send(client.GetConnId(), std::move(msg));
}

void sv::Server::Disconnect(Client& client)
{
    ws_.Close(client.GetConnId());
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
    clients_[conn] = std::make_unique<Client>(*this, conn);
}

void sv::Server::HandleWSMessage(WSConnId conn, const std::string& data)
{
    net::InMessage msg(data.data(), data.size());
    auto& client = clients_.at(conn);
    if (!client->ProcessMessage(msg))
    {
        client->Disconnect();
    }    
}

void sv::Server::HandleWSDisconnect(WSConnId conn)
{
    clients_.erase(conn);
}

void sv::Server::Update()
{
    // update game
    game_.Update();

    // update players
    for (const auto& [conn, client] : clients_)
    {
        client->Update();
    }

    game_.FinishFrame();
}
