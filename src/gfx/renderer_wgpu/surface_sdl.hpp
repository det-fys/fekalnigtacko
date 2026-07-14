#pragma once

#include <SDL2/SDL.h>
#include <webgpu/webgpu_cpp.h>

wgpu::Surface CreateWGPUSurfaceFromSDLWindow(wgpu::Instance instance, SDL_Window* window);
