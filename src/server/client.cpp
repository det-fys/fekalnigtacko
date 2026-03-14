#include "client.hpp"

#include "server.hpp"
#include "utils/validate.hpp"
#include "utils/version.hpp"

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
        
        return ProcessLoginMsg(msg);
    }

    return false;
}

void sv::Client::Update()
{
    if (player_)
    {
        player_->Update();
        
        auto msg = player_->GetMsg();
        if (!msg.empty())
        {
            Send(std::string(msg.data(), msg.size()));
            player_->ResetMsg();
        }
    }
}

void sv::Client::SendChat(const std::string& text)
{
    // not buffering till update here (unlike in Player), sent instantly
    static std::vector<char> msg_buf;
    msg_buf.clear();
    net::OutMessage msg(msg_buf);
    msg.Write(net::MSG_CHAT);
    msg.Write(net::ChatMessage(text));

    Send(std::string(msg_buf.data(), msg_buf.size()));
}

void sv::Client::Disconnect()
{
    server_.Disconnect(*this);
}

void sv::Client::Send(std::string msg)
{
    server_.Send(*this, std::move(msg));
}

bool sv::Client::ProcessLoginMsg(net::InMessage& msg)
{
    net::Version ver = 0;
    msg.Read(ver);
    
    // check ver
    if (ver != FEKAL_VERSION)
    {
        SendChat("^f55máš nahovno verzi " + std::to_string(ver) + ", server je na " + std::to_string(FEKAL_VERSION));
        return false;
    }

    net::PlayerName name;
    if (!msg.Read(name) || !utils::IsAlphanumeric(name))
        return false;
    
    player_ = std::make_unique<game::Player>(server_.GetGame(), name);
    state_ = CS_PLAYER;
    return true;
}
