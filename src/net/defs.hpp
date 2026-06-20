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

    // MENUACTION <MenuId> <MenuActionType> ... 
    MSG_MENUACTION,

    /*~~~~~~~~ Session ~~~~~~~~*/
    // CHAT <ChatMessage>
    MSG_CHAT,

    /*~~~~~~~~ World ~~~~~~~~*/
    // CHWORLD <MapName>
    MSG_CHWORLD,

    // ENV <DayTimeQ>
    MSG_ENV,

    // CAM <EntNum>
    MSG_CAM,

    // USETARGET ...
    MSG_USETARGET,

    // REMOTEMENU <MenuId> <MenuMessageType> ...
    MSG_REMOTEMENU,

    // HUD ...
    MSG_HUD,

    // DAMAGE ...
    MSG_DAMAGE,

    /*~~~~~~~~ Entity ~~~~~~~~*/
    // ENTSPAWN <EntNum> <EntType> data...
    MSG_ENTSPAWN,
    // ENTMSG <EntNum> data...
    MSG_ENTMSG,
    // UPDATEENTS <EntCount> ...
    MSG_UPDATEENTS,
    // ENTDESTROY <EntNum>
    MSG_ENTDESTROY,
    
    /*~~~~~~~~ Destructibles ~~~~~~~~*/
    // OBJDESTROY <ObjNum>
    MSG_OBJDESTROY,
    // OBJRESPAWN <ObjNum>
    MSG_OBJRESPAWN,
    
    /*~~~~~~~~ Effects ~~~~~~~~*/
    // BEAM <Position start> <Position end> <Color> <BeamTime>
    MSG_BEAM,

    // FX <ModelName effect> <Position> <Dir x> <Dir y> <Dir z>
    MSG_FX,

    /*~~~~~~~~~~~~~~~~*/
    MSG_COUNT,
};

using PlayerName = FixedStr<64>;
using MapName = FixedStr<32>;
using ModelName = FixedStr<64>;
using ChatMessage = FixedStr<1024>;

// pi approx fraction
constexpr long long PI_N = 245850922;
constexpr long long PI_D = 78256779;

using ViewYawQ = Quantized<uint16_t, 0, 2 * PI_N, PI_D>;
using ViewPitchQ = Quantized<uint16_t, -PI_N, PI_N, PI_D>;

// env
using DayTimeQ = Quantized<uint16_t, 0, 24>;

// entities
using EntNum = uint16_t;
using EntCount = EntNum;

enum EntType : uint8_t
{
    ET_NONE,

    ET_SIMPLE,
    ET_CHARACTER,
    ET_VEHICLE,
    ET_MARKER,
    
    ET_COUNT,
};

enum EntMsgType : uint8_t 
{
    EMSG_NONE,

    EMSG_NAMETAG,
    EMSG_ATTACH,
    // EMSG_UPDATE, // deprecated
    EMSG_PLAYSOUND,
    EMSG_DEFORM,
    EMSG_TUNING,
    EMSG_EQUIP,
    EMSG_FIRE,
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

using SoundName = FixedStr<64>;
using SoundVolumeQ = Quantized<uint8_t, 0, 2>;
using SoundPitchQ = Quantized<uint8_t, 0, 2>;

using AnimBlendQ = Quantized<uint8_t, 0, 1>;
using AnimPhaseQ = Quantized<uint8_t, 0, 1>;
using AnimTimeQ = Quantized<uint16_t, 0, 255>;

using AnimAimAngleQ = Quantized<uint16_t, -PI_N, PI_N, PI_D>;

using NumClothes = uint8_t;
using ClothesName = FixedStr<32>;

// destructibles
using ObjNum = uint16_t;
using ObjCount = ObjNum;

using NumTexels = uint16_t;

// version
using Version = uint32_t;

// tuning
using TuningPartIdx = uint8_t;

// use target
using UseTargetName = FixedStr<128>;
using UseDelayQ = Quantized<uint8_t, 0, 10>;

// remote menu

enum MenuMessageType
{
    MMSG_CREATE,
    MMSG_UPDATE,
    MMSG_ITEM_UPDATE_TEXT,
    MMSG_ITEM_UPDATE_SELECTION,
    MMSG_SET_HOVER,
    MMSG_CLOSE,
};

using MenuId = uint8_t;
using MenuTitle = FixedStr<64>;

using MenuItemId = uint8_t;
using MenuItemCount = MenuItemId;

using MenuItemText = FixedStr<64>;
using MenuItemSelection = FixedStr<64>;

// menu actions

enum MenuActionType
{
    MA_CLICK,
    MA_SELECT,
    MA_HOVER,
    MA_EXIT,
};

using MenuSelectDir = uint8_t; // 0=left, 1=right

using BeamTimeQ = Quantized<uint8_t, 0, 10>;

using DirQ = Quantized<uint8_t, -1, 1>;

} // namespace net