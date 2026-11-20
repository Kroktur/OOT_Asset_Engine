// SPOILER - The Legend of Zelda: Ocarina of Time : Impa devient la Sage de l'Ombre.
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <stdexcept>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "ImGuiApplication.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
namespace
{
    using Microsoft::WRL::ComPtr;
    struct Application
    {
        HWND window = nullptr;
        HINSTANCE instance = GetModuleHandleW(nullptr);
        bool registered = false, contextCreated = false, platform = false, renderer = false;
        UINT width = 0, height = 0;
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        ComPtr<IDXGISwapChain> swap;
        ComPtr<ID3D11RenderTargetView> target;
        ~Application()
        {
            if (renderer) ImGui_ImplDX11_Shutdown();
            if (platform) ImGui_ImplWin32_Shutdown();
            if (contextCreated) ImGui::DestroyContext();
            if (context) context->ClearState();
            target.Reset(); swap.Reset(); context.Reset(); device.Reset();
            if (window && IsWindow(window)) DestroyWindow(window);
            if (registered) UnregisterClassW(L"OOTAssetWindow", instance);
        }
        void Target()
        {
            ComPtr<ID3D11Texture2D> buffer;
            if (FAILED(swap->GetBuffer(0, IID_PPV_ARGS(buffer.GetAddressOf())))
                || FAILED(device->CreateRenderTargetView(buffer.Get(), nullptr, target.GetAddressOf())))
                throw std::runtime_error("Cannot create DirectX render target");
        }
    };
    LRESULT WINAPI WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam)) return 1;
        auto* app = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE)
        {
            app = static_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }
        if (message == WM_SIZE && app && wparam != SIZE_MINIMIZED)
        { app->width = LOWORD(lparam); app->height = HIWORD(lparam); return 0; }
        if (message == WM_SYSCOMMAND && (wparam & 0xfff0) == SC_KEYMENU) return 0;
        if (message == WM_CLOSE) { PostQuitMessage(0); return 0; }
        if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

int RunImGuiApplication(const std::function<void()>& draw, unsigned frameLimit)
{
    Application app;
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = app.instance;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = L"OOTAssetWindow";
    if (!RegisterClassExW(&wc)) throw std::runtime_error("Cannot register window class");
    app.registered = true;
    app.window = CreateWindowExW(0, wc.lpszClassName, L"OOT Asset Engine", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1180, 760, nullptr, nullptr, app.instance, &app);
    if (!app.window) throw std::runtime_error("Cannot create application window");

    DXGI_SWAP_CHAIN_DESC description{};
    description.BufferCount = 2;
    description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.OutputWindow = app.window;
    description.SampleDesc.Count = 1;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    auto create = [&](D3D_DRIVER_TYPE driver)
    {
        return D3D11CreateDeviceAndSwapChain(nullptr, driver, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &description, app.swap.GetAddressOf(), app.device.GetAddressOf(),
            nullptr, app.context.GetAddressOf());
    };
    if (FAILED(create(D3D_DRIVER_TYPE_HARDWARE)))
    {
        app.swap.Reset(); app.device.Reset(); app.context.Reset();
        if (FAILED(create(D3D_DRIVER_TYPE_WARP))) throw std::runtime_error("Cannot initialize DirectX 11");
    }
    app.Target();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext(); app.contextCreated = true;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::GetIO().IniFilename = nullptr;
    const float dpi = ImGui_ImplWin32_GetDpiScaleForHwnd(app.window);
    ImFontConfig font;
    font.SizePixels = 18.0f * dpi;
    ImGui::GetIO().Fonts->AddFontDefault(&font);
    ImGui::StyleColorsDark();
    ImGui::GetStyle().FrameRounding = 4;
    ImGui::GetStyle().WindowPadding = ImVec2(20, 20);
    ImGui::GetStyle().ScaleAllSizes(dpi);
    if (!ImGui_ImplWin32_Init(app.window)) throw std::runtime_error("Cannot initialize ImGui Win32 backend");
    app.platform = true;
    if (!ImGui_ImplDX11_Init(app.device.Get(), app.context.Get())) throw std::runtime_error("Cannot initialize ImGui DirectX backend");
    app.renderer = true;
    if (!frameLimit) { ShowWindow(app.window, SW_SHOW); UpdateWindow(app.window); }

    unsigned frames = 0;
    bool running = true;
    while (running)
    {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message); DispatchMessageW(&message);
            if (message.message == WM_QUIT) running = false;
        }
        if (!running) break;
        if (IsIconic(app.window)) { Sleep(20); continue; }
        if (app.width && app.height)
        {
            app.context->OMSetRenderTargets(0, nullptr, nullptr);
            app.target.Reset();
            if (FAILED(app.swap->ResizeBuffers(0, app.width, app.height, DXGI_FORMAT_UNKNOWN, 0)))
                throw std::runtime_error("Cannot resize DirectX buffers");
            app.width = app.height = 0;
            app.Target();
        }
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        draw();
        ImGui::Render();
        const float background[]{0.07f, 0.09f, 0.12f, 1.f};
        app.context->OMSetRenderTargets(1, app.target.GetAddressOf(), nullptr);
        app.context->ClearRenderTargetView(app.target.Get(), background);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        const auto presented = app.swap->Present(1, 0);
        if (FAILED(presented)) throw std::runtime_error("DirectX presentation failed");
        if (presented == DXGI_STATUS_OCCLUDED && !frameLimit) Sleep(20);
        if (frameLimit && ++frames >= frameLimit) break;
    }
    return 0;
}
