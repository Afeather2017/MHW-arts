#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <atomic>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "kiero.h"

namespace {
using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);

PresentFn g_originalPresent = nullptr;
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
std::once_flag g_imguiOnce;
std::atomic_bool g_visible = true;
bool g_f8WasDown = false;

void Log(const char* message) {
    std::ofstream out("nativePC\\plugins\\ValorGauge.log", std::ios::app);
    if (out) {
        out << message << '\n';
    }
}

bool InitializeRenderer(IDXGISwapChain* swapChain) {
    if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_device)))) {
        Log("DX11 device acquisition failed");
        return false;
    }

    g_device->GetImmediateContext(&g_context);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    if (!ImGui_ImplDX11_Init(g_device, g_context)) {
        Log("ImGui DX11 initialization failed");
        return false;
    }

    Log("DX11 renderer initialized");
    return true;
}

void DrawGauge(IDXGISwapChain* swapChain) {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swapChain->GetDesc(&desc))) {
        return;
    }

    RECT client{};
    if (!GetClientRect(desc.OutputWindow, &client)) {
        return;
    }

    const float width = static_cast<float>(client.right - client.left);
    const float height = static_cast<float>(client.bottom - client.top);
    if (width <= 0.0f || height <= 0.0f) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(width, height);
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    const bool f8Down = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
    if (f8Down && !g_f8WasDown) {
        g_visible = !g_visible.load();
    }
    g_f8WasDown = f8Down;

    if (g_visible) {
        constexpr float value = 0.65f;
        const float gaugeWidth = std::min(420.0f, width * 0.28f);
        const float gaugeHeight = 18.0f;
        const ImVec2 minPoint(48.0f, height * 0.31f);
        const ImVec2 maxPoint(minPoint.x + gaugeWidth, minPoint.y + gaugeHeight);
        const ImVec2 fillPoint(minPoint.x + gaugeWidth * value, maxPoint.y);

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        draw->AddRectFilled(ImVec2(minPoint.x - 3.0f, minPoint.y - 3.0f),
                            ImVec2(maxPoint.x + 3.0f, maxPoint.y + 3.0f),
                            IM_COL32(5, 10, 18, 205), 3.0f);
        draw->AddRectFilled(minPoint, maxPoint, IM_COL32(18, 35, 52, 230), 2.0f);
        draw->AddRectFilledMultiColor(minPoint, fillPoint,
                                      IM_COL32(30, 125, 220, 245),
                                      IM_COL32(55, 205, 255, 255),
                                      IM_COL32(35, 150, 235, 255),
                                      IM_COL32(20, 90, 190, 245));
        draw->AddRect(minPoint, maxPoint, IM_COL32(160, 225, 255, 230), 2.0f, 0, 1.0f);
        draw->AddText(ImVec2(minPoint.x, minPoint.y - 20.0f),
                      IM_COL32(205, 240, 255, 255), "VALOR");
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

HRESULT __stdcall PresentHook(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) {
    std::call_once(g_imguiOnce, [swapChain]() { InitializeRenderer(swapChain); });
    if (g_device && g_context) {
        DrawGauge(swapChain);
    }
    return g_originalPresent(swapChain, syncInterval, flags);
}

DWORD WINAPI InitializePlugin(void*) {
    Log("ValorGauge loading");

    for (int attempt = 0; attempt < 120; ++attempt) {
        const auto status = kiero::init(kiero::RenderType::D3D11);
        if (status == kiero::Status::Success) {
            const auto bindStatus = kiero::bind(8, reinterpret_cast<void**>(&g_originalPresent),
                                                reinterpret_cast<void*>(PresentHook));
            if (bindStatus == kiero::Status::Success) {
                Log("DX11 Present hook installed");
                return 0;
            }
            Log("DX11 Present bind failed");
            return 1;
        }
        Sleep(1000);
    }

    Log("DX11 initialization timed out");
    return 1;
}
} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        const HANDLE thread = CreateThread(nullptr, 0, InitializePlugin, nullptr, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
