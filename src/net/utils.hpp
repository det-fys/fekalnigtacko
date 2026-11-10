#pragma once

#include "defs.hpp"
#include "inmessage.hpp"
#include "outmessage.hpp"
#include "utils/transform.hpp"

namespace net
{

/// TRANSFORMS

inline void WritePosition(OutMessage& msg, const glm::vec3& pos)
{
    msg.Write<PositionElemQ>(pos.x);
    msg.Write<PositionElemQ>(pos.y);
    msg.Write<PositionElemQ>(pos.z);
}

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

inline void WriteFarPosition(OutMessage& msg, const glm::dvec3& pos)
{
    msg.Write<FarPositionElemQ>(pos.x);
    msg.Write<FarPositionElemQ>(pos.y);
    msg.Write<FarPositionElemQ>(pos.z);
}

inline void EncodeFarPosition(const glm::dvec3& pos, FarPositionQ& out)
{
    out.x.Encode(pos.x);
    out.y.Encode(pos.y);
    out.z.Encode(pos.z);
}

inline void WriteFarPositionQ(OutMessage& msg, const FarPositionQ& posq)
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
    const auto q = glm::normalize(quat);
    // Use x/y/z/w order for the component index stored on the wire.
    const float components[4] = {q.x, q.y, q.z, q.w};
    uint8_t largest = 0;
    for (uint8_t i = 1; i < 4; ++i)
        if (glm::abs(components[i]) > glm::abs(components[largest]))
            largest = i;

    out.i = largest | (components[largest] < 0.0f ? 0x4 : 0x0);
    QuatElemQ* encoded[3] = {&out.x, &out.y, &out.z};
    for (uint8_t i = 0, j = 0; i < 4; ++i)
        if (i != largest)
            encoded[j++]->Encode(components[i]);
}

inline void WriteRotationQ(OutMessage& msg, const QuatQ& rotq)
{
    msg.Write(rotq.i);
    msg.Write(rotq.x.value);
    msg.Write(rotq.y.value);
    msg.Write(rotq.z.value);
}

// inline void WriteTransform(OutMessage& msg, const Transform& trans)
// {
//     WritePosition(msg, trans.position);
//     WriteRotation(msg, trans.rotation);
// }

inline bool ReadPosition(InMessage& msg, glm::vec3& pos)
{
    return msg.Read<PositionElemQ>(pos.x) && msg.Read<PositionElemQ>(pos.y) && msg.Read<PositionElemQ>(pos.z);
}

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

inline bool ReadFarPosition(InMessage& msg, glm::dvec3& pos)
{
    return msg.Read<FarPositionElemQ>(pos.x) && msg.Read<FarPositionElemQ>(pos.y) && msg.Read<FarPositionElemQ>(pos.z);
}

inline bool ReadFarPositionQ(InMessage& msg, FarPositionQ& posq)
{
    return msg.Read(posq.x.value) && msg.Read(posq.y.value) && msg.Read(posq.z.value);
}

inline void DecodeFarPosition(const FarPositionQ& posq, glm::dvec3& out)
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
    return msg.Read(rotq.i) && msg.Read(rotq.x.value) && msg.Read(rotq.y.value) && msg.Read(rotq.z.value);
}

inline void DecodeRotation(const QuatQ& rotq, glm::quat& out)
{
    const uint8_t largest = rotq.i & 0x3;
    const bool largestNegative = (rotq.i & 0x4) != 0;
    const float decoded[3] = {rotq.x.Decode(), rotq.y.Decode(), rotq.z.Decode()};
    float components[4]{};
    float sumSquares = 0.0f;
    for (uint8_t i = 0, j = 0; i < 4; ++i)
    {
        if (i == largest)
            continue;
        components[i] = decoded[j++];
        sumSquares += components[i] * components[i];
    }
    components[largest] = glm::sqrt(glm::max(0.0f, 1.0f - sumSquares)) * (largestNegative ? -1.0f : 1.0f);
    out = glm::normalize(glm::quat(components[3], components[0], components[1], components[2]));
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

// COLOR 24bit
inline void WriteRGB(OutMessage& msg, uint32_t color)
{
    msg.Write<uint8_t>(color & 0xFF);
    msg.Write<uint8_t>((color >> 8) & 0xFF);
    msg.Write<uint8_t>((color >> 16) & 0xFF);
}

inline bool ReadRGB(InMessage& msg, uint32_t& color)
{
    uint8_t r, g, b;
    if (!msg.Read(r) || !msg.Read(g) || !msg.Read(b))
        return false;

    color = (b << 16) | (g << 8) | r;
    return true;
}


// DELTA
template <std::unsigned_integral T> requires (sizeof(T) == 1)
inline void WriteDelta(OutMessage& msg, T current, T previous)
{
    // 1 byte => just write current
    msg.Write(current);
}

template <std::unsigned_integral T> requires (sizeof(T) > 1)
inline void WriteDelta(OutMessage& msg, T current, T previous)
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
    WriteDelta(msg, current.value, previous.value);
}

template <std::unsigned_integral T> requires (sizeof(T) == 1)
inline bool ReadDelta(InMessage& msg, T& current, T previous)
{
    // 1 byte => just read current
    return msg.Read(current);
}

template <std::unsigned_integral T> requires (sizeof(T) > 1)
inline bool ReadDelta(InMessage& msg, T& current, T previous)
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
