#pragma once

#include "defs.hpp"
#include "inmessage.hpp"
#include "outmessage.hpp"
#include "utils/transform.hpp"

namespace net
{

/// TRANSFORMS

// inline void WritePosition(OutMessage& msg, const glm::vec3& pos)
// {
//     msg.Write<PositionQ>(pos.x);
//     msg.Write<PositionQ>(pos.y);
//     msg.Write<PositionQ>(pos.z);
// }

inline void EncodePosition(const glm::vec3& pos, PositionQ& out)
{
    out.x.Encode(pos.x);
    out.y.Encode(pos.y);
    out.z.Encode(pos.z);
}

inline void WritePositionQ(OutMessage& msg, const PositionQ& posq)
{
    msg.Write(posq.x.value);
    msg.Write(posq.y.value);
    msg.Write(posq.z.value);
}

// inline void WriteRotation(OutMessage& msg, const glm::quat& quat)
// {
//     auto q = glm::normalize(quat);
//     if (q.w < 0.0f)
//         q = -q;

//     msg.Write<QuatQ>(q.x);
//     msg.Write<QuatQ>(q.y);
//     msg.Write<QuatQ>(q.z);
// }

inline void EncodeRotation(const glm::quat& quat, QuatQ& out)
{
    auto q = glm::normalize(quat);
    if (q.w < 0.0f)
        q = -q;

    out.x.Encode(q.x);
    out.y.Encode(q.y);
    out.z.Encode(q.z);
}

inline void WriteRotationQ(OutMessage& msg, const QuatQ& rotq)
{
    msg.Write(rotq.x.value);
    msg.Write(rotq.y.value);
    msg.Write(rotq.z.value);
}

// inline void WriteTransform(OutMessage& msg, const Transform& trans)
// {
//     WritePosition(msg, trans.position);
//     WriteRotation(msg, trans.rotation);
// }

// inline bool ReadPosition(InMessage& msg, glm::vec3& pos)
// {
//     return msg.Read<PositionQ>(pos.x) && msg.Read<PositionQ>(pos.y) && msg.Read<PositionQ>(pos.z);
// }

inline bool ReadPositionQ(InMessage& msg, PositionQ& posq)
{
    return msg.Read(posq.x.value) && msg.Read(posq.y.value) && msg.Read(posq.z.value);
}

inline void DecodePosition(const PositionQ& posq, glm::vec3& out)
{
    out.x = posq.x.Decode();
    out.y = posq.y.Decode();
    out.z = posq.z.Decode();
}

// inline bool ReadRotation(InMessage& msg, glm::quat& q)
// {
//     glm::vec3 v;
//     if (!msg.Read<QuatQ>(v.x) || !msg.Read<QuatQ>(v.y) || !msg.Read<QuatQ>(v.z))
//         return false;

//     float w = glm::sqrt(glm::max(0.0f, 1.0f - glm::dot(v, v)));
//     q = glm::quat(w, v.x, v.y, v.z);

//     return true;
// }

inline bool ReadRotationQ(InMessage& msg, QuatQ& rotq)
{
    return msg.Read(rotq.x.value) && msg.Read(rotq.y.value) && msg.Read(rotq.z.value);
}

inline void DecodeRotation(const QuatQ& rotq, glm::quat& out)
{
    glm::vec3 v(rotq.x.Decode(), rotq.y.Decode(), rotq.z.Decode());
    float w = glm::sqrt(glm::max(0.0f, 1.0f - glm::dot(v, v)));
    out = glm::quat(w, v.x, v.y, v.z);
}

// inline bool ReadTransform(InMessage& msg, Transform& trans)
// {
//     return ReadPosition(msg, trans.position) && ReadRotation(msg, trans.rotation);
// }

/// COLOR

inline void WriteRGB(OutMessage& msg, const glm::vec3& color)
{
    msg.Write<ColorQ>(color.r);
    msg.Write<ColorQ>(color.g);
    msg.Write<ColorQ>(color.b);
}

inline bool ReadRGB(InMessage& msg, glm::vec3& color)
{
    return msg.Read<ColorQ>(color.r) && msg.Read<ColorQ>(color.g) && msg.Read<ColorQ>(color.b);
}

// DELTA
template <std::unsigned_integral T>
inline void WriteDelta(OutMessage& msg, T previous, T current)
{
    static_assert(sizeof(T) <= 4);

    int64_t delta = static_cast<int64_t>(current) - static_cast<int64_t>(previous);

    constexpr int64_t wrap = 1LL << (sizeof(T) * 8);
    delta = (delta + wrap / 2) % wrap - wrap / 2;

    msg.WriteVarInt(delta);
}

template <AnyQuantized T>
inline void WriteDelta(OutMessage& msg, T current, T previous)
{
    WriteDelta(msg, previous.value, current.value);
}

template <std::unsigned_integral T>
inline bool ReadDelta(InMessage& msg, T previous, T& current)
{
    static_assert(sizeof(T) <= 4);

    int64_t encoded;
    if (!msg.ReadVarInt(encoded))
        return false;

    constexpr uint64_t mask = (1ULL << (sizeof(T) * 8)) - 1;
    current = static_cast<T>((static_cast<uint64_t>(previous) + static_cast<uint64_t>(encoded)) & mask);
    return true;
}

template <std::unsigned_integral T>
inline bool ReadDelta(InMessage& msg, T& value)
{
    return ReadDelta<T>(msg, value, value);
}

template <AnyQuantized T>
inline bool ReadDelta(InMessage& msg, T& quant)
{
    return ReadDelta(msg, quant.value, quant.value);
}

} // namespace net