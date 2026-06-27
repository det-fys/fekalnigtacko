#pragma once

#include <cstdint>

namespace game
{
	using PlayerInputFlags = uint32_t;

	enum PlayerInputType : uint8_t
	{
		IN_FORWARD,
		IN_BACKWARD,
		IN_LEFT,
		IN_RIGHT,
		IN_JUMP,
		IN_CROUCH,
		IN_SPRINT,
		IN_USE,
		IN_ATTACK_PRIMARY,
		IN_ATTACK_SECONDARY,
		IN_HOLSTER,
		IN_RELOAD,
		IN_AIM_MODE,
		IN_WEAPON_1,
		IN_WEAPON_2,
		IN_WEAPON_3,
		IN_WEAPON_4,
		IN_WEAPON_5,
		IN_WEAPON_6,
		IN_WEAPON_7,
		IN_WEAPON_8,
		IN_WEAPON_9,
		IN_WEAPON_0,
		IN_DEBUG1,
		IN_DEBUG2,
		IN_DEBUG3,
		IN_DEBUG4,
		IN_DEBUG5,
		IN_MENU,

		IN__COUNT,
	};
}
