#pragma once

#include <SDL.h>
#include <imgui.h>
#include "utils/keys.hpp"

KeyCode GetKeyCodeFromSDLScancode(SDL_Scancode scancode);
KeyCode GetKeyCodeFromImGuiKey(ImGuiKey key);
