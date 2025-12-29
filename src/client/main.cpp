#include <SDL.h>
#include <iostream>
#include <memory>
#include "app.hpp"

#ifdef EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/html5_webgl.h>
#endif

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
    s_window = SDL_CreateWindow("PortalGame", 100, 100, 640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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

static void Frame()
{
    Uint32 current_time = SDL_GetTicks();
    s_app->SetTime(current_time / 1000.0f); // Set time in seconds

    PollEvents();

	int width, height;
	SDL_GetWindowSize(s_window, &width, &height);
	s_app->SetViewportSize(width, height);
    
	game::PlayerInputFlags input = 0;
	const uint8_t* kbd_state = SDL_GetKeyboardState(nullptr);
    
    if (kbd_state[SDL_GetScancodeFromKey(SDLK_w)])
		input |= game::PI_FORWARD;

	if (kbd_state[SDL_GetScancodeFromKey(SDLK_s)])
		input |= game::PI_BACKWARD;

	if (kbd_state[SDL_GetScancodeFromKey(SDLK_a)])
		input |= game::PI_LEFT;

	if (kbd_state[SDL_GetScancodeFromKey(SDLK_d)])
		input |= game::PI_RIGHT;

	if (kbd_state[SDL_GetScancodeFromKey(SDLK_SPACE)])
		input |= game::PI_JUMP;

	if (kbd_state[SDL_GetScancodeFromKey(SDLK_LCTRL)])
		input |= game::PI_CROUCH;

	if (kbd_state[SDL_GetScancodeFromKey(SDLK_e)])
		input |= game::PI_USE;

    if (kbd_state[SDL_GetScancodeFromKey(SDLK_F3)])
		input |= game::PI_DEBUG1;

	if (kbd_state[SDL_GetScancodeFromKey(SDLK_F4)])
		input |= game::PI_DEBUG2;

	if (kbd_state[SDL_GetScancodeFromKey(SDLK_F5)])
		input |= game::PI_DEBUG3;

	int mouse_state = SDL_GetMouseState(nullptr, nullptr);

	if (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT))
		input |= game::PI_ATTACK;


	s_app->SetInput(input);

    s_app->Frame();
    SDL_GL_SwapWindow(s_window);
}

static void Main() {
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

#ifdef EMSCRIPTEN
    emscripten_set_main_loop(Frame, 0, true);
#else
    while (!s_quit)
    {
        Frame();
    }

    s_app.reset();

    ShutdownGL();
    ShutdownSDL();
#endif
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
