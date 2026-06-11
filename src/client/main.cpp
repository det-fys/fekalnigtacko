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
#else
#include <easywsclient.hpp>
#endif // EMSCRIPTEN

#ifdef _WIN32
#define NOMINMAX
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm.lib")
#include <WinSock2.h>
#include <windows.h>
#include <chrono>
#include <thread>
#endif

#include "app.hpp"
#include "gl.hpp"

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
                    s_app->Input(game::IN_ATTACK_PRIMARY, event.button.state == SDL_PRESSED, event.button.clicks > 1);
                }
                else if (event.button.button == SDL_BUTTON_RIGHT)
                {
                    s_app->Input(game::IN_ATTACK_SECONDARY, event.button.state == SDL_PRESSED, event.button.clicks > 1);
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


static bool s_ws_connected = false;

#ifdef EMSCRIPTEN

static EMSCRIPTEN_WEBSOCKET_T s_ws = 0;

static EM_BOOL OnWSOpen(int type, const EmscriptenWebSocketOpenEvent *ev, void *ud)
{
    s_ws_connected = true;
    s_app->Connected();
    return EM_TRUE;
}

static EM_BOOL OnWSMessage(int type, const EmscriptenWebSocketMessageEvent *ev, void *ud)
{
    if (ev->isText)
        return EM_TRUE;

    net::InMessage msg(reinterpret_cast<char*>(ev->data), ev->numBytes);
    s_app->ProcessMessage(msg);

    return EM_TRUE;
}

static EM_BOOL OnWSClose(int type, const EmscriptenWebSocketCloseEvent *ev, void *ud)
{
    s_ws_connected = false;
    s_app->Disconnected("");
    return EM_TRUE;
}

static EM_BOOL OnWSError(int type, const EmscriptenWebSocketErrorEvent *ev, void *ud)
{
    s_ws_connected = false;
    s_app->Disconnected("error");
    return EM_TRUE;
}

static bool WSInit(const char* url)
{
    if (!emscripten_websocket_is_supported())
    {
        std::cerr << "EMSCRIPTEN WS NOT SUPPORTED" << std::endl;
        return false;
    }
    EmscriptenWebSocketCreateAttributes ws_attrs = {
        url,
        NULL,
        EM_TRUE
    };

    s_ws = emscripten_websocket_new(&ws_attrs);
    emscripten_websocket_set_onopen_callback(s_ws, NULL, OnWSOpen);
    emscripten_websocket_set_onmessage_callback(s_ws, NULL, OnWSMessage);
    emscripten_websocket_set_onclose_callback(s_ws, NULL, OnWSClose);
    emscripten_websocket_set_onerror_callback(s_ws, NULL, OnWSError);

    return true;
}

static void WSSend(std::span<const char> data)
{
    emscripten_websocket_send_binary(s_ws, (void*)data.data(), static_cast<uint32_t>(data.size()));
}

static void WSPoll() {}
static void WSClose() {}

#else /* !EMSCRIPTEN */

using namespace easywsclient; 

static std::unique_ptr<WebSocket> s_ws;

static bool WSInit(const char* url)
{

#ifdef _WIN32
    INT rc;
    WSADATA wsaData;

    rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (rc)
    {
        printf("WSAStartup Failed.\n");
        return false;
    }
#endif

    s_ws = std::unique_ptr<WebSocket>(WebSocket::from_url(url));

    if (!s_ws)
        return false;

    return true;
}

static void WSSend(std::span<const char> msg)
{
    static std::vector<uint8_t> data;
    data.resize(msg.size_bytes());
    memcpy(data.data(), msg.data(), msg.size_bytes());
    s_ws->sendBinary(data);
}

static void WSPoll()
{
    s_ws->poll();

    auto ws_state = s_ws->getReadyState();
    if (ws_state == WebSocket::OPEN && !s_ws_connected)
    {
        s_ws_connected = true;
        s_app->Connected();
    }
    else if (ws_state != WebSocket::OPEN && s_ws_connected)
    {
        s_ws_connected = false;
        s_app->Disconnected("WS closed");
    }

    s_ws->dispatchBinary([&](const std::vector<uint8_t>& data) {
        net::InMessage msg(reinterpret_cast<const char*>(data.data()), data.size());
        s_app->ProcessMessage(msg);
    });
}

static void WSClose()
{
    s_ws.reset();

#ifdef _WIN32
    WSACleanup();
#endif
}

#endif /* EMSCRIPTEN */

static void Frame()
{
    Uint32 current_time = SDL_GetTicks();
    s_app->SetTime(current_time / 1000.0f); // Set time in seconds

    PollEvents();
    WSPoll();

	int width, height;
	SDL_GetWindowSize(s_window, &width, &height);
	s_app->SetViewportSize(width, height);

    s_app->Frame();

    auto session = s_app->GetSession();
    if (session)
    {
        if (s_ws_connected)
        {
            auto msg = session->GetMsg();
            if (!msg.empty())
            {
                WSSend(msg);
            }
        }
    
        session->ResetMsg();
    }

    SDL_GL_SwapWindow(s_window);
}

static void Main() {
    if (s_url.empty())
        s_url = WS_URL;

    if (!WSInit(s_url.c_str()))
        return;

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


    s_app = std::make_unique<App>();
    s_app->SetUserName(s_username);
    s_app->AddChatMessagePrefix("WebSocket", "připojování na " + s_url);

#ifdef EMSCRIPTEN
    emscripten_set_main_loop(Frame, 0, true);
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
    WSClose();

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
