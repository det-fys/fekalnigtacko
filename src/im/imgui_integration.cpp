#include "imgui_integration.hpp"

#ifdef PG_IMGUI
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_wgpu.h>
#include <ImGuizmo.h>

#include "gfx/renderer.hpp"
#include "gfx/renderer_wgpu/renderer_wgpu.hpp"
#include "gfx/renderer_gl/renderer_gl.hpp"
#include "gfx/renderer_gl/shader_common.hpp"

static bool using_wgpu = false;
static bool need_mouse = false;

static void InitForGL(gfx::RendererGL& renderer)
{
    ImGui_ImplOpenGL3_Init(GLSL_VERSION);

    renderer.SetGuiRenderCallback([]() {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    });
}

static void InitForWGPU(gfx::RendererWGPU& renderer)
{
    wgpu::Device device_ = renderer.GetDevice();
    wgpu::TextureFormat surface_format_ = renderer.GetSurfaceFormat();

    ImGui_ImplWGPU_InitInfo init_info{};
    init_info.Device = device_.Get();
    init_info.NumFramesInFlight = 3;
    init_info.RenderTargetFormat = static_cast<WGPUTextureFormat>(surface_format_);
    init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;
    ImGui_ImplWGPU_Init(&init_info);

    renderer.SetGuiRenderCallback([](wgpu::RenderPassEncoder& pass) {
        ImGui::Render();
        ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass.Get());
    });
}

void ImGuiInit(SDL_Window* window)
{
    IMGUI_CHECKVERSION();
    auto ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplSDL2_InitForOther(window);

    // initialize for the current renderer
    auto& renderer = gfx::Renderer::GetInstance();
    if (auto wgpu_renderer = dynamic_cast<gfx::RendererWGPU*>(&renderer))
    {
        InitForWGPU(*wgpu_renderer);
        using_wgpu = true;
    }
    else if (auto gl_renderer = dynamic_cast<gfx::RendererGL*>(&renderer))
    {
        InitForGL(*gl_renderer);
    }
    else
    {
        throw std::runtime_error("ImGui integration: Unsupported renderer");
    }

    ImGuizmo::SetImGuiContext(ctx);
}

void ImGuiProcessEvent(const SDL_Event& event) 
{
    ImGui_ImplSDL2_ProcessEvent(&event);
}
    
void ImGuiFrame()
{
    if (using_wgpu)
    {
        ImGui_ImplWGPU_NewFrame();
    }
    else
    {
        ImGui_ImplOpenGL3_NewFrame();
    }

    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // gizmo
    ImGuizmo::BeginFrame();

    // setup docking space
    //ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    need_mouse =
        ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);

    auto& io = ImGui::GetIO();
    if (need_mouse)
    {
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    }
    else
    {
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    }
}

bool ImGuiWantCaptureMouse()
{
    return need_mouse;
}

bool ImGuiWantCaptureKeyboard()
{
    return ImGui::GetIO().WantCaptureKeyboard;
}

void ImGuiDeinit()
{
    if (using_wgpu)
    {
        ImGui_ImplWGPU_Shutdown();
    }
    else
    {
        ImGui_ImplOpenGL3_Shutdown();
    }

    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

#else // PG_IMGUI

void ImGuiInit(SDL_Window* window) {}
void ImGuiFrame() {}
void ImGuiDeinit() {}

#endif // PG_IMGUI