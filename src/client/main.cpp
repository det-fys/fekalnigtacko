#include <SDL.h>
#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <map>

#ifdef EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/html5_webgl.h>
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>
#endif // EMSCRIPTEN

#ifdef _WIN32
#define NOMINMAX
#pragma comment(lib, "winmm.lib")
#include <windows.h>
#include <chrono>
#include <thread>
#endif

#include "app.hpp"
#include "utils/cvars.hpp"
#include "key_map.hpp"
#include "gfx/renderer.hpp"
#include "utils/sdl_utils.hpp"
#include "gfx/renderer_gl/gl.hpp"

CVAR_CL(uint16_t, cl_maxfps, CV_SAVE, 0);

static std::string s_username;
static std::string s_url;

static SDL_Window *s_window = nullptr;
static bool s_quit = false;
static std::unique_ptr<App> s_app;

struct ClientConfig
{
    std::string username;
    std::string url;
};

static void InitSDL()
{
	std::cout << "Initializing SDL..." << std::endl;
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        ThrowSDLError("SDL_Init");
    }

    Uint32 window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

    if (gfx::Renderer::IsGL())
    {
#ifdef PG_GLES
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 8); 

        window_flags |= SDL_WINDOW_OPENGL;
    }

	std::cout << "Creating SDL window..." << std::endl;
    s_window = SDL_CreateWindow("Fekalni gtacko", 100, 100, 640, 480, window_flags);
    if (!s_window)
    {
        ThrowSDLError("SDL_CreateWindow");
    }
}

static void ShutdownSDL()
{
    if (s_window)
    {
        SDL_DestroyWindow(s_window);
        s_window = nullptr;
    }
    SDL_Quit();
}

static void PollEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_QUIT:
            s_quit = true;
            return;

        case SDL_MOUSEMOTION:
            {
                int xrel = event.motion.xrel;
                int yrel = event.motion.yrel;
                if (xrel != 0 || yrel != 0)
                {
                    s_app->MouseMove(glm::vec2(static_cast<float>(xrel), static_cast<float>(yrel)));
                }
            }
            break;

        case SDL_KEYDOWN:
        case SDL_KEYUP:
            {
                auto kc = GetKeyCodeFromSDLScancode(event.key.keysym.scancode);
                if (kc != KEY_NONE)
                {
                    s_app->KeyInput(kc, event.key.state == SDL_PRESSED, event.key.repeat);
                }
            }    

            break;

        case SDL_TEXTINPUT:
            s_app->TextInput(event.text.text);
            break;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            {
                bool pressed = event.button.state == SDL_PRESSED;
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    s_app->KeyInput(KEY_LMB, pressed, 0);
                }
                else if (event.button.button == SDL_BUTTON_RIGHT)
                {
                    s_app->KeyInput(KEY_RMB, pressed, 0);
                }
            }
            break;

        case SDL_MOUSEWHEEL:
            {
                s_app->KeyInput(event.wheel.y < 0 ? KEY_WHEELUP : KEY_WHEELDOWN, true, 0);
            }
            break;

        }
        
    }
}

#ifndef NDEBUG
#define USE_LOCAL_SERVER
#endif
#define USE_LOCAL_SERVER

#ifdef USE_LOCAL_SERVER
#define WS_URL "ws://127.0.0.1:11200/ws"
#else
#define WS_URL "ws://deadfish.cz:11200/ws"
#endif

#ifdef EMSCRIPTEN
#define SAVE_PATH "/persistent/settings.dat"
#else
#define SAVE_PATH "settings.dat"
#endif

static bool can_update = false;
static Uint32 last_update = 0;
static bool fullscreen = false;

static void UpdateFullscreen()
{
    if (fullscreen == s_app->IsFullscreenRequested())
        return;

    fullscreen = s_app->IsFullscreenRequested();

    if (fullscreen)
    {
        SDL_SetWindowFullscreen(s_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
    else
    {
        SDL_SetWindowFullscreen(s_window, 0);
    }
}

static void Frame()
{
    UpdateFullscreen();

    Uint32 current_time = SDL_GetTicks();
    last_update = current_time;
    s_app->SetTime(current_time / 1000.0f); // Set time in seconds

    PollEvents();

    s_app->Frame();

    SDL_GL_SwapWindow(s_window);
}

static void FrameSafe()
{
    try {
        Frame();
    }
    catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", e.what(), nullptr);
    }
}

// #ifdef EMSCRIPTEN

// static void Update(void* args)
// {

//     // std::cout << "update called" << std::endl;
//     if (can_update && (SDL_GetTicks() - last_update >= 100))
//     {
//         // std::cout << "calling Frame()" << std::endl;
//         Frame();
//     } 

//     emscripten_async_call(Update, nullptr, 100);
//     // std::cout << "update finished" << std::endl;
// }

// #endif

static void Main() {
    srand(time(NULL));

    if (s_url.empty())
        s_url = WS_URL;

    InitSDL();

    try
    {
        gfx::Renderer::Init(s_window);
    }
    catch (...)
    {
        ShutdownSDL();
        throw;
    }

    SDL_SetRelativeMouseMode(SDL_TRUE);

    s_app = std::make_unique<App>(SAVE_PATH);
    s_app->SetUserName(s_username);
    s_app->SetUrl(s_url);

    can_update = true;

#ifdef EMSCRIPTEN
    emscripten_set_main_loop(FrameSafe, 0, false);
    // Update(nullptr);
#else

#ifdef _WIN32
    timeBeginPeriod(1);
#endif
    SDL_GL_SetSwapInterval(0);

    while (!s_quit)
    {
        auto frame_dur = std::chrono::milliseconds(cl_maxfps.Get() > 0 ? (1000 / cl_maxfps.Get()) : 0);
        auto t_start = std::chrono::steady_clock::now();
        
        Frame();
    
        auto t_next = t_start + frame_dur;
        auto t_now = std::chrono::steady_clock::now();
        
        if (t_now < t_next)
        {
            std::this_thread::sleep_for(t_next - t_now);
        }
    }

    s_app.reset();

    gfx::Renderer::Uninit();
    ShutdownSDL();

#endif // EMSCRIPTEN
}

extern "C"
{

void RunMain()
{
    try {
        Main();
    }
    catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", e.what(), nullptr);
    }
}

void SetName(const char* name)
{
    s_username = name;
}

void SetUrl(const char* url)
{
    s_url = url;
}

void SetRenderer(const char* renderer)
{
    CVarRegistry::GetClientInstance().Set("r_renderer", renderer);
}

}

#ifndef EMSCRIPTEN

static void ProcessArgs(int argc, char* argv[])
{
    std::string var_name;

    for (int i = 1; i < argc; ++i)
    {
        // var name
        if (var_name.empty())
        {
            var_name = argv[i];
            continue;
        }

        // value
        std::string value = argv[i];
        CVarRegistry::GetClientInstance().Set(var_name, value);
        var_name.clear();
    }
}

int main(int argc, char *argv[])
{
    ProcessArgs(argc, argv);
    SetName("random guvno");
    RunMain();
	return 0;
}

#else

int main(int argc, char *argv[])
{

}


#endif
