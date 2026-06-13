#pragma once

#include <span>

#include "defs.hpp"
#include "outmessage.hpp"

#include <glm/glm.hpp>

namespace net
{

class MsgProducer
{
public:
    MsgProducer() = default;

    void ResetMsg();
    OutMessage BeginMsg(MessageType type = MSG_NONE);
    void DiscardMsg();
    std::span<const char> GetMsg() const { return message_buf_; };

private:
    std::vector<char> message_buf_;
    size_t msg_start_ = 0;
};

struct LocalMsgInfo
{
    size_t start;
    glm::vec3 position;
    float radius;
};

class LocalMsgProducer
{
public:
    LocalMsgProducer() = default;

    OutMessage BeginLocalMsg(const glm::vec3& position, float radius, MessageType type = MSG_NONE);
    void PickLocalMsgs(MsgProducer& target, const glm::vec3& target_pos);
    void ResetLocalMsgs();

private:
    std::vector<LocalMsgInfo> local_msgs_;
    std::vector<char> local_msg_buf_;

};

}