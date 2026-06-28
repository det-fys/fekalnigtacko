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
#include "gl.hpp"
#include "utils/cvars.hpp"

static std::string s_username;
static std::string s_url;

static SDL_Window *s_window = nullptr;
static SDL_GLContext s_context = nullptr;
static bool s_quit = false;
static std::unique_ptr<App> s_app;

struct ClientConfig
{
    std::string username;
    std::string url;
};

static void ThrowSDLError(const std::string& message)
{
    std::string error = SDL_GetError();
    throw std::runtime_error(message + ": " + error);
}

static void InitSDL()
{
	std::cout << "Initializing SDL..." << std::endl;
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        ThrowSDLError("SDL_Init");
    }

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

	std::cout << "Creating SDL window..." << std::endl;
    s_window =
        SDL_CreateWindow("Fekalni gtacko", 100, 100, 640, 480,
                         SDL_WINDOW_SHOWN /* | SDL_WINDOW_MAXIMIZED */| SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!s_window)
    {
        ThrowSDLError("SDL_CreateWindow");
    }
}

#ifndef PG_GLES
static void APIENTRY GLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    if (severity == 0x826b)
        return;
    
    ////std::cout << message << std::endl;
    fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
        (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
        type, severity, message);
}

#endif // PG_GLES


static void InitGL()
{
	std::cout << "Creating OpenGL context..." << std::endl;
    s_context = SDL_GL_CreateContext(s_window);
    if (!s_context)
    {
        ThrowSDLError("SDL_GL_CreateContext");
    }

    // Make context current
    if (SDL_GL_MakeCurrent(s_window, s_context) != 0)
    {
        SDL_GL_DeleteContext(s_context);
        ThrowSDLError("SDL_GL_MakeCurrent");
    }

#ifndef PG_GLES
	// Initialize GLAD
	std::cout << "Initializing GLAD..." << std::endl;
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        SDL_GL_DeleteContext(s_context);
        throw std::runtime_error("Failed to initialize GLAD");
    }
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(GLDebugCallback, 0);

    SDL_GL_SetSwapInterval(0);
#endif // PG_GLES

}

static void ShutdownGL()
{
    if (s_context)
    {
        SDL_GL_DeleteContext(s_context);
        s_context = nullptr;
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

static const std::map<SDL_Scancode, game::PlayerInputType> s_inputmap = {
    { SDL_SCANCODE_W, game::IN_FORWARD },
    { SDL_SCANCODE_S, game::IN_BACKWARD },
    { SDL_SCANCODE_A, game::IN_LEFT },
    { SDL_SCANCODE_D, game::IN_RIGHT },
    { SDL_SCANCODE_SPACE, game::IN_JUMP },
    { SDL_SCANCODE_LSHIFT, game::IN_SPRINT },
    { SDL_SCANCODE_LCTRL, game::IN_CROUCH },
    { SDL_SCANCODE_E, game::IN_USE },
    { SDL_SCANCODE_Q, game::IN_HOLSTER },
    { SDL_SCANCODE_R, game::IN_RELOAD },
    { SDL_SCANCODE_LALT, game::IN_AIM_MODE },
    { SDL_SCANCODE_1, game::IN_WEAPON_1 },
    { SDL_SCANCODE_2, game::IN_WEAPON_2 },
    { SDL_SCANCODE_3, game::IN_WEAPON_3 },
    { SDL_SCANCODE_4, game::IN_WEAPON_4 },
    { SDL_SCANCODE_5, game::IN_WEAPON_5 },
    { SDL_SCANCODE_6, game::IN_WEAPON_6 },
    { SDL_SCANCODE_7, game::IN_WEAPON_7 },
    { SDL_SCANCODE_8, game::IN_WEAPON_8 },
    { SDL_SCANCODE_9, game::IN_WEAPON_9 },
    { SDL_SCANCODE_0, game::IN_WEAPON_0 },
    { SDL_SCANCODE_F3, game::IN_DEBUG1 },
    { SDL_SCANCODE_F4, game::IN_DEBUG2 },
    { SDL_SCANCODE_F5, game::IN_DEBUG3 },
    { SDL_SCANCODE_TAB, game::IN_MENU },
};

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
                auto in_it = s_inputmap.find(event.key.keysym.scancode);
                if (in_it != s_inputmap.end())
                {
                    s_app->Input(in_it->second, event.key.state == SDL_PRESSED, event.key.repeat != 0);
                }
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            {
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    s_app->Input(game::IN_ATTACK_PRIMARY, event.button.state == SDL_PRESSED, false);
                }
                else if (event.button.button == SDL_BUTTON_RIGHT)
                {
                    s_app->Input(game::IN_ATTACK_SECONDARY, event.button.state == SDL_PRESSED, false);
                }
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

static void Frame()
{
    Uint32 current_time = SDL_GetTicks();
    last_update = current_time;
    s_app->SetTime(current_time / 1000.0f); // Set time in seconds

    PollEvents();

	int width, height;
	SDL_GetWindowSize(s_window, &width, &height);
	s_app->SetViewportSize(width, height);

    s_app->Frame();

    SDL_GL_SwapWindow(s_window);
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
    if (s_url.empty())
        s_url = WS_URL;

    InitSDL();

    try
    {
        InitGL();
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
    emscripten_set_main_loop(Frame, 0, false);
    // Update(nullptr);
#else

#ifdef _WIN32
    timeBeginPeriod(1);
#endif
    SDL_GL_SetSwapInterval(0);

    //auto frame_dur = std::chrono::milliseconds(0);
    
    while (!s_quit)
    {
        //auto t_start = std::chrono::steady_clock::now();
        
        Frame();
    
        //auto t_next = t_start + frame_dur;
        //auto t_now = std::chrono::steady_clock::now();
        
        //if (t_now < t_next)
        //{
            //std::this_thread::sleep_for(t_next - t_now);
        //}
    }

    s_app.reset();

    ShutdownGL();
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

}

#ifndef EMSCRIPTEN

int main(int argc, char *argv[])
{
    SetName("random guvno");
    RunMain();
	return 0;
}

#else

int main(int argc, char *argv[])
{

}


#endif
