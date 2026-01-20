#include "msg_producer.hpp"

#include <cstdint>

void net::MsgProducer::ResetMsg()
{
    message_buf_.clear();
}

net::OutMessage net::MsgProducer::BeginMsg(net::MessageType type)
{
    msg_start_ = message_buf_.size();
    OutMessage msg(message_buf_);
    if (type != net::MSG_NONE)
        msg.Write(type);
    return msg;
}

void net::MsgProducer::DiscardMsg()
{
    message_buf_.resize(msg_start_);
}
