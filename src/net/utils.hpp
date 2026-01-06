#pragma once

#include "defs.hpp"
#include "outmessage.hpp"
#include "inmessage.hpp"
#include "utils/transform.hpp"

namespace net
{

inline void WritePosition(OutMessage& msg, const glm::vec3& pos)
{
    msg.Write<PositionQ>(pos.x);
    msg.Write<PositionQ>(pos.y);
    msg.Write<PositionQ>(pos.z);
}

inline void WriteRotation(OutMessage& msg, const glm::quat& quat)
{
    auto q = glm::normalize(quat);
    if (q.w < 0.0f)
        q = -q;

    msg.Write<QuatQ>(q.x);
    msg.Write<QuatQ>(q.y);
    msg.Write<QuatQ>(q.z);
}

inline void WriteTransform(OutMessage& msg, const Transform& trans)
{
    WritePosition(msg, trans.position);
    WriteRotation(msg, trans.rotation);
}

inline bool ReadPosition(InMessage& msg, glm::vec3& pos)
{
    return msg.Read<PositionQ>(pos.x) && msg.Read<PositionQ>(pos.y) && msg.Read<PositionQ>(pos.z);
}

inline bool ReadRotation(InMessage& msg, glm::quat& q)
{
    glm::vec3 v;
    if (!msg.Read<QuatQ>(v.x) || !msg.Read<QuatQ>(v.y) || !msg.Read<QuatQ>(v.z))
        return false;

    float w = glm::sqrt(glm::max(0.0f, 1.0f - glm::dot(v, v)));
    q = glm::quat(w, v.x, v.y, v.z);
}

inline bool ReadTransform(InMessage& msg, Transform& trans)
{
    return ReadPosition(msg, trans.position) && ReadRotation(msg, trans.rotation);
}

}