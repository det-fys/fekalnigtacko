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

net::OutMessage net::LocalMsgProducer::BeginLocalMsg(const glm::vec3& position, float radius, MessageType type)
{
    LocalMsgInfo& local_msg = local_msgs_.emplace_back();
    local_msg.start = local_msg_buf_.size();
    local_msg.position = position;
    local_msg.radius = radius;

    OutMessage msg(local_msg_buf_);
    if (type != net::MSG_NONE)
        msg.Write(type);
    return msg;
}

void net::LocalMsgProducer::PickLocalMsgs(MsgProducer& target, const glm::vec3& target_pos)
{
    for (size_t i = 0; i < local_msgs_.size(); ++i)
    {
        const auto& local_msg = local_msgs_[i];

        auto d = local_msg.position - target_pos;
        if (glm::dot(d, d) > (local_msg.radius * local_msg.radius))
            continue;

        auto start = local_msgs_[i].start;
        auto end = (i + 1) < local_msgs_.size() ? local_msgs_[i + 1].start : local_msg_buf_.size();

        auto msg = target.BeginMsg();
        msg.Write(std::span<char>(&local_msg_buf_[start], end - start));
    }
}

void net::LocalMsgProducer::ResetLocalMsgs()
{
    local_msgs_.clear();
    local_msg_buf_.clear();
}
