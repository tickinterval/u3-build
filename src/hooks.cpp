#include "hooks.h"
#include "menu.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <MinHook.h>
#include <d3d11.h>
#include <dxgi.h>
#include <thread>
#include <atomic>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// Forward declare Win32 handler from ImGui
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace hooks {

static HWND g_hwnd = nullptr;
static WNDPROC g_original_wndproc = nullptr;
static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_context = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;
static bool g_initialized = false;
static bool g_show_menu = true;
static std::atomic<bool> g_running = true;

typedef HRESULT(WINAPI* PresentFn)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(WINAPI* ResizeBuffersFn)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

static PresentFn g_original_present = nullptr;
static ResizeBuffersFn g_original_resize = nullptr;

// Helper to get window handle
BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam) {
    DWORD pid;
    GetWindowThreadProcessId(handle, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(handle)) {
        wchar_t title[256];
        GetWindowTextW(handle, title, 256);
        if (wcslen(title) > 0) {
            *reinterpret_cast<HWND*>(lParam) = handle;
            return FALSE;
        }
    }
    return TRUE;
}

HWND GetGameWindow() {
    HWND hwnd = nullptr;
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&hwnd));
    return hwnd;
}

LRESULT CALLBACK WndProcHook(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // Toggle menu with INSERT key (обрабатываем ДО ImGui)
    if (msg == WM_KEYDOWN && wparam == VK_INSERT) {
        g_show_menu = !g_show_menu;
        return 0; // Блокируем передачу в игру
    }
    
    if (g_show_menu) {
        // Передаём событие в ImGui
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
        
        // Получаем IO для проверки захвата ввода
        ImGuiIO& io = ImGui::GetIO();
        
        // Блокируем события мыши если ImGui их использует
        if (io.WantCaptureMouse) {
            switch (msg) {
                case WM_LBUTTONDOWN:
                case WM_LBUTTONUP:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONUP:
                case WM_MBUTTONDOWN:
                case WM_MBUTTONUP:
                case WM_XBUTTONDOWN:
                case WM_XBUTTONUP:
                case WM_MOUSEWHEEL:
                case WM_MOUSEMOVE:
                case WM_MOUSEHWHEEL:
                    return 0; // Блокируем передачу в игру
            }
        }
        
        // Блокируем события клавиатуры если ImGui их использует
        if (io.WantCaptureKeyboard) {
            switch (msg) {
                case WM_KEYDOWN:
                case WM_KEYUP:
                case WM_SYSKEYDOWN:
                case WM_SYSKEYUP:
                case WM_CHAR:
                    return 0; // Блокируем передачу в игру
            }
        }
        
        // Блокируем текстовый ввод если ImGui его использует
        if (io.WantTextInput && msg == WM_CHAR) {
            return 0;
        }
    }

    // Если ImGui не захватил ввод - передаём в игру
    return CallWindowProc(g_original_wndproc, hwnd, msg, wparam, lparam);
}

HRESULT WINAPI PresentHook(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags) {
    if (!g_initialized) {
        if (SUCCEEDED(swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&g_device))) {
            g_device->GetImmediateContext(&g_context);
            
            DXGI_SWAP_CHAIN_DESC desc;
            swap_chain->GetDesc(&desc);
            g_hwnd = desc.OutputWindow;
            
            if (!g_hwnd) g_hwnd = GetGameWindow();

            // Init ImGui
            ImGui::CreateContext();
            menu::InitStyle();
            
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            
            ImGui_ImplWin32_Init(g_hwnd);
            ImGui_ImplDX11_Init(g_device, g_context);

            // Hook WndProc
            g_original_wndproc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProcHook);
            
            ID3D11Texture2D* back_buffer = nullptr;
            swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer);
            if (back_buffer) {
                g_device->CreateRenderTargetView(back_buffer, nullptr, &g_rtv);
                back_buffer->Release();
            }
            
            g_initialized = true;
        }
    }

    if (g_initialized && g_rtv) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        
        ImGuiIO& io = ImGui::GetIO();
        
        // Управление курсором мыши
        if (g_show_menu) {
            io.MouseDrawCursor = true;  // ImGui рисует курсор
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse; // Включаем мышь
            
            menu::Render();
        } else {
            io.MouseDrawCursor = false; // Скрываем ImGui курсор
        }

        ImGui::Render();
        
        // Рисуем только если меню открыто
        if (g_show_menu) {
            g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }
    }

    return g_original_present(swap_chain, sync_interval, flags);
}

HRESULT WINAPI ResizeBuffersHook(IDXGISwapChain* swap_chain, UINT buffer_count, UINT width, UINT height, DXGI_FORMAT format, UINT flags) {
    if (g_rtv) {
        g_rtv->Release();
        g_rtv = nullptr;
    }

    HRESULT hr = g_original_resize(swap_chain, buffer_count, width, height, format, flags);

    if (SUCCEEDED(hr) && g_device) {
        ID3D11Texture2D* back_buffer = nullptr;
        swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer);
        if (back_buffer) {
            g_device->CreateRenderTargetView(back_buffer, nullptr, &g_rtv);
            back_buffer->Release();
        }
    }

    return hr;
}

void InitThread() {
    // Wait for game window
    while (!GetGameWindow()) {
        Sleep(100);
    }

    // Create dummy swapchain to get VTable
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swap_chain = nullptr;
    
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = GetGameWindow();
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    if (FAILED(D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &featureLevel, 1, 
            D3D11_SDK_VERSION, &scd, &swap_chain, &device, nullptr, &context))) {
        return;
    }

    // Get VTable
    void** vtable = *reinterpret_cast<void***>(swap_chain);
    void* present_addr = vtable[8];
    void* resize_addr = vtable[13];

    // Cleanup dummy
    device->Release();
    context->Release();
    swap_chain->Release();

    // Initialize MinHook
    if (MH_Initialize() != MH_OK) {
        return;
    }

    // Create hooks
    if (MH_CreateHook(present_addr, &PresentHook, reinterpret_cast<void**>(&g_original_present)) != MH_OK) {
        return;
    }
    if (MH_CreateHook(resize_addr, &ResizeBuffersHook, reinterpret_cast<void**>(&g_original_resize)) != MH_OK) {
        return;
    }

    // Enable hooks
    MH_EnableHook(MH_ALL_HOOKS);
}

void Init() {
    std::thread(InitThread).detach();
}

void Shutdown() {
    g_running = false;
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    
    if (g_original_wndproc && g_hwnd) {
        SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_original_wndproc);
    }
}

}
