#include "surface_sdl.hpp"

#include <SDL2/SDL_syswm.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

wgpu::Surface CreateWGPUSurfaceFromSDLWindow(wgpu::Instance instance, SDL_Window* window)
{
#if defined(__EMSCRIPTEN__)
    // webgpu_cpp / Emscripten C++ mapping
    wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc{};
    canvasDesc.sType = wgpu::SType::EmscriptenSurfaceSourceCanvasHTMLSelector;
    canvasDesc.selector = "#canvas"; // Emscripten's default mapping for SDL2 window

    wgpu::SurfaceDescriptor surfaceDesc{};
    surfaceDesc.nextInChain = &canvasDesc;
    surfaceDesc.label = "Emscripten Canvas Surface";

    return instance.CreateSurface(&surfaceDesc);

#else
    // Native Desktop platforms via SDL2 SysWM
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get WM info: %s", SDL_GetError());
        return nullptr;
    }

    wgpu::SurfaceDescriptor surfaceDesc{};

#if defined(SDL_VIDEO_DRIVER_WINDOWS)
    wgpu::SurfaceSourceWindowsHWND winDesc{};
    winDesc.sType = wgpu::SType::SurfaceSourceWindowsHWND;
    winDesc.hinstance = GetModuleHandle(nullptr);
    winDesc.hwnd = wmInfo.info.win.window;
    surfaceDesc.nextInChain = &winDesc;

#elif defined(SDL_VIDEO_DRIVER_COCOA)
    wgpu::SurfaceSourceMetalLayer metalDesc{};
    metalDesc.sType = wgpu::SType::SurfaceSourceMetalLayer;
    metalDesc.layer = wmInfo.info.cocoa.window;
    surfaceDesc.nextInChain = &metalDesc;

#elif defined(SDL_VIDEO_DRIVER_X11)
    wgpu::SurfaceSourceXlibWindow x11Desc{};
    x11Desc.sType = wgpu::SType::SurfaceSourceXlibWindow;
    x11Desc.display = wmInfo.info.x11.display;
    x11Desc.window = wmInfo.info.x11.window;
    surfaceDesc.nextInChain = &x11Desc;

#elif defined(SDL_VIDEO_DRIVER_WAYLAND)
    wgpu::SurfaceSourceWaylandSurface waylandDesc{};
    waylandDesc.sType = wgpu::SType::SurfaceSourceWaylandSurface;
    waylandDesc.display = wmInfo.info.wl.display;
    waylandDesc.surface = wmInfo.info.wl.surface;
    surfaceDesc.nextInChain = &waylandDesc;
#endif

    return instance.CreateSurface(&surfaceDesc);
#endif
}
