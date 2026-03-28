#pragma once

#include "game/player_input.hpp"
#include "gui/menu.hpp"

inline bool InputToMenuInput(game::PlayerInputType in, gui::MenuInput& mi)
{
	switch (in)
	{
		case game::IN_FORWARD: mi = gui::MI_UP; return true;
		case game::IN_BACKWARD: mi = gui::MI_DOWN; return true;
		case game::IN_LEFT: mi = gui::MI_LEFT; return true;
		case game::IN_RIGHT: mi = gui::MI_RIGHT; return true;
		case game::IN_JUMP: mi = gui::MI_ENTER; return true;
		case game::IN_USE: mi = gui::MI_ENTER; return true;
		case game::IN_CROUCH: mi = gui::MI_BACK; return true;
		default: return false;
	}
};