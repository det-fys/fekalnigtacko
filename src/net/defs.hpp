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

    // IN <PlayerInputFlags> <ViewYawQ> <ViewPitchQ>
    MSG_IN,

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

    /*~~~~~~~~~~~~~~~~*/
    MSG_COUNT,
};

using PlayerName = FixedStr<24>;
using MapName = FixedStr<32>;

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

    EMSG_UPDATE,
};

} // namespace net