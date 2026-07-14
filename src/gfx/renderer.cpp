#pragma once

#include "renderer.hpp"

#include <memory>

#include <SDL.h>

#include "renderer_gl/renderer_gl.hpp"
#include "renderer_wgpu/renderer_wgpu.hpp"
#include "utils/cvars.hpp"

CVAR_CL(std::string, r_renderer, CV_SAVE, "gl");

static std::unique_ptr<gfx::Renderer> renderer;
static gfx::Renderer* renderer_ptr = nullptr;

bool gfx::Renderer::IsGL()
{
    return r_renderer.Get() == "gl";
}

void gfx::Renderer::Init(SDL_Window* window)
{
    if (r_renderer.Get() == "gl")
    {
        renderer = std::make_unique<RendererGL>(window);
    }
    else if (r_renderer.Get() == "wgpu")
    {
        renderer = std::make_unique<RendererWGPU>(window);
    }
    else
    {
        throw std::runtime_error("Unknown renderer type: " + r_renderer.Get());
    }

    renderer_ptr = renderer.get();
}

gfx::Renderer& gfx::Renderer::GetInstance()
{
    return *renderer_ptr;
}

void gfx::Renderer::Uninit()
{
    renderer.reset();
    renderer_ptr = nullptr;
}

gfx::Renderer::Renderer(SDL_Window* window) : window_(window) {}

glm::u32vec2 gfx::Renderer::GetViewportSize() const
{
    glm::ivec2 size{};
#ifdef EMSCRIPTEN
    emscripten_get_canvas_element_size("#canvas", &size.x, &size.y);
#else
    SDL_GetWindowSize(window_, &size.x, &size.y);
#endif

    return glm::u32vec2(static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y));
}

