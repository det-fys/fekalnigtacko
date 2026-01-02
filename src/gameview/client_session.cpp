#include "client_session.hpp"

game::view::ClientSession::ClientSession(App& app) : app_(app) {}

bool game::view::ClientSession::ProcessMessage(net::InMessage& msg)
{
    while (true)
    {
        net::MessageType type = net::MSG_NONE;
        if (!msg.Read(type))
            break;

        if (type == net::MSG_NONE || type >= net::MSG_COUNT)
            return false;

        ProcessSingleMessage(type, msg);
    }
}

bool game::view::ClientSession::ProcessSingleMessage(net::MessageType type, net::InMessage& msg)
{
    switch (type)
    {
    case net::MSG_CHWORLD:
        return ProcessWorldMsg(msg);

    default:
        // try pass the msg to world
        if (world_ && world_->ProcessMsg(type, msg))
            return true;

        return false;
    }
}

bool game::view::ClientSession::ProcessWorldMsg(net::InMessage& msg)
{
    net::MapName mapname;
    if (!msg.Read(mapname))
        return false;

    // TODO: pass mapname
    world_ = std::make_unique<WorldView>();

    return true;
}
