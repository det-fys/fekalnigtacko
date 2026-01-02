#include "client.hpp"

#include "server.hpp"
#include "utils/validate.hpp"

sv::Client::Client(Server& server, WSConnId id) : server_(server), id_(id) {}

bool sv::Client::ProcessMessage(net::InMessage& msg)
{
    while (true)
    {
        net::MessageType type = net::MSG_NONE;
        if (!msg.Read(type))
            return true;

        if (type == net::MSG_NONE || type >= net::MSG_COUNT)
            return false; 

        ProcessSingleMessage(type, msg);
    }
}

bool sv::Client::ProcessSingleMessage(net::MessageType type, net::InMessage& msg)
{
    if (state_ == CS_PLAYER)
        return player_->ProcessMsg(type, msg);

    if (state_ == CS_INIT)
    {
        // allow only login
        if (type != net::MSG_ID)
            return false;
        
        net::PlayerName name;
        if (!msg.Read(name) || !utils::IsAlphanumeric(name))
            return false;
            
        player_ = std::make_unique<game::Player>(server_.GetGame(), name);
        state_ = CS_PLAYER;
        return true;
    }

    return false;
}

void sv::Client::Update()
{
    if (player_)
    {
        player_->ResetMsg();
        player_->Update();

        auto msg = player_->GetMsg();
        if (!msg.empty())
            Send(std::string(msg.data(), msg.size()));
    }
}

void sv::Client::Send(std::string msg)
{
    server_.Send(*this, std::move(msg));
}
