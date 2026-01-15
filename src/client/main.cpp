#include <SDL.h>
#include <iostream>
#include <memory>
#include <vector>

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
#include <WinSock2.h>
#endif

#include "app.hpp"
#include "gl.hpp"

static SDL_Window *s_window = nullptr;
static SDL_GLContext s_context = nullptr;
static bool s_quit = false;
static std::unique_ptr<App> s_app;

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

	std::cout << "Creating SDL window..." << std::endl;
    s_window =
        SDL_CreateWindow("PortalGame", 100, 100, 640, 480,
                         SDL_WINDOW_SHOWN /* | SDL_WINDOW_MAXIMIZED */| SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!s_window)
    {
        ThrowSDLError("SDL_CreateWindow");
    }
}

#ifndef PG_GLES
static void APIENTRY GLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    //if (severity == 0x826b)
    //    return;
    //
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
			int xrel = event.motion.xrel;
			int yrel = event.motion.yrel;
            if (xrel != 0 || yrel != 0)
            {
				s_app->MouseMove(glm::vec2(static_cast<float>(xrel), static_cast<float>(yrel)));
            }
            break;
        }
        
    }
}

#ifdef NDEBUG
#define WS_URL "ws://deadfish.cz:11200/ws"
#else
#define WS_URL "ws://127.0.0.1:11200/ws"
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
    
	game::PlayerInputFlags input = 0;
	const uint8_t* kbd_state = SDL_GetKeyboardState(nullptr);
    
    if (kbd_state[SDL_GetScancodeFromKey(SDLK_w)])
		input |= (1 << game::IN_FORWARD);

	if (kbd_state[SDL_GetScancodeFromKey(SDLK_s)])
		input |= (1 << game::IN_BACKWARD);

	if (kbd_state[SDL_GetScancodeFromKey(SDLK_a)])
		input |= (1 << game::IN_LEFT);

	if (kbd_state[SDL_GetScancodeFromKey(SDLK_d)])
		input |= (1 << game::IN_RIGHT);

	if (kbd_state[SDL_GetScancodeFromKey(SDLK_SPACE)])
		input |= (1 << game::IN_JUMP);

	if (kbd_state[SDL_GetScancodeFromKey(SDLK_LCTRL)])
		input |= (1 << game::IN_CROUCH);

	if (kbd_state[SDL_GetScancodeFromKey(SDLK_e)])
		input |= (1 << game::IN_USE);

    if (kbd_state[SDL_GetScancodeFromKey(SDLK_F3)])
		input |= (1 << game::IN_DEBUG1);

	if (kbd_state[SDL_GetScancodeFromKey(SDLK_F4)])
		input |= (1 << game::IN_DEBUG2);

	if (kbd_state[SDL_GetScancodeFromKey(SDLK_F5)])
		input |= (1 << game::IN_DEBUG3);

	int mouse_state = SDL_GetMouseState(nullptr, nullptr);

	if (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT))
		input |= game::IN_ATTACK;

	s_app->SetInput(input);

    s_app->Frame();

    if (s_ws_connected)
    {
        auto msg = s_app->GetMsg();
        if (!msg.empty())
        {
            WSSend(msg);
        }
    }

    s_app->ResetMsg();

    SDL_GL_SwapWindow(s_window);
}

static void Main() {
    if (!WSInit(WS_URL))
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
    s_app->AddChatMessagePrefix("WebSocket", "připojování na " + std::string(WS_URL));

#ifdef EMSCRIPTEN
    emscripten_set_main_loop(Frame, 0, true);
#else

    SDL_GL_SetSwapInterval(0);

    while (!s_quit)
    {
        Frame();
    }
        
    s_app.reset();

    ShutdownGL();
    ShutdownSDL();
    WSClose();

#endif // EMSCRIPTEN
}

int main(int argc, char *argv[])
{
    try {
        Main();
    }
    catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", e.what(), nullptr);
        return EXIT_FAILURE;
    }

	return EXIT_SUCCESS;
}
