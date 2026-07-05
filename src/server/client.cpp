#include "client.hpp"

#include "server.hpp"
#include "utils/validate.hpp"
#include "utils/version.hpp"
#include "utils/chatcolors.hpp"
#include "utils/cvars.hpp"

CVAR(uint8_t, sv_require_alphanumeric_names, CV_NONE, 0, 0, 1);

sv::Client::Client(Server& server, net::ConnId id) : server_(server), id_(id) {}

bool sv::Client::ProcessMessage(net::InMessage& msg)
{
    while (true)
    {
        net::MessageType type = net::MSG_NONE;
        if (!msg.Read(type))
            return true;

        if (type == net::MSG_NONE || type >= net::MSG_COUNT || !ProcessSingleMessage(type, msg))
        {
            SendChat("^f55obdržena podivná zpráva typu " + std::to_string(type));
            return false;
        }
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

    // read "token": TODO: really token
    net::PlayerName name_token;
    if (!msg.Read(name_token))
        return false;

    if (sv_require_alphanumeric_names.Get() > 0 && !utils::IsAlphanumeric(name_token))
    {
        SendChat(COL_ERROR "chyba: máš nahovno jméno - musí bejt písmena, čísla nebo _");
        return false;
    }

    auto& db = server_.GetGame().GetDb();

    // login
    db::PlayerId player_id;
    auto res = db.VerifyPlayer(name_token, player_id);
    if (res != db::QR_OK)
    {
        SendChat(COL_ERROR "chyba při přihlašování: " + std::string(db::GetResultDescription(res)));
        return false;
    }

    player_ = std::make_unique<game::Player>(server_.GetGame(), player_id);
    state_ = CS_PLAYER;
    return true;
}
