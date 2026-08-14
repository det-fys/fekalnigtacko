#include "key_map.hpp"

#include <map>

static std::map<SDL_Scancode, KeyCode> sdl_to_kc = {
    { SDL_SCANCODE_UNKNOWN, KEY_NONE },

    { SDL_SCANCODE_A, KEY_A },
    { SDL_SCANCODE_B, KEY_B },
    { SDL_SCANCODE_C, KEY_C },
    { SDL_SCANCODE_D, KEY_D },
    { SDL_SCANCODE_E, KEY_E },
    { SDL_SCANCODE_F, KEY_F },
    { SDL_SCANCODE_G, KEY_G },
    { SDL_SCANCODE_H, KEY_H },
    { SDL_SCANCODE_I, KEY_I },
    { SDL_SCANCODE_J, KEY_J },
    { SDL_SCANCODE_K, KEY_K },
    { SDL_SCANCODE_L, KEY_L },
    { SDL_SCANCODE_M, KEY_M },
    { SDL_SCANCODE_N, KEY_N },
    { SDL_SCANCODE_O, KEY_O },
    { SDL_SCANCODE_P, KEY_P },
    { SDL_SCANCODE_Q, KEY_Q },
    { SDL_SCANCODE_R, KEY_R },
    { SDL_SCANCODE_S, KEY_S },
    { SDL_SCANCODE_T, KEY_T },
    { SDL_SCANCODE_U, KEY_U },
    { SDL_SCANCODE_V, KEY_V },
    { SDL_SCANCODE_W, KEY_W },
    { SDL_SCANCODE_X, KEY_X },
    { SDL_SCANCODE_Y, KEY_Y },
    { SDL_SCANCODE_Z, KEY_Z },

    { SDL_SCANCODE_1, KEY_1 },
    { SDL_SCANCODE_2, KEY_2 },
    { SDL_SCANCODE_3, KEY_3 },
    { SDL_SCANCODE_4, KEY_4 },
    { SDL_SCANCODE_5, KEY_5 },
    { SDL_SCANCODE_6, KEY_6 },
    { SDL_SCANCODE_7, KEY_7 },
    { SDL_SCANCODE_8, KEY_8 },
    { SDL_SCANCODE_9, KEY_9 },
    { SDL_SCANCODE_0, KEY_0 },

    { SDL_SCANCODE_RETURN, KEY_ENTER },
    { SDL_SCANCODE_ESCAPE, KEY_ESCAPE },
    { SDL_SCANCODE_BACKSPACE, KEY_BACKSPACE },
    { SDL_SCANCODE_TAB, KEY_TAB },
    { SDL_SCANCODE_SPACE, KEY_SPACE },

    { SDL_SCANCODE_MINUS, KEY_MINUS },
    { SDL_SCANCODE_EQUALS, KEY_EQUALS },
    { SDL_SCANCODE_LEFTBRACKET, KEY_LEFTBRACKET },
    { SDL_SCANCODE_RIGHTBRACKET, KEY_RIGHTBRACKET },
    { SDL_SCANCODE_BACKSLASH, KEY_BACKSLASH },
    { SDL_SCANCODE_SEMICOLON, KEY_SEMICOLON },
    { SDL_SCANCODE_APOSTROPHE, KEY_APOSTROPHE },
    { SDL_SCANCODE_GRAVE, KEY_GRAVE },
    { SDL_SCANCODE_COMMA, KEY_COMMA },
    { SDL_SCANCODE_PERIOD, KEY_PERIOD },
    { SDL_SCANCODE_SLASH, KEY_SLASH },

    { SDL_SCANCODE_CAPSLOCK, KEY_CAPSLOCK },

    { SDL_SCANCODE_F1, KEY_F1 },
    { SDL_SCANCODE_F2, KEY_F2 },
    { SDL_SCANCODE_F3, KEY_F3 },
    { SDL_SCANCODE_F4, KEY_F4 },
    { SDL_SCANCODE_F5, KEY_F5 },
    { SDL_SCANCODE_F6, KEY_F6 },
    { SDL_SCANCODE_F7, KEY_F7 },
    { SDL_SCANCODE_F8, KEY_F8 },
    { SDL_SCANCODE_F9, KEY_F9 },
    { SDL_SCANCODE_F10, KEY_F10 },
    { SDL_SCANCODE_F11, KEY_F11 },
    { SDL_SCANCODE_F12, KEY_F12 },

    { SDL_SCANCODE_PRINTSCREEN, KEY_PRINTSCREEN },
    { SDL_SCANCODE_SCROLLLOCK, KEY_SCROLLLOCK },
    { SDL_SCANCODE_PAUSE, KEY_PAUSE },

    { SDL_SCANCODE_INSERT, KEY_INSERT },
    { SDL_SCANCODE_HOME, KEY_HOME },
    { SDL_SCANCODE_PAGEUP, KEY_PAGEUP },
    { SDL_SCANCODE_DELETE, KEY_DELETE },
    { SDL_SCANCODE_END, KEY_END },
    { SDL_SCANCODE_PAGEDOWN, KEY_PAGEDOWN },

    { SDL_SCANCODE_RIGHT, KEY_RIGHT },
    { SDL_SCANCODE_LEFT, KEY_LEFT },
    { SDL_SCANCODE_DOWN, KEY_DOWN },
    { SDL_SCANCODE_UP, KEY_UP },

    { SDL_SCANCODE_NUMLOCKCLEAR, KEY_NUMLOCK },
    { SDL_SCANCODE_KP_DIVIDE, KEY_DIVIDE },
    { SDL_SCANCODE_KP_MULTIPLY, KEY_MULTIPLY },
    { SDL_SCANCODE_KP_MINUS, KEY_MINUS },
    { SDL_SCANCODE_KP_PLUS, KEY_ADD },
    { SDL_SCANCODE_KP_ENTER, KEY_ENTER },
    { SDL_SCANCODE_KP_1, KEY_NUMPAD1 },
    { SDL_SCANCODE_KP_2, KEY_NUMPAD2 },
    { SDL_SCANCODE_KP_3, KEY_NUMPAD3 },
    { SDL_SCANCODE_KP_4, KEY_NUMPAD4 },
    { SDL_SCANCODE_KP_5, KEY_NUMPAD5 },
    { SDL_SCANCODE_KP_6, KEY_NUMPAD6 },
    { SDL_SCANCODE_KP_7, KEY_NUMPAD7 },
    { SDL_SCANCODE_KP_8, KEY_NUMPAD8 },
    { SDL_SCANCODE_KP_9, KEY_NUMPAD9 },
    { SDL_SCANCODE_KP_0, KEY_NUMPAD0 },
    { SDL_SCANCODE_KP_PERIOD, KEY_DECIMAL },

    { SDL_SCANCODE_LCTRL, KEY_LCTRL },
    { SDL_SCANCODE_LSHIFT, KEY_LSHIFT },
    { SDL_SCANCODE_LALT, KEY_LALT },
    { SDL_SCANCODE_RCTRL, KEY_RCTRL },
    { SDL_SCANCODE_RSHIFT, KEY_RSHIFT },
    { SDL_SCANCODE_RALT, KEY_RALT },
    { SDL_SCANCODE_MENU, KEY_MENU }
};

KeyCode GetKeyCodeFromSDLScancode(SDL_Scancode scancode)
{
    auto it = sdl_to_kc.find(scancode);
    if (it == sdl_to_kc.end())
        return KEY_NONE;
    
    return it->second;
}
static std::map<ImGuiKey, KeyCode> imgui_to_kc = {
    {ImGuiKey_None, KEY_NONE},

    // Alphabet
    {ImGuiKey_A, KEY_A},
    {ImGuiKey_B, KEY_B},
    {ImGuiKey_C, KEY_C},
    {ImGuiKey_D, KEY_D},
    {ImGuiKey_E, KEY_E},
    {ImGuiKey_F, KEY_F},
    {ImGuiKey_G, KEY_G},
    {ImGuiKey_H, KEY_H},
    {ImGuiKey_I, KEY_I},
    {ImGuiKey_J, KEY_J},
    {ImGuiKey_K, KEY_K},
    {ImGuiKey_L, KEY_L},
    {ImGuiKey_M, KEY_M},
    {ImGuiKey_N, KEY_N},
    {ImGuiKey_O, KEY_O},
    {ImGuiKey_P, KEY_P},
    {ImGuiKey_Q, KEY_Q},
    {ImGuiKey_R, KEY_R},
    {ImGuiKey_S, KEY_S},
    {ImGuiKey_T, KEY_T},
    {ImGuiKey_U, KEY_U},
    {ImGuiKey_V, KEY_V},
    {ImGuiKey_W, KEY_W},
    {ImGuiKey_X, KEY_X},
    {ImGuiKey_Y, KEY_Y},
    {ImGuiKey_Z, KEY_Z},

    // Numbers
    {ImGuiKey_1, KEY_1},
    {ImGuiKey_2, KEY_2},
    {ImGuiKey_3, KEY_3},
    {ImGuiKey_4, KEY_4},
    {ImGuiKey_5, KEY_5},
    {ImGuiKey_6, KEY_6},
    {ImGuiKey_7, KEY_7},
    {ImGuiKey_8, KEY_8},
    {ImGuiKey_9, KEY_9},
    {ImGuiKey_0, KEY_0},

    // Control & Editing
    {ImGuiKey_Enter, KEY_ENTER},
    {ImGuiKey_Escape, KEY_ESCAPE},
    {ImGuiKey_Backspace, KEY_BACKSPACE},
    {ImGuiKey_Tab, KEY_TAB},
    {ImGuiKey_Space, KEY_SPACE},

    // Punctuation & Symbols
    {ImGuiKey_Minus, KEY_MINUS},
    {ImGuiKey_Equal, KEY_EQUALS},
    {ImGuiKey_LeftBracket, KEY_LEFTBRACKET},
    {ImGuiKey_RightBracket, KEY_RIGHTBRACKET},
    {ImGuiKey_Backslash, KEY_BACKSLASH},
    {ImGuiKey_Semicolon, KEY_SEMICOLON},
    {ImGuiKey_Apostrophe, KEY_APOSTROPHE},
    {ImGuiKey_GraveAccent, KEY_GRAVE},
    {ImGuiKey_Comma, KEY_COMMA},
    {ImGuiKey_Period, KEY_PERIOD},
    {ImGuiKey_Slash, KEY_SLASH},

    // Lock & Function Keys
    {ImGuiKey_CapsLock, KEY_CAPSLOCK},
    {ImGuiKey_F1, KEY_F1},
    {ImGuiKey_F2, KEY_F2},
    {ImGuiKey_F3, KEY_F3},
    {ImGuiKey_F4, KEY_F4},
    {ImGuiKey_F5, KEY_F5},
    {ImGuiKey_F6, KEY_F6},
    {ImGuiKey_F7, KEY_F7},
    {ImGuiKey_F8, KEY_F8},
    {ImGuiKey_F9, KEY_F9},
    {ImGuiKey_F10, KEY_F10},
    {ImGuiKey_F11, KEY_F11},
    {ImGuiKey_F12, KEY_F12},

    // Navigation & System
    {ImGuiKey_PrintScreen, KEY_PRINTSCREEN},
    {ImGuiKey_ScrollLock, KEY_SCROLLLOCK},
    {ImGuiKey_Pause, KEY_PAUSE},

    {ImGuiKey_Insert, KEY_INSERT},
    {ImGuiKey_Home, KEY_HOME},
    {ImGuiKey_PageUp, KEY_PAGEUP},
    {ImGuiKey_Delete, KEY_DELETE},
    {ImGuiKey_End, KEY_END},
    {ImGuiKey_PageDown, KEY_PAGEDOWN},

    // Arrow Keys
    {ImGuiKey_RightArrow, KEY_RIGHT},
    {ImGuiKey_LeftArrow, KEY_LEFT},
    {ImGuiKey_DownArrow, KEY_DOWN},
    {ImGuiKey_UpArrow, KEY_UP},

    // Keypad
    {ImGuiKey_NumLock, KEY_NUMLOCK},
    {ImGuiKey_KeypadDivide, KEY_DIVIDE},
    {ImGuiKey_KeypadMultiply, KEY_MULTIPLY},
    {ImGuiKey_KeypadSubtract, KEY_MINUS},
    {ImGuiKey_KeypadAdd, KEY_ADD},
    {ImGuiKey_KeypadEnter, KEY_ENTER},
    {ImGuiKey_Keypad1, KEY_NUMPAD1},
    {ImGuiKey_Keypad2, KEY_NUMPAD2},
    {ImGuiKey_Keypad3, KEY_NUMPAD3},
    {ImGuiKey_Keypad4, KEY_NUMPAD4},
    {ImGuiKey_Keypad5, KEY_NUMPAD5},
    {ImGuiKey_Keypad6, KEY_NUMPAD6},
    {ImGuiKey_Keypad7, KEY_NUMPAD7},
    {ImGuiKey_Keypad8, KEY_NUMPAD8},
    {ImGuiKey_Keypad9, KEY_NUMPAD9},
    {ImGuiKey_Keypad0, KEY_NUMPAD0},
    {ImGuiKey_KeypadDecimal, KEY_DECIMAL},

    // Modifiers & Context
    {ImGuiKey_LeftCtrl, KEY_LCTRL},
    {ImGuiKey_LeftShift, KEY_LSHIFT},
    {ImGuiKey_LeftAlt, KEY_LALT},
    {ImGuiKey_RightCtrl, KEY_RCTRL},
    {ImGuiKey_RightShift, KEY_RSHIFT},
    {ImGuiKey_RightAlt, KEY_RALT},
    {ImGuiKey_Menu, KEY_MENU}
};

KeyCode GetKeyCodeFromImGuiKey(ImGuiKey key)
{
    auto it = imgui_to_kc.find(key);
    if (it == imgui_to_kc.end())
        return KEY_NONE;

    return it->second;
}
