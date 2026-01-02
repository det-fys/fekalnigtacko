#pragma once

#include <cstdint>

namespace game
{
	using PlayerInputFlags = uint16_t;

	enum PlayerInputFlag : PlayerInputFlags
	{
		IN_FORWARD		= 1 << 0,
		IN_BACKWARD		= 1 << 1,
		IN_LEFT			= 1 << 2,
		IN_RIGHT		= 1 << 3,
		IN_JUMP			= 1 << 4,
		IN_CROUCH		= 1 << 5,
		IN_USE			= 1 << 6,
		IN_ATTACK		= 1 << 7,
		IN_DEBUG1		= 1 << 8,
		IN_DEBUG2		= 1 << 9,
		IN_DEBUG3		= 1 << 10,
	};
}
