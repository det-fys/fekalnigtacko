#pragma once

#include <cstddef>
#include <cstdint>

#include "fixed_str.hpp"
#include "quantized.hpp"

namespace net
{

enum MessageType : uint8_t
{
    MSG_NONE,

    /*~~~~~~~~ Client->Server ~~~~~~~~*/
    // ID <PlayerName>
    MSG_ID,

    // IN <u8, MSB=down/~up, 6..0=input type>
    MSG_IN,

    // VIEWANGLES <ViewYawQ> <ViewPitchQ>
    MSG_VIEWANGLES,

    /*~~~~~~~~ Session ~~~~~~~~*/
    // CHAT <ChatMessage>
    MSG_CHAT,

    /*~~~~~~~~ World ~~~~~~~~*/
    // CHWORLD <MapName>
    MSG_CHWORLD,

    /*~~~~~~~~ Entity ~~~~~~~~*/
    // ENTSPAWN <EntNum> <EntType> data...
    MSG_ENTSPAWN,
    // ENTMSG <EntNum> data...
    MSG_ENTMSG,
    // ENTDESTROY <EntNum>
    MSG_ENTDESTROY,
    
    // CAM <EntNum>
    MSG_CAM,

    /*~~~~~~~~~~~~~~~~*/
    MSG_COUNT,
};

using PlayerName = FixedStr<24>;
using MapName = FixedStr<32>;
using ModelName = FixedStr<64>;
using ChatMessage = FixedStr<1024>;

// pi approx fraction
constexpr long long PI_N = 245850922;
constexpr long long PI_D = 78256779;

using ViewYawQ = Quantized<uint16_t, 0, 2 * PI_N, PI_D>;
using ViewPitchQ = Quantized<uint16_t, -PI_N, PI_N, PI_D>;

using EntNum = uint16_t;

enum EntType : uint8_t
{
    ET_NONE,

    ET_CHARACTER,
    ET_VEHICLE,

    ET_COUNT,
};

enum EntMsgType : uint8_t 
{
    EMSG_NONE,

    EMSG_NAMETAG,
    EMSG_UPDATE,
};

using PositionElemQ = Quantized<uint32_t, -10000, 10000, 1>;
struct PositionQ
{
    PositionElemQ x, y, z;
};

using AngleQ = Quantized<uint16_t, -PI_N, PI_N, PI_D>;
using PositiveAngleQ = Quantized<uint16_t, 0, PI_N * 2, PI_D>;

using QuatElemQ = Quantized<uint16_t, -1, 1, 1>;
struct QuatQ
{
    QuatElemQ x, y, z;
};

using WheelZOffsetQ = Quantized<uint8_t, -1, 1, 1>;
using RotationSpeedQ = Quantized<uint16_t, -300, 300, 1>;

using ColorQ = Quantized<uint8_t, 0, 1>;

using NameTag = FixedStr<64>;

using AnimBlendQ = Quantized<uint8_t, 0, 1>;
using AnimTimeQ = Quantized<uint8_t, 0, 1>;

} // namespace net