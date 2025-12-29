#pragma once

#include <cstdint>

namespace game
{
	using PlayerInputFlags = uint16_t;

	enum PlayerInputFlag : PlayerInputFlags
	{
		PI_FORWARD		= 1 << 0,
		PI_BACKWARD		= 1 << 1,
		PI_LEFT			= 1 << 2,
		PI_RIGHT		= 1 << 3,
		PI_JUMP			= 1 << 4,
		PI_CROUCH		= 1 << 5,
		PI_USE			= 1 << 6,
		PI_ATTACK		= 1 << 7,
		PI_DEBUG1		= 1 << 8,
		PI_DEBUG2		= 1 << 9,
		PI_DEBUG3		= 1 << 10,
	};
}
