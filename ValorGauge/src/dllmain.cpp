#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <Psapi.h>

#include <atomic>
#include <algorithm>
#include <chrono>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "kiero.h"
#include "MinHook.h"

namespace {
using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
struct ActionInfo {
    std::int32_t actionSet;
    std::int32_t actionId;
};
using DoActionFn = bool(__fastcall*)(void*, ActionInfo*);

PresentFn g_originalPresent = nullptr;
DoActionFn g_originalDoAction = nullptr;
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
std::once_flag g_imguiOnce;
std::atomic_bool g_visible = true;
bool g_f8WasDown = false;
bool g_f7WasDown = false;
std::atomic_bool g_stateLoggerEnabled = true;
std::atomic_uintptr_t g_stateAddress = 0;
std::optional<std::uint32_t> g_lastState;
std::chrono::steady_clock::time_point g_nextStatePoll{};
std::mutex g_logMutex;
std::mutex g_actionMutex;
void* g_lastLongSwordController = nullptr;
void* g_lockedPlayerController = nullptr;
std::optional<ActionInfo> g_lastLongSwordAction;
std::atomic_uint32_t g_actionDiagnosticCount = 0;
std::atomic_uint32_t g_valorCharge = 0;
std::atomic_bool g_valorMode = false;
std::atomic_int32_t g_lastObservedSpiritLevel = -1;

bool IsNormalModeChargingAction(const ActionInfo& action) {
    if (action.actionSet != 3) {
        return false;
    }
    switch (action.actionId) {
    case 7:  // Step Slash from sheath
    case 85: // Step Slash after draw
    case 87: // Overhead Slash after Step Slash
    case 79: // Thrust
    case 80: // Rising Slash
    case 11: // Spirit Blade I from sheath
    case 16: // Spirit Blade I after draw
    case 67: // Spirit Blade II
    case 68: // Spirit Blade III
        return true;
    default:
        return false;
    }
}

void Log(const char* message) {
    std::scoped_lock lock(g_logMutex);
    std::ofstream out("nativePC\\plugins\\ValorGauge.log", std::ios::app);
    if (out) {
        out << message << '\n';
    }
}

void Log(const std::string& message) {
    Log(message.c_str());
}

std::optional<std::uintptr_t> ParseAddress(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return std::nullopt;
    }
    value = value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
    if (value.starts_with("0x") || value.starts_with("0X")) {
        value.erase(0, 2);
    }

    std::uintptr_t address = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), address, 16);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return address;
}

void LoadStateAddress() {
    std::ifstream input("nativePC\\plugins\\ValorGauge.state-address.txt");
    std::string value;
    if (!input || !std::getline(input, value)) {
        g_stateAddress = 0;
        g_lastState.reset();
        Log("State logger waiting: create ValorGauge.state-address.txt with a hexadecimal address");
        return;
    }

    const auto parsed = ParseAddress(value);
    if (!parsed || *parsed == 0) {
        g_stateAddress = 0;
        g_lastState.reset();
        Log("State logger address is invalid; expected hexadecimal, for example 0x7FF612345678");
        return;
    }

    g_stateAddress = *parsed;
    g_lastState.reset();
    std::ostringstream message;
    message << "State logger watching 0x" << std::hex << std::uppercase << *parsed;
    Log(message.str());
}

bool IsReadable(const void* address, std::size_t size) {
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(address, &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto end = start + size;
    const auto regionEnd = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    return end >= start && end <= regionEnd;
}

bool IsWritable(const void* address, std::size_t size) {
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(address, &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE |
                           PAGE_EXECUTE_WRITECOPY;
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto end = start + size;
    const auto regionEnd = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    return (info.Protect & writable) != 0 && end >= start && end <= regionEnd;
}

std::int32_t* ResolveLongSwordLevel(void* controller) {
    if (!controller) {
        return nullptr;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    auto* weaponAddress = player + 0x76B0;
    if (!IsReadable(weaponAddress, sizeof(void*))) {
        return nullptr;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(weaponAddress);
    if (!weapon) {
        return nullptr;
    }
    auto* level = reinterpret_cast<std::int32_t*>(weapon + 0x2370);
    return IsReadable(level, sizeof(*level)) && IsWritable(level, sizeof(*level)) ? level : nullptr;
}

void ActivateValorMode(void* controller) {
    if (g_valorMode.load()) {
        return;
    }
    auto* level = ResolveLongSwordLevel(controller);
    if (!level) {
        Log("Valor activation failed: Long Sword level pointer chain did not resolve");
        return;
    }

    const auto previous = *level;
    *level = 3; // Red aura: strongest Long Sword level and confirmed writable by the Lua mod.
    g_lastObservedSpiritLevel = 3;
    g_valorMode = true;

    std::ostringstream message;
    message << "Valor mode activated: Long Sword level " << previous << " -> 3 at 0x"
            << std::hex << std::uppercase << reinterpret_cast<std::uintptr_t>(level);
    Log(message.str());
}

void PollValorLifecycle() {
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    auto* level = ResolveLongSwordLevel(controller);
    if (!level) {
        return;
    }

    const auto current = *level;
    const auto previous = g_lastObservedSpiritLevel.exchange(current);
    if (previous != current) {
        std::ostringstream message;
        message << "Long Sword spirit level changed: " << previous << " -> " << current;
        Log(message.str());
    }

}

std::uint8_t* FindPattern(const std::uint8_t* pattern, const char* mask, std::size_t length) {
    const HMODULE game = GetModuleHandleW(nullptr);
    MODULEINFO module{};
    if (!game || !GetModuleInformation(GetCurrentProcess(), game, &module, sizeof(module))) {
        return nullptr;
    }

    const auto start = static_cast<const std::uint8_t*>(module.lpBaseOfDll);
    const auto imageSize = static_cast<std::size_t>(module.SizeOfImage);
    if (imageSize < length) {
        return nullptr;
    }

    for (std::size_t i = 0; i <= imageSize - length; ++i) {
        bool matches = true;
        for (std::size_t j = 0; j < length; ++j) {
            if (mask[j] == 'x' && start[i + j] != pattern[j]) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return const_cast<std::uint8_t*>(start + i);
        }
    }
    return nullptr;
}

bool IsLongSwordController(void* controller) {
    // Player controllers are inline at player + 0x61C8. A player also owns a human controller
    // pointer at +0x12608. Only validated reads are used here; no player-only game function is
    // called on arbitrary entity callbacks.
    const auto ownerAddress = static_cast<std::uint8_t*>(controller) + 0x100;
    if (!IsReadable(ownerAddress, sizeof(void*))) {
        return false;
    }
    void* owner = *reinterpret_cast<void**>(ownerAddress);
    if (!owner) {
        return false;
    }
    if (static_cast<std::uint8_t*>(owner) + 0x61C8 != controller) {
        return false;
    }
    const auto humanControllerAddress = static_cast<std::uint8_t*>(owner) + 0x12608;
    if (!IsReadable(humanControllerAddress, sizeof(void*))) {
        return false;
    }
    void* humanController = *reinterpret_cast<void**>(humanControllerAddress);
    return humanController && IsReadable(humanController, sizeof(void*));
}

bool __fastcall DoActionHook(void* controller, ActionInfo* action) {
    const bool validAction = controller && action && IsReadable(action, sizeof(ActionInfo));
    if (validAction) {
        const auto sample = g_actionDiagnosticCount.fetch_add(1);
        if (sample < 32) {
            void* owner = nullptr;
            void* humanController = nullptr;
            const auto ownerAddress = static_cast<std::uint8_t*>(controller) + 0x100;
            if (IsReadable(ownerAddress, sizeof(void*))) {
                owner = *reinterpret_cast<void**>(ownerAddress);
            }
            if (owner) {
                const auto humanControllerAddress = static_cast<std::uint8_t*>(owner) + 0x12608;
                if (IsReadable(humanControllerAddress, sizeof(void*))) {
                    humanController = *reinterpret_cast<void**>(humanControllerAddress);
                }
            }
            std::ostringstream diagnostic;
            diagnostic << "Action hook sample " << (sample + 1) << ": set=" << action->actionSet
                       << " id=" << action->actionId << " controller=0x" << std::hex << std::uppercase
                       << reinterpret_cast<std::uintptr_t>(controller) << " owner=0x"
                       << reinterpret_cast<std::uintptr_t>(owner) << " humanController=0x"
                       << reinterpret_cast<std::uintptr_t>(humanController);
            Log(diagnostic.str());
        } else if (sample == 32) {
            Log("Action hook diagnostic sample limit reached");
        }
    }

    if (validAction && IsLongSwordController(controller)) {
        const ActionInfo current = *action;
        bool changed = false;
        bool isLockedPlayer = false;
        {
            std::scoped_lock lock(g_actionMutex);
            // Weapon actions use set 3. Once observed, keep this controller for the session so
            // monster/secondary controllers that happen to pass the structural test are ignored.
            if (!g_lockedPlayerController && current.actionSet == 3) {
                g_lockedPlayerController = controller;
                std::ostringstream locked;
                locked << "Player action controller locked at 0x" << std::hex << std::uppercase
                       << reinterpret_cast<std::uintptr_t>(controller);
                Log(locked.str());
            }
            isLockedPlayer = controller == g_lockedPlayerController;
            changed = isLockedPlayer &&
                      (controller != g_lastLongSwordController || !g_lastLongSwordAction ||
                       g_lastLongSwordAction->actionSet != current.actionSet ||
                       g_lastLongSwordAction->actionId != current.actionId);
            if (changed) {
                g_lastLongSwordController = controller;
                g_lastLongSwordAction = current;
                const bool normalAction = IsNormalModeChargingAction(current);
                if (g_valorMode.load() && normalAction) {
                    g_valorMode = false;
                    g_valorCharge = 0;
                    Log("Valor mode exited via normal-mode action; charging re-enabled");
                }
                if (normalAction) {
                    const auto oldCharge = g_valorCharge.load();
                    const auto newCharge = std::min<std::uint32_t>(100, oldCharge + 10);
                    g_valorCharge = newCharge;
                    std::ostringstream chargeMessage;
                    chargeMessage << "Valor charge: " << oldCharge << "% -> " << newCharge
                                  << "% from action 3:" << current.actionId;
                    if (newCharge == 100 && oldCharge < 100) {
                        chargeMessage << " (READY)";
                        ActivateValorMode(controller);
                    }
                    Log(chargeMessage.str());
                }
            }
        }
        if (changed && isLockedPlayer) {
            std::ostringstream message;
            message << "Long Sword action changed: set=" << current.actionSet
                    << " id=" << current.actionId << " controller=0x"
                    << std::hex << std::uppercase << reinterpret_cast<std::uintptr_t>(controller);
            Log(message.str());
        }
    }
    return g_originalDoAction(controller, action);
}

bool InstallActionHook() {
    constexpr std::uint8_t pattern[] = {
        0x48, 0x8D, 0x41, 0x07, 0x48, 0xC1, 0xE0, 0x04, 0x46, 0x3B, 0x04, 0x08
    };
    constexpr char mask[] = "xxxxxxxxxxxx";
    auto* match = FindPattern(pattern, mask, sizeof(pattern));
    if (!match) {
        Log("Action hook failed: ActionController::DoAction signature not found");
        return false;
    }

    void* target = match - 10; // SharpPluginLoader AddressRecords.json offset.
    const auto createStatus = MH_CreateHook(target, reinterpret_cast<void*>(DoActionHook),
                                            reinterpret_cast<void**>(&g_originalDoAction));
    if (createStatus != MH_OK) {
        std::ostringstream message;
        message << "Action hook failed: MH_CreateHook status=" << createStatus;
        Log(message.str());
        return false;
    }
    const auto enableStatus = MH_EnableHook(target);
    if (enableStatus != MH_OK) {
        std::ostringstream message;
        message << "Action hook failed: MH_EnableHook status=" << enableStatus;
        Log(message.str());
        return false;
    }

    std::ostringstream message;
    message << "Long Sword action hook installed at 0x" << std::hex << std::uppercase
            << reinterpret_cast<std::uintptr_t>(target);
    Log(message.str());
    return true;
}

void PollStateLogger() {
    const auto now = std::chrono::steady_clock::now();
    if (!g_stateLoggerEnabled || now < g_nextStatePoll) {
        return;
    }
    g_nextStatePoll = now + std::chrono::milliseconds(16);

    const auto address = g_stateAddress.load();
    if (address == 0 || !IsReadable(reinterpret_cast<const void*>(address), sizeof(std::uint32_t))) {
        return;
    }

    const auto state = *reinterpret_cast<const volatile std::uint32_t*>(address);
    if (!g_lastState) {
        g_lastState = state;
        std::ostringstream message;
        message << "State initial: " << state << " (0x" << std::hex << std::uppercase << state << ')';
        Log(message.str());
    } else if (*g_lastState != state) {
        std::ostringstream message;
        message << "State changed: " << *g_lastState << " -> " << state
                << " (0x" << std::hex << std::uppercase << *g_lastState << " -> 0x" << state << ')';
        Log(message.str());
        g_lastState = state;
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

    const bool f7Down = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
    if (f7Down && !g_f7WasDown) {
        LoadStateAddress();
        g_stateLoggerEnabled = true;
        Log("State logger reloaded with F7");
    }
    g_f7WasDown = f7Down;
    PollStateLogger();
    PollValorLifecycle();

    if (g_visible) {
        std::optional<ActionInfo> displayedAction;
        {
            std::scoped_lock lock(g_actionMutex);
            displayedAction = g_lastLongSwordAction;
        }
        char actionText[64]{};
        if (displayedAction) {
            snprintf(actionText, sizeof(actionText), "Action %d:%d",
                     displayedAction->actionSet, displayedAction->actionId);
        } else {
            snprintf(actionText, sizeof(actionText), "Action --:--");
        }

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        draw->AddText(ImGui::GetFont(), 22.0f, ImVec2(0.0f, 0.0f),
                      IM_COL32(255, 55, 55, 255), actionText);
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
    LoadStateAddress();

    for (int attempt = 0; attempt < 120; ++attempt) {
        const auto status = kiero::init(kiero::RenderType::D3D11);
        if (status == kiero::Status::Success) {
            const auto bindStatus = kiero::bind(8, reinterpret_cast<void**>(&g_originalPresent),
                                                reinterpret_cast<void*>(PresentHook));
            if (bindStatus == kiero::Status::Success) {
                Log("DX11 Present hook installed");
                InstallActionHook();
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
