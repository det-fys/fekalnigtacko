#include "msg_producer.hpp"

#include <cstdint>

void net::MsgProducer::ResetMsg()
{
    message_buf_.clear();
}

net::OutMessage net::MsgProducer::BeginMsg(net::MessageType type)
{
    OutMessage msg(message_buf_);
    if (type != net::MSG_NONE)
        msg.Write(type);
    return msg;
}
