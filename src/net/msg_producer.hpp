#pragma once

#include <span>

#include "defs.hpp"
#include "outmessage.hpp"

namespace net
{

class MsgProducer
{
public:
    MsgProducer() = default;

    void ResetMsg();
    OutMessage BeginMsg(MessageType type = MSG_NONE);
    std::span<const char> GetMsg() const { return message_buf_; };

private:
    std::vector<char> message_buf_;
};

}