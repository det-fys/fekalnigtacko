#pragma once

#include <SDL.h>

void ImGuiInit(SDL_Window* window);
void ImGuiProcessEvent(const SDL_Event& event);
void ImGuiFrame();
bool ImGuiWantCaptureMouse();
bool ImGuiWantCaptureKeyboard();
void ImGuiDeinit();
