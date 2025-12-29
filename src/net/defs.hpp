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
    // IN <PlayerInputFlags> <ViewYaw> <ViewPitch>
    MSG_IN,

    /*~~~~~~~~ World ~~~~~~~~*/
    // CHWORLD <MapName>
    MSG_CHWORLD,

    /*~~~~~~~~ Entity ~~~~~~~~*/
    // ENTSPAWN <EntNum> <EntType> data...
    MSG_ENTSPAWN,
    // ENTMSG <EntNum> data...
    MSG_ENTMSG,
    // ENTRM <EntNum>
    MSG_ENTRM,

    /*~~~~~~~~~~~~~~~~*/
    MSG_COUNT,
};

using MapName = FixedStr<32>;

using ViewYaw = Quantized<uint16_t, 0, 360>;
using ViewPitch = Quantized<uint16_t, -180, 180>;

using EntNum = uint16_t;

enum EntType : uint8_t
{
    ET_NONE,

    ET_PAWN,
    ET_CAR,

    ET_COUNT,
};

} // namespace net