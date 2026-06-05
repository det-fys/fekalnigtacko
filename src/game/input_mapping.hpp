#include "player_input.hpp"
#include "vehicle.hpp"
#include "character.hpp"

namespace game
{

inline game::CharacterInputFlags MapPlayerInputToCharacterInput(game::PlayerInputFlags in)
{
    game::CharacterInputFlags c_in = 0;

    if (in & (1 << game::IN_FORWARD))
        c_in |= 1 << game::CIN_FORWARD;

    if (in & (1 << game::IN_BACKWARD))
        c_in |= 1 << game::CIN_BACKWARD;

    if (in & (1 << game::IN_LEFT))
        c_in |= 1 << game::CIN_LEFT;

    if (in & (1 << game::IN_RIGHT))
        c_in |= 1 << game::CIN_RIGHT;

    if (in & (1 << game::IN_JUMP))
        c_in |= 1 << game::CIN_JUMP;

    if (in & (1 << game::IN_SPRINT))
        c_in |= 1 << game::CIN_SPRINT;

    return c_in;
}

static game::VehicleInputFlags MapPlayerInputToVehicleInput(game::PlayerInputFlags in)
{
    game::VehicleInputFlags vin = 0;

    if (in & (1 << game::IN_FORWARD))
        vin |= 1 << game::VIN_FORWARD;

    if (in & (1 << game::IN_BACKWARD))
        vin |= 1 << game::VIN_BACKWARD;

    if (in & (1 << game::IN_LEFT))
        vin |= 1 << game::VIN_LEFT;

    if (in & (1 << game::IN_RIGHT))
        vin |= 1 << game::VIN_RIGHT;

    return vin;
}

}