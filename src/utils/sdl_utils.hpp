#pragma once

#include <SDL.h>
#include <stdexcept>

inline void ThrowSDLError(const std::string& message)
{
    std::string error = SDL_GetError();
    throw std::runtime_error(message + ": " + error);
}
