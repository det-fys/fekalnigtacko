#pragma once

#include <cstdint>

namespace game
{
	using PlayerInputFlags = uint16_t;

	enum PlayerInputType : uint8_t
	{
		IN_FORWARD,
		IN_BACKWARD,
		IN_LEFT,
		IN_RIGHT,
		IN_JUMP,
		IN_CROUCH,
		IN_USE,
		IN_ATTACK,
		IN_DEBUG1,
		IN_DEBUG2,
		IN_DEBUG3,
		IN_DEBUG4,
		IN_DEBUG5,

		IN__COUNT,
	};
}
