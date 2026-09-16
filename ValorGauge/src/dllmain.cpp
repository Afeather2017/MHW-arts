#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <Psapi.h>

#include <atomic>
#include <algorithm>
#include <bit>
#include <chrono>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <intrin.h>
#include <mutex>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

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
using Aee050Fn = float(__fastcall*)(void* pool, int reactionType);
Aee050Fn g_originalAee050 = nullptr;
std::atomic_uint32_t g_poolLogCount = 0;
std::atomic<std::uintptr_t> g_lastPlayerPool{0};
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
// Force-action: F6 (or the clang detector) arms a one-shot flag; the property
// getter hook consumes it ON the game thread by calling DoAction directly -
// the same entry the FSM's own link execution uses. No game-memory writes.
std::atomic_bool g_forceArmed{false};
std::atomic_bool g_insideDoAction{false};
std::uint32_t g_forceSet = 3;
std::uint32_t g_forceId = 104;
float g_forceInvulnSec = 0.5f;
double g_counterWindowSec = 0.6;
std::atomic_uint32_t g_clangProbeLogCount{0};
// The mined clang latch: player+0xB228 sets when the deflect presentation
// starts (hit negated during a special evasion) and clears when it ends.
// Silent on whiffs - log-verified 0 changes across 3 whiff runs vs set+reset
// on every clang.
std::uint32_t g_clangLatchPrev = 0;
bool g_counterAutoForce = true;
std::chrono::steady_clock::time_point g_counterWindowUntil{};
bool g_counterCtrlWasDown = false;
std::atomic<std::uintptr_t> g_moduleBase{0};
bool g_clangLatch = false; // one force per 见切 action
void TryQueuedForceWrite(const void* entity);

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
    g_insideDoAction.store(true, std::memory_order_release);
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

            if (false && action->actionSet == 3 && action->actionId >= 98 && action->actionId <= 102) {
                auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
                std::ostringstream special;
                special << "SPECIAL action 3:" << action->actionId
                        << " controller=0x" << std::hex << std::uppercase
                        << reinterpret_cast<std::uintptr_t>(controller)
                        << " player=0x" << reinterpret_cast<std::uintptr_t>(player);
                auto read32 = [&](std::size_t offset, const char* name) {
                    const auto* address = player + offset;
                    if (IsReadable(address, sizeof(std::int32_t))) {
                        special << " " << name << "=" << std::dec
                                << *reinterpret_cast<const std::int32_t*>(address)
                                << "(0x" << std::hex << *reinterpret_cast<const std::int32_t*>(address) << ")";
                    } else {
                        special << " " << name << "=<unreadable>";
                    }
                };
                read32(0x6274, "fsmTarget");
                read32(0x6278, "fsmId");
                read32(0x6284, "deriveSet");
                read32(0x6288, "deriveId");
                Log(special.str().c_str());
            }
        } else if (sample == 32) {
            Log("Action hook diagnostic sample limit reached");
        }
    }

    if (validAction && action->actionSet == 3 && action->actionId >= 98 && action->actionId <= 102) {
        auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
        std::ostringstream special;
        special << "SPECIAL action 3:" << action->actionId << " controller=0x" << std::hex << std::uppercase
                << reinterpret_cast<std::uintptr_t>(controller) << " player=0x" << reinterpret_cast<std::uintptr_t>(player);
        auto read32 = [&](std::size_t offset, const char* name) {
            const auto* address = player + offset;
            if (IsReadable(address, sizeof(std::int32_t))) {
                const auto value = *reinterpret_cast<const std::int32_t*>(address);
                special << " " << name << "=" << std::dec << value << "(0x" << std::hex << value << ")";
            }
        };
        read32(0x6274, "fsmTarget"); read32(0x6278, "fsmId");
        read32(0x6284, "deriveSet"); read32(0x6288, "deriveId");
        Log(special.str().c_str());
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
                const auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
                std::ostringstream addrLog;
                addrLog << "ADDR player=0x" << std::hex << std::uppercase
                        << reinterpret_cast<std::uintptr_t>(player);
                const auto* weaponSlot = player + 0x76B0;
                if (IsReadable(weaponSlot, sizeof(void*))) {
                    const void* weapon = *reinterpret_cast<void* const*>(weaponSlot);
                    addrLog << " weaponSlot=0x"
                            << reinterpret_cast<std::uintptr_t>(weaponSlot)
                            << " weapon=0x" << reinterpret_cast<std::uintptr_t>(weapon);
                    if (weapon && IsReadable(weapon, 0x2400)) {
                        addrLog << " spiritLevel="
                                << std::dec
                                << *reinterpret_cast<const std::int32_t*>(
                                       static_cast<const std::uint8_t*>(weapon) + 0x2370)
                                << " invulnTimer="
                                << *reinterpret_cast<const float*>(
                                       static_cast<const std::uint8_t*>(weapon) + 0x2388);
                    }
                } else {
                    addrLog << " weaponSlot=<unreadable>";
                }
                Log(addrLog.str());
            }
            isLockedPlayer = controller == g_lockedPlayerController;
            changed = isLockedPlayer &&
                      (controller != g_lastLongSwordController || !g_lastLongSwordAction ||
                       g_lastLongSwordAction->actionSet != current.actionSet ||
                       g_lastLongSwordAction->actionId != current.actionId);
            if (changed) {
                g_lastLongSwordController = controller;
                g_lastLongSwordAction = current;
                // Re-arm the clang latch only when a NEW foresight slash starts;
                // resetting on every action change caused a force loop (the
                // injected window kept producing rejections during the counter).
                if (current.actionSet == 3 && current.actionId == 86) {
                    g_clangLatch = false;
                }
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
    const bool result = g_originalDoAction(controller, action) != 0;
    g_insideDoAction.store(false, std::memory_order_release);
    return result;
}

// Hook the damage-layer defense query (FUN_140aee050, RVA 0xAEE050). Every
// arbitration hands us the pool pointer actually in use; record the one that
// belongs to the local player so DIFF watches the real thing.
// --- 见切 hit-path diagnostics -----------------------------------------------
// During 见切 (3:86): log every pool query and every damage-calc entry so a
// whiff / clang / direct-eat comparison reveals the exact negation signal.
std::atomic_uint32_t g_fsdLogCount{0};
bool ActionIsForesightSlash() {
    std::scoped_lock lock(g_actionMutex);
    auto& a = g_lastLongSwordAction;
    return a && a->actionSet == 3 && a->actionId == 86;
}

float __fastcall Aee050Hook(void* pool, int reactionType) {
    const float result = g_originalAee050 ? g_originalAee050(pool, reactionType) : 0.0f;
    auto* p = static_cast<std::uint8_t*>(pool);
    if (ActionIsForesightSlash() && g_fsdLogCount.load() < 96) {
        ++g_fsdLogCount;
        std::ostringstream m;
        m << "FSDQ type=" << std::dec << reactionType << " ret=" << result
          << std::hex << std::uppercase << " caller=0x"
          << reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - g_moduleBase.load()
          << " pool=0x" << reinterpret_cast<std::uintptr_t>(pool);
        Log(m.str());
    }
    if (p && IsReadable(p, 0x180) && g_poolLogCount.load() < 24) {
        void* playerPtr = nullptr;
        {
            std::scoped_lock lock(g_actionMutex);
            if (g_lockedPlayerController) {
                playerPtr = static_cast<std::uint8_t*>(g_lockedPlayerController) - 0x61C8;
            }
        }
        if (playerPtr && *reinterpret_cast<void* const*>(p + 8) == playerPtr) {
            g_lastPlayerPool = reinterpret_cast<std::uintptr_t>(pool);
            if (g_poolLogCount.fetch_add(1) < 24) {
                std::ostringstream m;
                m << "POOLQ pool=0x" << std::hex << std::uppercase
                  << reinterpret_cast<std::uintptr_t>(pool) << " type=" << std::dec
                  << reactionType << " ret=" << result << std::hex << " [94]=0x"
                  << *reinterpret_cast<const std::uint32_t*>(p + 0x94) << " [98]=0x"
                  << *reinterpret_cast<const std::uint32_t*>(p + 0x98) << " [9C]=0x"
                  << *reinterpret_cast<const std::uint32_t*>(p + 0x9C) << " [A0]=0x"
                  << *reinterpret_cast<const std::uint32_t*>(p + 0xA0);
                Log(m.str());
            }
        }
    }
    return result;
}

// --- 刚1 clang detector (hit dispatcher) --------------------------------------
// FUN_141f71960 routes every hit attempt; 0xD returns are the per-frame
// ambient no-collision checks. Live diagnostics (log-verified): during 见切
// (3:86) a negated hit (the clang) returns a NON-0xD code and the damage calc
// (FUN_141f66dd0) is never entered; a real hit enters the damage calc. So:
// clang = non-0xD return while action is 3:86 and no damage processing ran
// for this dispatch. Fires on the game thread at the clang frame.
using DmgCalcFn = long long(__fastcall*)(long long, long long, std::uint32_t*);
DmgCalcFn g_originalDmgCalc = nullptr;
bool g_damageThisDispatch = false; // game thread only

long long __fastcall DmgCalcHook(long long a1, long long a2, std::uint32_t* a3) {
    g_damageThisDispatch = true;
    return g_originalDmgCalc(a1, a2, a3);
}

using HitDispatchFn = std::int32_t* (__fastcall*)(std::int64_t*, std::int64_t*);
HitDispatchFn g_originalHitDispatch = nullptr;
std::atomic_uint32_t g_clangCount{0};

std::int32_t* __fastcall HitDispatchHook(std::int64_t* a1, std::int64_t* a2) {
    g_damageThisDispatch = false;
    // The dispatcher resolves the interaction type at entry via vtable+8;
    // type 6 (== case 0) is the damage path. Mirror that read so a damage-type
    // interaction that never reached the damage calc = a negated hit.
    long long interactType = -1;
    if (a2 && IsReadable(a2, sizeof(void*))) {
        void* obj = *reinterpret_cast<void**>(a2);
        if (obj && IsReadable(obj, 0x10)) {
            void* vtbl = *reinterpret_cast<void**>(obj);
            if (IsReadable(reinterpret_cast<char*>(vtbl) + 8, sizeof(void*))) {
                                using TypeGetterFn = long long(__fastcall*)(std::int64_t*);
                auto fn = *reinterpret_cast<TypeGetterFn*>(
                    reinterpret_cast<char*>(vtbl) + 8);
                if (IsReadable(reinterpret_cast<const void*>(fn), 1)) {
                    interactType = fn(a2);
                }
                (void)0;
            }
        }
    }
    auto* result = g_originalHitDispatch(a1, a2);
    const auto code = reinterpret_cast<std::uintptr_t>(result);
    const bool attackRejected = interactType == 6 && code != 0xD &&
                                !g_damageThisDispatch;
    if (attackRejected && ActionIsForesightSlash()) {
        // A monster attack was just negated inside the parry window: open the
        // counter window and pre-inject the no-hit coverage.
        if (!g_clangLatch) {
            g_clangLatch = true;
            g_counterWindowUntil = std::chrono::steady_clock::now() +
                std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<double>(g_counterWindowSec));
            auto pool = g_lastPlayerPool.load();
            if (pool != 0 && g_forceInvulnSec > 0.0f) {
                auto* window = reinterpret_cast<float*>(pool + 0xE4);
                if (IsWritable(window, sizeof(float))) {
                    *window = *window > g_forceInvulnSec ? *window : g_forceInvulnSec;
                }
            }
            Log("COUNTER WINDOW OPEN (attack negated during 见切) - press the "
                "spirit key to counter");
        }
        if (g_fsdLogCount.load() < 4096) {
            ++g_fsdLogCount;
            const auto clangN = g_clangCount.fetch_add(1) + 1;
            if (clangN <= 16) {
                std::ostringstream m;
                m << "HITDISPATCH #" << std::dec << clangN << ": rejected during 见切"
                  << std::hex << std::uppercase << " (type=" << interactType
                  << " code=0x" << code << ")";
                Log(m.str());
            }
        }
    }
    return result;
}

// --- 見切り回避成功後の攻撃ヒット condition evaluator hook ---------------------
void OpenCounterWindow(const char* reason);
// FUN_14118bda0 resolves and evaluates FSM condition-tree VariableNodes; the
// 見切り回避成功後の攻撃ヒット node carries the property-name pointer
// 0x1430BA8F0 in its VariableInfo. Scan the node for that pointer: matching
// calls evaluate THE vanilla success latch, and the return value is its
// current state. A nonzero return = the vanilla 刚1 branch condition passed.
constexpr std::uintptr_t kForesightSuccessName = 0x1430BA8F0;
using CondEvalFn = long long(__fastcall*)(unsigned int, long long, long long);
CondEvalFn g_originalCondEval = nullptr;
std::atomic_uint32_t g_condEvalLogCount{0};

long long __fastcall CondEvalHook(unsigned int a1, long long a2, long long a3) {
    // Empirical layout probe: find pointers into the property-name string
    // table (0x1430b0000..0x1430c0000) inside a2/a3 and log where they sit.
    if (g_condEvalLogCount.load() < 24 && ActionIsForesightSlash()) {
        auto scanNames = [&](long long base, const char* tag, std::ostringstream& m) {
            if (!base || !IsReadable(reinterpret_cast<const void*>(base), 0x100)) {
                return;
            }
            const auto* words = reinterpret_cast<const void* const*>(base);
            for (int off = 0; off < 0x100; off += (int)sizeof(void*)) {
                const auto v = reinterpret_cast<std::uintptr_t>(words[off / sizeof(void*)]);
                if (v >= 0x1430b0000 && v < 0x1430c0000) {
                    m << " " << tag << "+0x" << std::hex << std::uppercase << off << "=0x" << v;
                }
            }
        };
        std::ostringstream m;
        m << "CONDSCAN" << std::hex << std::uppercase << " a2=0x" << a2 << " a3=0x" << a3;
        scanNames(a2, "a2", m);
        scanNames(a3, "a3", m);
        Log(m.str());
        ++g_condEvalLogCount;
    }
    bool isForesightSuccess = false;
    if (a3 && IsReadable(reinterpret_cast<const void*>(a3), 0x100)) {
        const auto* node = reinterpret_cast<const void* const*>(a3);
        for (int off = 0; off < 0x100; off += (int)sizeof(void*)) {
            if (node[off / sizeof(void*)] == reinterpret_cast<const void*>(kForesightSuccessName)) {
                isForesightSuccess = true;
                break;
            }
        }
    }
    if (a2 && IsReadable(reinterpret_cast<const void*>(a2), 0x100)) {
        const auto* node = reinterpret_cast<const void* const*>(a2);
        for (int off = 0; off < 0x100; off += (int)sizeof(void*)) {
            if (node[off / sizeof(void*)] == reinterpret_cast<const void*>(kForesightSuccessName)) {
                isForesightSuccess = true;
                break;
            }
        }
    }
    const long long result = g_originalCondEval(a1, a2, a3);
    if (isForesightSuccess) {
        std::ostringstream m;
        m << "COND631 evaluated -> " << std::dec << result;
        Log(m.str());
    }
    if (isForesightSuccess && result != 0) {
        // The vanilla success+hit latch just evaluated true: this is the exact
        // 刚1 trigger moment. Open the counter window and arm the force.
        if (!g_clangLatch) {
            g_clangLatch = true;
        }
        OpenCounterWindow("vanilla success condition evaluated TRUE");
        if (!g_forceArmed.exchange(true)) {
            Log("COUNTER INPUT not needed: vanilla condition passed - force armed");
        }
    }
    return result;
}

bool InstallCondEvalHook() {
    void* target = reinterpret_cast<std::uint8_t*>(g_moduleBase.load()) + 0x118BDA0;
    if (MH_CreateHook(target, reinterpret_cast<void*>(&CondEvalHook),
                      reinterpret_cast<void**>(&g_originalCondEval)) != MH_OK ||
        MH_EnableHook(target) != MH_OK) {
        Log("Condition evaluator hook failed");
        return false;
    }
    Log("Condition evaluator hook installed (RVA 0x118BDA0)");
    return true;
}

// --- Wwise sound-event logging (叮 source hunt) --------------------------------
// AK::SoundEngine::PostEvent wrappers in the current build. Log every posted
// event (id + name) during 见切 so a clang run vs a whiff run reveals the
// deflect sound's event id and its posting code path.
std::atomic_uint32_t g_wwiseLogCount{0};

using PostEventIdFn = unsigned long long(__fastcall*)(unsigned int, unsigned long long,
                                                      unsigned int, void*, void*,
                                                      int, void*);
PostEventIdFn g_originalPostEventId = nullptr;

unsigned long long __fastcall PostEventIdHook(unsigned int eventId, unsigned long long objId,
                                              unsigned int flags, void* callback,
                                              void* cookie, int extCount, void* extSrc) {
    if (ActionIsForesightSlash() && g_wwiseLogCount.load() < 512) {
        ++g_wwiseLogCount;
        std::ostringstream m;
        m << "WWISEEV id=" << eventId << std::hex << std::uppercase << " obj=0x"
          << objId << " caller=0x"
          << reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - g_moduleBase.load();
        Log(m.str());
    }
    return g_originalPostEventId(eventId, objId, flags, callback, cookie, extCount, extSrc);
}

using PostEventNameFn = unsigned long long(__fastcall*)(const char*, unsigned long long,
                                                        unsigned int, void*, void*,
                                                        int, void*);
PostEventNameFn g_originalPostEventName = nullptr;

// The game's own sound-post wrapper (FUN_1426d4c50): (entity, eventId,
// gameObjectId, flags) -> AK::SoundEngine::PostEvent. Event 1086158451 is the
// 见切 deflect clang (whiff-vs-clang diff, log-verified). Logging its direct
// caller pinpoints the success-detection code.
constexpr unsigned int kClangSoundEvent = 1086158451u;
using SoundPostFn = unsigned long long(__fastcall*)(void*, unsigned int,
                                                    unsigned int, unsigned int);
SoundPostFn g_originalSoundPost = nullptr;
std::atomic_uint32_t g_clangSoundLogCount{0};

unsigned long long __fastcall SoundPostHook(void* a1, unsigned int eventId,
                                            unsigned int gameObjectId, unsigned int flags);

unsigned long long __fastcall SoundPostHook(void* a1, unsigned int eventId,
                                            unsigned int gameObjectId, unsigned int flags);

// The concrete "post sound" virtual (audio singleton vtable slot 0x150),
// resolved at first clang and MinHooked. Its edx == the sound event id, so
// filtering on 1086158451 gives an exact, game-thread clang detector.
using ClangSoundVirtualFn = void* (__fastcall*)(void* audio, unsigned int eventId,
                                                unsigned int a3, unsigned int a4);
ClangSoundVirtualFn g_originalClangSoundVirtual = nullptr;
bool g_clangSoundVirtualInstalled = false;

void* __fastcall ClangSoundVirtualHook(void* audio, unsigned int eventId,
                                       unsigned int a3, unsigned int a4) {
    if (eventId == kClangSoundEvent && !g_clangLatch) {
        g_clangLatch = true;
        auto pool = g_lastPlayerPool.load();
        if (pool != 0 && g_forceInvulnSec > 0.0f) {
            auto* window = reinterpret_cast<float*>(pool + 0xE4);
            if (IsWritable(window, sizeof(float))) {
                *window = *window > g_forceInvulnSec ? *window : g_forceInvulnSec;
            }
        }
        if (!g_forceArmed.exchange(true)) {
            std::ostringstream m;
            m << "CLANG DETECTED (sound post, event " << std::dec << eventId
              << ") - counter force armed, no-hit window extended";
            Log(m.str());
        }
    }
    return g_originalClangSoundVirtual(audio, eventId, a3, a4);
}

bool InstallClangSoundVirtualHook() {
    auto* singletonPtr = reinterpret_cast<std::uint8_t**>(g_moduleBase.load() + 0x51C458);
    if (!IsReadable(singletonPtr, sizeof(void*)) || !*singletonPtr) {
        Log("CLANG hook: audio singleton unavailable");
        return false;
    }
    auto** vtable = *reinterpret_cast<void***>(*singletonPtr);
    if (!IsReadable(vtable, 0x158)) {
        Log("CLANG hook: audio vtable unreadable");
        return false;
    }
    void* target = vtable[0x150 / sizeof(void*)];
    if (!target) {
        Log("CLANG hook: vtable slot empty");
        return false;
    }
    if (MH_CreateHook(target, reinterpret_cast<void*>(&ClangSoundVirtualHook),
                      reinterpret_cast<void**>(&g_originalClangSoundVirtual)) != MH_OK ||
        MH_EnableHook(target) != MH_OK) {
        Log("CLANG hook: MH_CreateHook failed");
        return false;
    }
    g_clangSoundVirtualInstalled = true;
    std::ostringstream m;
    m << "CLANG hook installed on audio vtable+0x150 at 0x" << std::hex << std::uppercase
      << reinterpret_cast<std::uintptr_t>(target);
    Log(m.str());
    return true;
}

unsigned long long __fastcall SoundPostHook(void* a1, unsigned int eventId,
                                            unsigned int gameObjectId, unsigned int flags) {
    if (eventId == kClangSoundEvent) {
        if (g_clangSoundLogCount.load() < 8) {
            ++g_clangSoundLogCount;
            std::ostringstream m;
            m << "CLANGSOUND #" << std::dec << g_clangSoundLogCount.load()
              << std::hex << std::uppercase << " caller=0x"
              << reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - g_moduleBase.load()
              << " a1=0x" << reinterpret_cast<std::uintptr_t>(a1)
              << " obj=0x" << gameObjectId;
            Log(m.str());
        }
        // NOTE: event 1086158451 turned out to be a shared "metal blade ring"
        // sound (draw attacks post it too), NOT clang-exclusive. No triggering
        // from audio. The counter window comes from the hit dispatcher instead.
    }
    return g_originalSoundPost(a1, eventId, gameObjectId, flags);
}

unsigned long long __fastcall PostEventNameHook(const char* name, unsigned long long objId,
                                                unsigned int flags, void* callback,
                                                void* cookie, int extCount, void* extSrc) {
    if (ActionIsForesightSlash() && g_wwiseLogCount.load() < 512) {
        ++g_wwiseLogCount;
        char nameBuf[64] = {};
        if (name && IsReadable(name, 1)) {
            size_t n = 0;
            while (n < sizeof(nameBuf) - 1 && IsReadable(name + n, 1) && name[n]) {
                nameBuf[n] = name[n];
                ++n;
            }
        }
        std::ostringstream m;
        m << "WWISEEV name='" << nameBuf << "'" << std::hex << std::uppercase
          << " obj=0x" << objId << " caller=0x"
          << reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - g_moduleBase.load();
        Log(m.str());
    }
    return g_originalPostEventName(name, objId, flags, callback, cookie, extCount, extSrc);
}

bool InstallPoolQueryHook() {
    const HMODULE game = GetModuleHandleW(nullptr);
    MODULEINFO module{};
    if (!game || !GetModuleInformation(GetCurrentProcess(), game, &module, sizeof(module))) {
        Log("Pool query hook failed: module info unavailable");
        return false;
    }
    g_moduleBase = reinterpret_cast<std::uintptr_t>(module.lpBaseOfDll);
    void* target = static_cast<std::uint8_t*>(module.lpBaseOfDll) + 0xAEE050;
    if (MH_CreateHook(target, reinterpret_cast<void*>(&Aee050Hook),
                      reinterpret_cast<void**>(&g_originalAee050)) != MH_OK) {
        Log("Pool query hook failed: MH_CreateHook");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        Log("Pool query hook failed: MH_EnableHook");
        return false;
    }
    if (MH_CreateHook(static_cast<std::uint8_t*>(module.lpBaseOfDll) + 0x1F66DD0,
                      reinterpret_cast<void*>(&DmgCalcHook),
                      reinterpret_cast<void**>(&g_originalDmgCalc)) != MH_OK ||
        MH_EnableHook(static_cast<std::uint8_t*>(module.lpBaseOfDll) + 0x1F66DD0) != MH_OK) {
        Log("Damage calc hook failed");
    } else {
        Log("Damage calc hook installed (RVA 0x1F66DD0)");
    }
    if (MH_CreateHook(static_cast<std::uint8_t*>(module.lpBaseOfDll) + 0x1F71960,
                      reinterpret_cast<void*>(&HitDispatchHook),
                      reinterpret_cast<void**>(&g_originalHitDispatch)) != MH_OK ||
        MH_EnableHook(static_cast<std::uint8_t*>(module.lpBaseOfDll) + 0x1F71960) != MH_OK) {
        Log("Hit dispatcher hook failed");
    } else {
        Log("Hit dispatcher hook installed (RVA 0x1F71960)");
    }
    if (MH_CreateHook(static_cast<std::uint8_t*>(module.lpBaseOfDll) + 0x28B8DD0,
                      reinterpret_cast<void*>(&PostEventIdHook),
                      reinterpret_cast<void**>(&g_originalPostEventId)) != MH_OK ||
        MH_EnableHook(static_cast<std::uint8_t*>(module.lpBaseOfDll) + 0x28B8DD0) != MH_OK) {
        Log("Wwise PostEvent(id) hook failed");
    } else {
        Log("Wwise PostEvent(id) hook installed (RVA 0x28B8DD0)");
    }
    if (MH_CreateHook(static_cast<std::uint8_t*>(module.lpBaseOfDll) + 0x26D4C50,
                      reinterpret_cast<void*>(&SoundPostHook),
                      reinterpret_cast<void**>(&g_originalSoundPost)) != MH_OK ||
        MH_EnableHook(static_cast<std::uint8_t*>(module.lpBaseOfDll) + 0x26D4C50) != MH_OK) {
        Log("Sound post hook failed");
    } else {
        Log("Sound post hook installed (RVA 0x26D4C50)");
    }
    InstallCondEvalHook();
    if (MH_CreateHook(static_cast<std::uint8_t*>(module.lpBaseOfDll) + 0x28B8FE0,
                      reinterpret_cast<void*>(&PostEventNameHook),
                      reinterpret_cast<void**>(&g_originalPostEventName)) != MH_OK ||
        MH_EnableHook(static_cast<std::uint8_t*>(module.lpBaseOfDll) + 0x28B8FE0) != MH_OK) {
        Log("Wwise PostEvent(name) hook failed");
    } else {
        Log("Wwise PostEvent(name) hook installed (RVA 0x28B8FE0)");
    }
    std::ostringstream m;
    m << "Pool query hook installed at 0x" << std::hex << std::uppercase
      << reinterpret_cast<std::uintptr_t>(target);
    Log(m.str());
    return true;
}

// --- F11 FSM property-access histogram --------------------------------------
// FUN_141922190 / FUN_1419221f0 / FUN_141922240 are the FSM condition-tree
// property getters: (entity, id, index[, default]) -> int16 counter stored at
// row+0x24+index*2 of the entity's property row (the float variant scales the
// counter by a fixed frame constant). Whatever the game consults to decide
// "success dodge" (the clang + blue flash) must be read here only in captures
// where a hit actually lands inside the defense window. F11 dumps the
// accumulated (entity, id, index) histogram with caller RVAs, then resets it.
using PropGetIntFn = int (__fastcall*)(void* entity, void* id, std::uint32_t index, int defaultValue);
using PropGetBoolFn = bool (__fastcall*)(void* entity, void* id, std::uint32_t index);
using PropGetFloatFn = float (__fastcall*)(void* entity, void* id, std::uint32_t index, std::uint64_t defaultBits);
using PropRegistryLookupFn = void* (__fastcall*)(void* table, std::int32_t key);
using PropNameFn = const char* (__fastcall*)(std::int32_t id);

PropGetIntFn g_originalPropInt = nullptr;
PropGetBoolFn g_originalPropBool = nullptr;
PropGetFloatFn g_originalPropFloat = nullptr;
bool g_f11WasDown = false;

struct PropCallKey {
    const void* entity;
    std::uint32_t id;
    std::uint32_t index;
    std::uint32_t kind; // 0 = int, 1 = bool, 2 = float
    bool operator==(const PropCallKey& other) const {
        return entity == other.entity && id == other.id &&
               index == other.index && kind == other.kind;
    }
};

struct PropCallKeyHash {
    std::size_t operator()(const PropCallKey& key) const noexcept {
        std::uint64_t h = reinterpret_cast<std::uint64_t>(key.entity);
        h ^= h >> 33;
        h *= 0xFF51AFD7ED558CCDull;
        h ^= (h >> 33) ^ (static_cast<std::uint64_t>(key.id) << 1);
        h *= 0xC4CEB9FE1A85EC53ull;
        h ^= (h >> 33) ^ (static_cast<std::uint64_t>(key.index) << 17) ^
             static_cast<std::uint64_t>(key.kind);
        return static_cast<std::size_t>(h);
    }
};

struct PropCallStats {
    std::uint32_t count = 0;
    std::uintptr_t callers[6] = {};
    std::uint32_t callerCount = 0;
    // Highest return value seen (bit-cast for the float variant). Reveals
    // whether a condition property ever reads true during a capture.
    std::int32_t maxRet = 0;
};

std::mutex g_propMutex;
std::unordered_map<PropCallKey, PropCallStats, PropCallKeyHash> g_propCalls;
bool g_propOverflow = false;
std::atomic_bool g_propRegistryDumped{false};

void RecordPropCall(const void* entity, std::uint32_t id, std::uint32_t index,
                    std::uint32_t kind, const void* caller, std::int32_t ret) {
    // Benign racy read: the pointer is only used to tag the player in the dump.
    const void* controller = g_lockedPlayerController;
    const void* player = controller ? static_cast<const std::uint8_t*>(controller) - 0x61C8
                                    : nullptr;
    if (entity != player && g_propCalls.size() > 20000) {
        g_propOverflow = true;
        return;
    }
    const auto callerRva = caller
        ? reinterpret_cast<std::uintptr_t>(caller) - g_moduleBase.load()
        : 0;
    std::scoped_lock lock(g_propMutex);
    auto& stats = g_propCalls[{entity, id, index, kind}];
    ++stats.count;
    if (ret > stats.maxRet) {
        stats.maxRet = ret;
    }
    bool knownCaller = false;
    for (std::uint32_t i = 0; i < stats.callerCount; ++i) {
        knownCaller = knownCaller || stats.callers[i] == callerRva;
    }
    if (!knownCaller && stats.callerCount < 6) {
        stats.callers[stats.callerCount++] = callerRva;
    }
}

// Game-thread consumer of an armed force request: calls DoAction directly -
// the same entry the FSM's own link execution uses, so a mid-action interrupt
// is architecturally identical to what the game itself does. Runs inside the
// property-getter hook (game update thread).
void TryQueuedForceWrite(const void* entity) {
    if (!g_forceArmed.load(std::memory_order_acquire) || g_insideDoAction.load()) {
        return;
    }
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
        return;
    }
    const void* player = static_cast<const std::uint8_t*>(controller) - 0x61C8;
    if (entity != player) {
        return;
    }
    g_forceArmed.store(false, std::memory_order_release);

    std::optional<ActionInfo> current;
    {
        std::scoped_lock lock(g_actionMutex);
        current = g_lastLongSwordAction;
    }
    ActionInfo force{};
    force.actionSet = static_cast<std::int32_t>(g_forceSet);
    force.actionId = static_cast<std::int32_t>(g_forceId);
    std::ostringstream m;
    m << "FORCE: DoAction(3:" << g_forceId << ") mid-action";
    if (current) {
        m << " (interrupting 3:" << current->actionId << ')';
    }
    const bool ok = g_originalDoAction(controller, &force) != 0;
    m << " result=" << (ok ? "accepted" : "rejected");
    Log(m.str());
}

int __fastcall PropIntHook(void* entity, void* id, std::uint32_t index, int defaultValue) {
    TryQueuedForceWrite(entity);
    const int result = g_originalPropInt(entity, id, index, defaultValue);
    RecordPropCall(entity, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(id)),
                   index, 0, _ReturnAddress(), result);
    return result;
}

bool __fastcall PropBoolHook(void* entity, void* id, std::uint32_t index) {
    const bool result = g_originalPropBool(entity, id, index);
    RecordPropCall(entity, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(id)),
                   index, 1, _ReturnAddress(), result ? 1 : 0);
    return result;
}

float __fastcall PropFloatHook(void* entity, void* id, std::uint32_t index, std::uint64_t defaultBits) {
    const float result = g_originalPropFloat(entity, id, index, defaultBits);
    std::int32_t bits = 0;
    std::memcpy(&bits, &result, sizeof(bits));
    RecordPropCall(entity, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(id)),
                   index, 2, _ReturnAddress(), bits);
    return result;
}

// FUN_141979770 resolves a property id to its display name from the game's
// own name tables; SEH-wrapped because it runs before we know table state.
const char* SafePropName(std::int32_t id) {
    const auto nameFn = reinterpret_cast<PropNameFn>(g_moduleBase.load() + 0x1979770);
    __try {
        return nameFn(id);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

// One-time enumeration of the global property registry (object at RVA
// 0x5012048, count at +0xB8, row lookup at RVA 0x462700) so histogram ids
// can be matched against display names.
void DumpPropRegistry() {
    if (g_propRegistryDumped.exchange(true)) {
        return;
    }
    auto* table = reinterpret_cast<std::uint8_t*>(g_moduleBase.load() + 0x5012048);
    if (!IsReadable(table, 0xC0)) {
        Log("REGISTRY unavailable: table unreadable");
        return;
    }
    const auto count = *reinterpret_cast<const std::int32_t*>(table + 0xB8);
    std::ostringstream header;
    header << "REGISTRY BEGIN count=" << std::dec << count;
    Log(header.str());
    if (count > 0) {
        const auto lookup = reinterpret_cast<PropRegistryLookupFn>(g_moduleBase.load() + 0x462700);
        const std::int32_t limit = count > 1500 ? 1500 : count;
        for (std::int32_t i = 0; i < limit; ++i) {
            const char* name = SafePropName(i);
            void* row = lookup(table, i);
            std::ostringstream line;
            line << "REG id=" << std::dec << i << std::hex << std::uppercase
                 << " row=0x" << reinterpret_cast<std::uintptr_t>(row)
                 << " name='" << (name ? name : "?") << "'";
            Log(line.str());
        }
    }
    Log("REGISTRY END");
}

void DumpPropHistogram() {
    std::vector<std::pair<PropCallKey, PropCallStats>> entries;
    {
        std::scoped_lock lock(g_propMutex);
        entries.assign(g_propCalls.begin(), g_propCalls.end());
        g_propCalls.clear();
    }
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) {
                  if (a.first.kind != b.first.kind) return a.first.kind < b.first.kind;
                  if (a.first.id != b.first.id) return a.first.id < b.first.id;
                  if (a.first.index != b.first.index) return a.first.index < b.first.index;
                  return a.first.entity < b.first.entity;
              });

    const void* controller = g_lockedPlayerController;
    const void* player = controller ? static_cast<const std::uint8_t*>(controller) - 0x61C8
                                    : nullptr;

    std::ostringstream header;
    header << "PROPHIST BEGIN entries=" << std::dec << entries.size()
           << (g_propOverflow ? " OVERFLOW(some non-player entries dropped)" : "");
    g_propOverflow = false;
    Log(header.str());

    // Property names for the id space seen in this capture (id -> name).
    std::unordered_map<std::uint32_t, const char*> names;
    for (const auto& entry : entries) {
        if (names.size() >= 80 || names.count(entry.first.id)) {
            continue;
        }
        names.emplace(entry.first.id, SafePropName(static_cast<std::int32_t>(entry.first.id)));
    }
    for (const auto& [id, name] : names) {
        if (name && *name) {
            std::ostringstream line;
            line << "PNAME id=" << std::dec << id << " name='" << name << "'";
            Log(line.str());
        }
    }

    constexpr std::size_t kMaxLines = 4000;
    std::size_t printed = 0;
    for (const auto& [key, stats] : entries) {
        if (printed >= kMaxLines) {
            std::ostringstream truncated;
            truncated << "PROPHIST TRUNCATED remaining=" << std::dec
                      << entries.size() - printed;
            Log(truncated.str());
            break;
        }
        std::ostringstream line;
        line << "PROP kind=" << (key.kind == 0 ? "INT" : key.kind == 1 ? "BOOL" : "FLT")
             << " id=" << std::dec << key.id << std::hex << std::uppercase
             << "(0x" << key.id << ") idx=" << std::dec << key.index
             << " ent=" << (key.entity == player
                                ? "PLAYER"
                                : ([&] {
                                      std::ostringstream e;
                                      e << std::hex << std::uppercase << "0x"
                                        << reinterpret_cast<std::uintptr_t>(key.entity);
                                      return e.str();
                                  })())
             << " cnt=" << std::dec << stats.count << std::hex << std::uppercase
             << " max=0x" << stats.maxRet << " callers=[";
        for (std::uint32_t i = 0; i < stats.callerCount; ++i) {
            if (i) {
                line << ',';
            }
            line << std::hex << std::uppercase << "0x" << stats.callers[i];
        }
        line << ']';
        Log(line.str());
        ++printed;
    }
    Log("PROPHIST END");
}

void PollPropHistogram() {
    const bool f11Down = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
    if (f11Down && !g_f11WasDown) {
        DumpPropRegistry();
        DumpPropHistogram();
    }
    g_f11WasDown = f11Down;
}

bool InstallPropHistogramHooks() {
    const HMODULE game = GetModuleHandleW(nullptr);
    MODULEINFO module{};
    if (!game || !GetModuleInformation(GetCurrentProcess(), game, &module, sizeof(module))) {
        Log("Property histogram hooks failed: module info unavailable");
        return false;
    }
    g_moduleBase = reinterpret_cast<std::uintptr_t>(module.lpBaseOfDll);

    struct HookEntry {
        std::uintptr_t rva;
        void* detour;
        void** original;
        const char* label;
    };
    const HookEntry entries[] = {
        {0x1922190, reinterpret_cast<void*>(&PropIntHook),
         reinterpret_cast<void**>(&g_originalPropInt), "int"},
        {0x19221F0, reinterpret_cast<void*>(&PropBoolHook),
         reinterpret_cast<void**>(&g_originalPropBool), "bool"},
        {0x1922240, reinterpret_cast<void*>(&PropFloatHook),
         reinterpret_cast<void**>(&g_originalPropFloat), "float"},
    };
    bool allOk = true;
    for (const auto& entry : entries) {
        void* target = reinterpret_cast<std::uint8_t*>(g_moduleBase.load()) + entry.rva;
        if (MH_CreateHook(target, entry.detour, entry.original) != MH_OK ||
            MH_EnableHook(target) != MH_OK) {
            std::ostringstream m;
            m << "Property histogram hook failed (" << entry.label << ") at 0x"
              << std::hex << std::uppercase
              << reinterpret_cast<std::uintptr_t>(target);
            Log(m.str());
            allOk = false;
        }
    }
    if (allOk) {
        Log("Property histogram hooks installed (int/bool/float), F11 dumps");
    }
    return allOk;
}

// Optional nativePC\plugins\ValorGauge.force-action.txt:
// "<set> <id> [invulnSec]" (invulnSec: no-hit window injected at the clang).
void LoadForceActionConfig() {
    std::ifstream input("nativePC\\plugins\\ValorGauge.force-action.txt");
    std::int32_t set = 0;
    std::int32_t id = 0;
    float invulnSec = 0.0f;
    if (input >> set >> id && set >= 0 && id >= 0) {
        g_forceSet = static_cast<std::uint32_t>(set);
        g_forceId = static_cast<std::uint32_t>(id);
    }
    if (input >> invulnSec && invulnSec >= 0.0f && invulnSec <= 3.0f) {
        g_forceInvulnSec = invulnSec;
    }
    std::ifstream autoCfg("nativePC\plugins\ValorGauge.counter-auto.txt");
    int autoForce = 1;
    if (autoCfg >> autoForce) {
        g_counterAutoForce = (autoForce != 0);
    }
    std::ostringstream msg;
    msg << "Force action configured: F6/clang queues set=" << g_forceSet
        << " id=" << g_forceId << ", counter no-hit injection "
        << g_forceInvulnSec << "s, auto-force="
        << (g_counterAutoForce ? "on" : "off (Ctrl inside window)");
    Log(msg.str());
}

// --- RXN watcher: reaction manager + configurable FSM property polling -------
// mgr = *(player+0x7D20); mgr+0x7C0 arms at move start (NOT a clang detector).
// Property chain (decompilation-verified): component = *(player+0x126D8);
// key = *(int32*)(component+0x84+id*24); registry = base+0x5012048 (count
// +0xB8, rows *(+0xC8)[key]); counters = *(int16*)(row+0x24+index*2).
// Watched ids come from nativePC\plugins\ValorGauge.prop-watch.txt (default
// "222 631"): 222 = foresight block (idx1 family flag / idx2 success counter),
// 631 = 見切り回避成功後の攻撃ヒット (success + follow-up hit latch).
std::uint64_t g_rxnFrame = 0;
std::uint32_t g_rxnStandby = 0;
std::uint8_t g_rxnGate = 0;
float g_rxnNohit = 0.0f;
bool g_rxnValid = false;
std::uint32_t g_rxnRowMissLog = 0;
std::uint8_t* g_rxnDiagPlayer = nullptr;
std::uint64_t g_rxnDiagNextFrame = 0;
constexpr int kMaxWatchIds = 6;
std::uint32_t g_watchIds[kMaxWatchIds] = {222, 631};
int g_watchCount = 2;
std::int16_t g_watchPrev[kMaxWatchIds][3] = {};
std::int16_t g_watchCur[kMaxWatchIds][3] = {};
std::uint32_t g_watchKeys[kMaxWatchIds] = {};
std::int16_t* g_watchCounters[kMaxWatchIds] = {};
// Prop trigger: when this watched id/index INCREASES during 见切, open the
// counter window (the vanilla success counter 222 idx2 = a hit was negated).
// Config nativePC\plugins\ValorGauge.prop-trigger.txt: "<id> <index>",
// id 0 disables.
std::uint32_t g_triggerId = 222;
std::uint32_t g_triggerIndex = 2;

void OpenCounterWindow(const char* reason) {
    g_counterWindowUntil = std::chrono::steady_clock::now() +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(g_counterWindowSec));
    auto pool = g_lastPlayerPool.load();
    if (pool != 0 && g_forceInvulnSec > 0.0f) {
        auto* window = reinterpret_cast<float*>(pool + 0xE4);
        if (IsWritable(window, sizeof(float))) {
            *window = *window > g_forceInvulnSec ? *window : g_forceInvulnSec;
        }
    }
    std::ostringstream m;
    m << "COUNTER WINDOW OPEN (" << reason << ") - press the spirit key to counter";
    Log(m.str());
}

std::int16_t* ResolvePropCounters(std::uint8_t* player, std::uint32_t id,
                                  std::uint32_t* keyOut) {
    if (!player || g_moduleBase.load() == 0) {
        return nullptr;
    }
    auto* componentSlot = player + 0x126D8;
    if (!IsReadable(componentSlot, sizeof(void*))) {
        return nullptr;
    }
    auto* component = *reinterpret_cast<std::uint8_t**>(componentSlot);
    if (!component) {
        return nullptr;
    }
    auto* keyAddress = component + 0x84 + static_cast<std::size_t>(id) * 24;
    if (!IsReadable(keyAddress, sizeof(std::int32_t))) {
        return nullptr;
    }
    const auto key = *reinterpret_cast<const std::int32_t*>(keyAddress);
    if (keyOut) {
        *keyOut = static_cast<std::uint32_t>(key);
    }
    auto* registry = reinterpret_cast<std::uint8_t*>(g_moduleBase.load() + 0x5012048);
    if (!IsReadable(registry, 0xD0)) {
        return nullptr;
    }
    const auto count = *reinterpret_cast<const std::int32_t*>(registry + 0xB8);
    if (key < 0 || key >= count) {
        return nullptr;
    }
    auto** rowTable = *reinterpret_cast<std::uint8_t***>(registry + 0xC8);
    if (!IsReadable(rowTable, (static_cast<std::size_t>(key) + 1) * sizeof(void*))) {
        return nullptr;
    }
    auto* row = rowTable[key];
    if (!row || !IsReadable(row, 0x30)) {
        return nullptr;
    }
    return reinterpret_cast<std::int16_t*>(row + 0x24);
}

// One line describing every link of the manager/property chain. Fired when a
// watched row stays unresolved (including key==0 = property unbound).
void LogChainDiagnostics(std::uint8_t* player, std::uint32_t id) {
    std::ostringstream m;
    m << std::hex << std::uppercase << "RXN DIAG id=" << std::dec << id << " player=0x"
      << reinterpret_cast<std::uintptr_t>(player);
    auto* mgrSlot = player + 0x7D20;
    if (!IsReadable(mgrSlot, sizeof(void*))) {
        m << " mgrSlot=unreadable";
    } else {
        auto* mgr = *reinterpret_cast<std::uint8_t**>(mgrSlot);
        m << " mgr=0x" << reinterpret_cast<std::uintptr_t>(mgr);
        if (mgr && IsReadable(mgr, 0x10)) {
            m << " backlink="
              << (*reinterpret_cast<void* const*>(mgr + 8) == player ? "ok" : "MISMATCH");
            m << " 7C0raw=0x" << *reinterpret_cast<const std::uint32_t*>(mgr + 0x7C0);
        }
    }
    auto* compSlot = player + 0x126D8;
    if (!IsReadable(compSlot, sizeof(void*))) {
        m << " compSlot=unreadable";
        Log(m.str());
        return;
    }
    auto* comp = *reinterpret_cast<std::uint8_t**>(compSlot);
    m << " comp=0x" << reinterpret_cast<std::uintptr_t>(comp);
    if (!comp) {
        Log(m.str());
        return;
    }
    m << " keys[";
    for (int wi = 0; wi < g_watchCount; ++wi) {
        auto* keyAddress = comp + 0x84 + static_cast<std::size_t>(g_watchIds[wi]) * 24;
        if (!IsReadable(keyAddress, sizeof(std::int32_t))) {
            m << " ?";
            continue;
        }
        m << " " << g_watchIds[wi] << "="
          << std::dec << *reinterpret_cast<const std::int32_t*>(keyAddress) << std::hex;
    }
    m << " ]";
    auto* registry = reinterpret_cast<std::uint8_t*>(g_moduleBase.load() + 0x5012048);
    if (!IsReadable(registry, 0xD0)) {
        m << " registry=unreadable base=0x" << std::hex << std::uppercase
          << g_moduleBase.load();
        Log(m.str());
        return;
    }
    const auto regCount = *reinterpret_cast<const std::int32_t*>(registry + 0xB8);
    auto* rowTable = *reinterpret_cast<std::uint8_t***>(registry + 0xC8);
    m << std::hex << std::uppercase << " reg=0x" << reinterpret_cast<std::uintptr_t>(registry)
      << " rawB8=0x" << *reinterpret_cast<const std::uint32_t*>(registry + 0xB8)
      << " rawBC=0x" << *reinterpret_cast<const std::uint32_t*>(registry + 0xBC)
      << " rowTable=0x" << reinterpret_cast<std::uintptr_t>(rowTable);
    for (int wi = 0; wi < g_watchCount; ++wi) {
        const auto key = *reinterpret_cast<const std::int32_t*>(
            comp + 0x84 + static_cast<std::size_t>(g_watchIds[wi]) * 24);
        m << std::dec << " id" << g_watchIds[wi] << "(key=" << key << "):";
        if (key < 0 || key >= regCount || !IsReadable(rowTable, (key + 1) * 8)) {
            m << "oob";
            continue;
        }
        auto* row = rowTable[key];
        m << std::hex << std::uppercase << "row=0x" << reinterpret_cast<std::uintptr_t>(row);
        if (row && IsReadable(row, 0x30)) {
            m << " vals=[" << std::dec << *reinterpret_cast<const std::int16_t*>(row + 0x24)
              << ',' << *reinterpret_cast<const std::int16_t*>(row + 0x26)
              << ',' << *reinterpret_cast<const std::int16_t*>(row + 0x28) << ']';
        } else {
            m << "unreadable";
        }
    }
    Log(m.str());
}

void PollReactionWatch() {
    ++g_rxnFrame;
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;

    auto* mgrSlot = player + 0x7D20;
    std::uint8_t* mgr = nullptr;
    if (IsReadable(mgrSlot, sizeof(void*))) {
        mgr = *reinterpret_cast<std::uint8_t**>(mgrSlot);
    }
    std::uint32_t standby = 0;
    float nohit = 0.0f;
    if (mgr && IsReadable(mgr, 0x7D0)) {
        standby = *reinterpret_cast<const std::uint32_t*>(mgr + 0x7C0);
    }
    if (mgr && IsReadable(mgr + 0x28 + 0xE4, sizeof(float))) {
        nohit = *reinterpret_cast<const float*>(mgr + 0x28 + 0xE4);
    }
    std::uint8_t gate7626 = 0;
    if (IsReadable(player + 0x7626, 1)) {
        gate7626 = player[0x7626];
    }

    // Poll every watched property id.
    for (int wi = 0; wi < g_watchCount; ++wi) {
        auto* counters = ResolvePropCounters(player, g_watchIds[wi], &g_watchKeys[wi]);
        if (g_watchKeys[wi] == 0) {
            counters = nullptr; // key==0: property unbound (no FSM reference)
        }
        g_watchCounters[wi] = counters;
        for (int i = 0; i < 3; ++i) {
            g_watchCur[wi][i] = counters ? counters[i] : 0;
        }
    }

    // 刚1 trigger: the mined clang latch at player+0xB228. Rising edge during
    // a special evasion (见切 86 / iai 101/102) = a hit was just negated.
    if (IsReadable(player + 0xB228, sizeof(std::uint32_t))) {
        const auto latch = *reinterpret_cast<const std::uint32_t*>(player + 0xB228);
        if (latch != 0 && g_clangLatchPrev == 0) {
            bool validEvasion = false;
            {
                std::scoped_lock lock(g_actionMutex);
                validEvasion = g_lastLongSwordAction &&
                               g_lastLongSwordAction->actionSet == 3 &&
                               (g_lastLongSwordAction->actionId == 86 ||
                                g_lastLongSwordAction->actionId == 101 ||
                                g_lastLongSwordAction->actionId == 102);
            }
            if (validEvasion) {
                g_clangLatch = true;
                OpenCounterWindow("clang latch rising edge (player+0xB228)");
                if (g_counterAutoForce && !g_forceArmed.exchange(true)) {
                    Log("CLANG latch set - auto-force armed");
                }
            }
        }
        g_clangLatchPrev = latch;
    }

    // Prop trigger: a successful negation increments the success counter.
    if (g_rxnValid && g_triggerId != 0) {
        for (int wi = 0; wi < g_watchCount; ++wi) {
            if (g_watchIds[wi] != g_triggerId) continue;
            const auto nowV = g_watchCur[wi][g_triggerIndex < 3 ? g_triggerIndex : 0];
            const auto prevV = g_watchPrev[wi][g_triggerIndex < 3 ? g_triggerIndex : 0];
            if (nowV > prevV && ActionIsForesightSlash()) {
                OpenCounterWindow("success counter edge (prop trigger)");
            }
        }
    }

    if (!g_rxnValid) {
        g_rxnStandby = standby;
        g_rxnNohit = nohit;
        g_rxnGate = gate7626;
        for (int wi = 0; wi < g_watchCount; ++wi) {
            for (int i = 0; i < 3; ++i) {
                g_watchPrev[wi][i] = g_watchCur[wi][i];
            }
        }
        g_rxnValid = true;
        for (int wi = 0; wi < g_watchCount; ++wi) {
            if (g_watchCounters[wi]) {
                std::ostringstream m;
                m << "RXN: prop " << g_watchIds[wi] << " resolved key="
                  << g_watchKeys[wi] << " counters=0x" << std::hex << std::uppercase
                  << reinterpret_cast<std::uintptr_t>(g_watchCounters[wi]) << std::dec
                  << " initial=[" << g_watchCur[wi][0] << ',' << g_watchCur[wi][1]
                  << ',' << g_watchCur[wi][2] << ']';
                Log(m.str());
            } else if (wi == 0 && g_rxnRowMissLog < 3) {
                ++g_rxnRowMissLog;
                LogChainDiagnostics(player, g_watchIds[wi]);
            }
        }
        return;
    }

    const bool windowEdge = (nohit == 0.0f) != (g_rxnNohit == 0.0f);
    bool propsChanged = false;
    for (int wi = 0; wi < g_watchCount && !propsChanged; ++wi) {
        for (int i = 0; i < 3; ++i) {
            propsChanged = propsChanged || g_watchCur[wi][i] != g_watchPrev[wi][i];
        }
    }
    const bool changed = standby != g_rxnStandby || windowEdge ||
                         gate7626 != g_rxnGate || propsChanged;
    if (!changed) {
        g_rxnNohit = nohit;
        return;
    }

    ActionInfo action{};
    bool haveAction = false;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction) {
            action = *g_lastLongSwordAction;
            haveAction = true;
        }
    }
    std::ostringstream line;
    line << "RXN f=" << std::dec << g_rxnFrame << " act=";
    if (haveAction) {
        line << action.actionSet << ':' << action.actionId;
    } else {
        line << "--";
    }
    line << " 7C0=" << standby << " gate=" << static_cast<int>(gate7626) << std::fixed
         << std::setprecision(2) << " nohit=" << nohit;
    for (int wi = 0; wi < g_watchCount; ++wi) {
        line << " p" << g_watchIds[wi] << "=[" << g_watchCur[wi][0] << ','
             << g_watchCur[wi][1] << ',' << g_watchCur[wi][2] << ']';
    }
    Log(line.str());

    g_rxnStandby = standby;
    g_rxnNohit = nohit;
    g_rxnGate = gate7626;
    for (int wi = 0; wi < g_watchCount; ++wi) {
        for (int i = 0; i < 3; ++i) {
            g_watchPrev[wi][i] = g_watchCur[wi][i];
        }
    }
}

// Optional nativePC\plugins\ValorGauge.prop-watch.txt: property ids to watch,
// up to 6, e.g. "222 631".
void LoadPropWatchConfig() {
    std::ifstream trigger("nativePC\plugins\ValorGauge.prop-trigger.txt");
    std::int32_t tid = 0;
    std::int32_t tidx = 0;
    if (trigger >> tid >> tidx && tid >= 0 && tidx >= 0) {
        g_triggerId = static_cast<std::uint32_t>(tid);
        g_triggerIndex = static_cast<std::uint32_t>(tidx);
    }
    std::ostringstream msg;
    msg << "Prop trigger configured: id=" << g_triggerId << " idx=" << g_triggerIndex
        << (g_triggerId == 0 ? " (disabled)" : "");
    Log(msg.str());
    std::ifstream input("nativePC\\plugins\\ValorGauge.prop-watch.txt");
    std::int32_t id = 0;
    int count = 0;
    while (count < kMaxWatchIds && (input >> id) && id >= 0) {
        g_watchIds[count++] = static_cast<std::uint32_t>(id);
    }
    if (count > 0) {
        g_watchCount = count;
    }
    std::ostringstream watchMsg;
    watchMsg << "Property watch list:";
    for (int wi = 0; wi < g_watchCount; ++wi) {
        watchMsg << ' ' << g_watchIds[wi];
    }
    Log(watchMsg.str());
}

// --- F6 property write test (Shift+F6) ---------------------------------------
// Writes the 0xDE-family counter row through the verified chain. Requires the
// property to be bound (key != 0), i.e. some FSM node must reference it.
std::uint32_t g_propWriteId = 222;
std::uint32_t g_propWriteIndex = 0;
std::int32_t g_propWriteValue = 1;
bool g_f6WasDown = false;

// Optional nativePC\plugins\ValorGauge.prop-write.txt: "<id> <index> <value>".
void LoadPropWriteConfig() {
    std::ifstream input("nativePC\\plugins\\ValorGauge.prop-write.txt");
    std::int32_t id = 0;
    std::int32_t index = 0;
    std::int32_t value = 0;
    if (input >> id >> index >> value && id >= 0 && index >= 0) {
        g_propWriteId = static_cast<std::uint32_t>(id);
        g_propWriteIndex = static_cast<std::uint32_t>(index);
        g_propWriteValue = value;
    }
    std::ostringstream msg;
    msg << "Property write test configured: Shift+F6 writes id=" << g_propWriteId
        << " idx=" << g_propWriteIndex << " value=" << g_propWriteValue;
    Log(msg.str());
}

void TryPropertyWrite() {
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
        Log("PROPWRITE: player controller not locked yet");
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    std::uint32_t key = 0;
    auto* counters = ResolvePropCounters(player, g_propWriteId, &key);
    if (counters && key == 0) {
        Log("PROPWRITE: property key slot is 0 - unbound (no FSM node references it); "
            "add an R/property condition via XFsm and reload the area first");
        return;
    }
    if (!counters) {
        Log("PROPWRITE: property row unresolved");
        return;
    }
    auto* target = counters + g_propWriteIndex;
    DWORD oldProtect = 0;
    const bool directlyWritable = IsWritable(target, sizeof(std::int16_t));
    if (!directlyWritable) {
        MEMORY_BASIC_INFORMATION mbi{};
        VirtualQuery(target, &mbi, sizeof(mbi));
        std::ostringstream m;
        m << std::hex << std::uppercase << "PROPWRITE: target=0x"
          << reinterpret_cast<std::uintptr_t>(target) << " protect=0x" << mbi.Protect
          << " state=0x" << mbi.State << std::dec << " - trying VirtualProtect";
        Log(m.str());
        if (!VirtualProtect(target, sizeof(std::int16_t), PAGE_READWRITE, &oldProtect)) {
            Log("PROPWRITE: VirtualProtect failed; write aborted");
            return;
        }
    }

    std::ostringstream before;
    before << "PROPWRITE id=" << std::dec << g_propWriteId << " idx=" << g_propWriteIndex
           << " key=" << key << std::hex << std::uppercase << " counters=0x"
           << reinterpret_cast<std::uintptr_t>(counters) << std::dec
           << " before=[" << counters[0] << ',' << counters[1] << ',' << counters[2] << ']';
    Log(before.str());

    counters[g_propWriteIndex] = static_cast<std::int16_t>(g_propWriteValue);

    if (!directlyWritable) {
        VirtualProtect(target, sizeof(std::int16_t), oldProtect, &oldProtect);
    }

    std::ostringstream after;
    after << "PROPWRITE wrote " << g_propWriteValue << (directlyWritable ? "" : " (protected)")
          << "; readback=[" << counters[0] << ',' << counters[1] << ',' << counters[2] << ']';
    Log(after.str());
}

// --- F9 clang miner: paired-scenario state mining -----------------------------
// F5 #1: start mining (phase 1). Each 见切 (3:86 entry) records one run of
// per-frame memory deltas over weapon/mgr/pool/FSM regions plus HP.
// F5 #2: advance to phase 2 (do the other scenario, e.g. real clangs).
// F5 #3: stop and dump the analysis: offsets whose change pattern is
// exclusive to one phase = candidate clang-state signals, no guessing.
struct MinerChange { std::uint32_t key; std::uint32_t val; std::uint32_t frame; };
struct MinerRun {
    int phase = 0;
    float hpStart = 0.0f;
    float hpEnd = 0.0f;
    std::uint32_t frames = 0;
    std::vector<MinerChange> changes;
};
struct MinerRegionSpec {
    std::uint8_t* base = nullptr;
    std::size_t size = 0;
    const char* name = "";
    std::vector<std::uint8_t> prev;
};
std::vector<MinerRegionSpec> g_minerRegions;
std::vector<MinerRun> g_minerRuns;
MinerRun g_minerCur;
bool g_minerInRun = false;
int g_minerPhase = 0;
bool g_minerActive = false;
bool g_f9WasDown = false;
std::uint32_t g_minerFrameInRun = 0;
constexpr std::size_t kMinerMaxChanges = 30000;

float MinerReadHealth(std::uint8_t* player) {
    auto* hpBox = player + 0x7630; // {player+0x7630} + 0x64 (Lua engine layout)
    if (!IsReadable(hpBox, sizeof(void*))) {
        return -1.0f;
    }
    auto* box = *reinterpret_cast<std::uint8_t**>(hpBox);
    if (!box || !IsReadable(box + 0x64, sizeof(float))) {
        return -1.0f;
    }
    return *reinterpret_cast<const float*>(box + 0x64);
}

void MinerSetupRegions(std::uint8_t* player) {
    g_minerRegions.clear();
    auto* weaponSlot = player + 0x76B0;
    if (IsReadable(weaponSlot, sizeof(void*)) && *reinterpret_cast<void**>(weaponSlot)) {
        auto* weapon = *reinterpret_cast<std::uint8_t**>(weaponSlot);
        if (IsReadable(weapon, 0x2400)) {
            g_minerRegions.push_back({weapon, 0x2400, "weapon", {}});
        }
    }
    auto* mgrSlot = player + 0x7D20;
    if (IsReadable(mgrSlot, sizeof(void*)) && *reinterpret_cast<void**>(mgrSlot)) {
        auto* mgr = *reinterpret_cast<std::uint8_t**>(mgrSlot);
        if (IsReadable(mgr, 0x800)) {
            g_minerRegions.push_back({mgr, 0x800, "mgr", {}});
            auto* pool = mgr + 0x28;
            if (IsReadable(pool, 0x180)) {
                g_minerRegions.push_back({pool, 0x180, "pool", {}});
            }
        }
    }
    // FSM fields on the player struct (controller + fsmTarget/id block).
    if (IsReadable(player + 0x6100, 0x900)) {
        g_minerRegions.push_back({player + 0x6100, 0x900, "playerFsm", {}});
    }
    // Player combat windows: event bit array (+0x8938!), flags, broad sweep.
    struct PlayerWindow { std::size_t off; std::size_t size; const char* name; };
    const PlayerWindow windows[] = {
        {0x1000, 0x400, "pFlags"},
        {0x1200, 0x7700, "pCore"},
        {0x8900, 0x180, "pEventBits"},
        {0x8A80, 0x2A80, "pTail"},
    };
    for (const auto& w : windows) {
        if (IsReadable(player + w.off, w.size)) {
            g_minerRegions.push_back({player + w.off, w.size, w.name, {}});
        }
    }
    for (auto& r : g_minerRegions) {
        r.prev.assign(r.base, r.base + r.size);
    }
}

void MinerBeginRun(std::uint8_t* player) {
    MinerSetupRegions(player);
    g_minerCur = MinerRun{};
    g_minerCur.phase = g_minerPhase;
    g_minerCur.hpStart = MinerReadHealth(player);
    g_minerFrameInRun = 0;
    g_minerInRun = true;
}

void MinerEndRun() {
    g_minerInRun = false;
    g_minerRuns.push_back(std::move(g_minerCur));
    g_minerCur = MinerRun{};
    std::ostringstream m;
    m << "MINER run " << g_minerRuns.size() << " (phase " << g_minerRuns.back().phase
      << "): " << g_minerRuns.back().frames << " frames, "
      << g_minerRuns.back().changes.size() << " changes, HP "
      << g_minerRuns.back().hpStart << " -> " << g_minerRuns.back().hpEnd;
    Log(m.str());
}

void MinerCaptureFrame() {
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
        return;
    }
    if (!g_minerActive) {
        g_minerActive = true;
        g_minerPhase = 1;
        Log("MINER auto-started: recording every 见切 as phase 1. "
            "F5 marks a new phase + dumps a snapshot.");
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;

    bool is86 = false;
    {
        std::scoped_lock lock(g_actionMutex);
        is86 = g_lastLongSwordAction && g_lastLongSwordAction->actionSet == 3 &&
               g_lastLongSwordAction->actionId == 86;
    }
    if (is86 && !g_minerInRun) {
        MinerBeginRun(player);
    }
    if (!is86 && g_minerInRun) {
        MinerEndRun();
        while (g_minerRuns.size() > 16) {
            g_minerRuns.erase(g_minerRuns.begin());
        }
        return;
    }
    if (!g_minerInRun) {
        return;
    }
    ++g_minerFrameInRun;
    g_minerCur.frames = g_minerFrameInRun;
    g_minerCur.hpEnd = MinerReadHealth(player);

    for (std::size_t ri = 0; ri < g_minerRegions.size(); ++ri) {
        auto& r = g_minerRegions[ri];
        if (!IsReadable(r.base, r.size)) {
            continue;
        }
        const auto* cur = reinterpret_cast<const std::uint32_t*>(r.base);
        auto* prev = reinterpret_cast<std::uint32_t*>(r.prev.data());
        const auto dwordCount = r.size / sizeof(std::uint32_t);
        const auto regionTag = static_cast<std::uint32_t>(ri << 24);
        for (std::size_t d = 0; d < dwordCount; ++d) {
            if (cur[d] != prev[d]) {
                prev[d] = cur[d];
                if (g_minerCur.changes.size() < kMinerMaxChanges) {
                    const auto offset = static_cast<std::uint32_t>(d * 4);
                    g_minerCur.changes.push_back(
                        {regionTag | offset, cur[d], g_minerFrameInRun});
                }
            }
        }
    }
}

void MinerDumpAnalysis() {
    std::ostringstream header;
    header << "MINER ANALYSIS: " << g_minerRuns.size() << " runs";
    Log(header.str());
    for (std::size_t i = 0; i < g_minerRuns.size(); ++i) {
        std::ostringstream m;
        m << "MINER run " << (i + 1) << " phase=" << g_minerRuns[i].phase
          << " frames=" << g_minerRuns[i].frames << " changes="
          << g_minerRuns[i].changes.size() << std::fixed << std::setprecision(1)
          << " HP " << g_minerRuns[i].hpStart << "->" << g_minerRuns[i].hpEnd;
        Log(m.str());
    }
    if (g_minerRuns.empty()) {
        return;
    }
    // key -> per-phase stats {runsChanged, minFirstFrame}
    struct KeyStat { int changed[3] = {}; int minFrame[3] = {}; };
    std::map<std::uint32_t, KeyStat> stats;
    for (const auto& run : g_minerRuns) {
        if (run.phase < 1 || run.phase > 2) {
            continue;
        }
        std::map<std::uint32_t, std::uint32_t> firstFrame;
        for (const auto& c : run.changes) {
            if (!firstFrame.count(c.key) || c.frame < firstFrame[c.key]) {
                firstFrame[c.key] = c.frame;
            }
        }
        for (const auto& [key, frame] : firstFrame) {
            auto& st = stats[key];
            ++st.changed[run.phase];
            if (st.minFrame[run.phase] == 0 || frame < st.minFrame[run.phase]) {
                st.minFrame[run.phase] = frame;
            }
        }
    }
    // Correlation-ranked report: keys whose change rate across ALL phase-2
    // runs is higher than across ALL phase-1 runs, strongest first.
    struct KeyInfo {
        int changed[3] = {};
        int minFrame[3] = {};
    };
    // Phase 1 = whiffs, every LATER phase = clangs (users often do mixed
    // runs in phase 2 and clean clangs in 3+; all of them are clang data).
    int phaseTotal[3] = {};
    for (const auto& run : g_minerRuns) {
        if (run.phase >= 1) {
            ++phaseTotal[run.phase == 1 ? 1 : 2];
        }
    }
    std::map<std::uint32_t, KeyInfo> kinfo;
    for (const auto& run : g_minerRuns) {
        if (run.phase < 1) {
            continue;
        }
        const int slot = run.phase == 1 ? 1 : 2;
        std::map<std::uint32_t, std::uint32_t> firstFrame;
        for (const auto& c : run.changes) {
            if (!firstFrame.count(c.key) || c.frame < firstFrame[c.key]) {
                firstFrame[c.key] = c.frame;
            }
        }
        for (const auto& [key, frame] : firstFrame) {
            auto& ki = kinfo[key];
            ++ki.changed[slot];
            if (ki.minFrame[slot] == 0 || frame < ki.minFrame[slot]) {
                ki.minFrame[slot] = frame;
            }
        }
    }
    std::vector<std::pair<std::uint32_t, KeyInfo>> ranked(kinfo.begin(), kinfo.end());
    const double wTotal = phaseTotal[1] > 0 ? phaseTotal[1] : 1;
    const double cTotal = phaseTotal[2] > 0 ? phaseTotal[2] : 1;
    std::sort(ranked.begin(), ranked.end(),
              [&](const auto& a, const auto& b) {
                  const double sa = a.second.changed[2] / cTotal;
                  const double sb = b.second.changed[2] / cTotal;
                  const double wa = a.second.changed[1] / wTotal;
                  const double wb = b.second.changed[1] / wTotal;
                  return (sa - wa) > (sb - wb);
              });
    const char* regionNames[8] = {"weapon", "mgr", "pool", "playerFsm",
                                  "pFlags", "pCore", "pEventBits", "pTail"};
    int reported = 0;
    for (const auto& [key, ki] : ranked) {
        if (reported >= 60) {
            break;
        }
        const int region = static_cast<int>((key >> 24) & 7);
        const auto offset = key & 0x00FFFFFF;
        if (reported == 0) {
            std::ostringstream hdr;
            hdr << "MINER CANDIDATES (whiff runs=" << std::dec << phaseTotal[1]
                << ", clang runs=" << phaseTotal[2] << "):";
            Log(hdr.str());
        }
        ++reported;
        std::ostringstream m;
        m << "MINER CANDIDATE " << regionNames[region] << "+0x" << std::hex
          << std::uppercase << offset << std::dec << " whiff " << ki.changed[1] << '/'
          << phaseTotal[1] << " clang " << ki.changed[2] << '/' << phaseTotal[2]
          << " firstFrame w=" << ki.minFrame[1] << " c=" << ki.minFrame[2];
        Log(m.str());
    }
    Log("MINER ANALYSIS END");
}

void MinerHandleF9() {
    // Recording is ALWAYS on once the controller is locked; F5 tags upcoming
    // runs with the next phase and dumps a ranked snapshot of everything so
    // far. Nothing to start, nothing to stop.
    ++g_minerPhase;
    MinerDumpAnalysis();
    std::ostringstream m;
    m << "MINER: now tagging runs as phase " << g_minerPhase
      << " (previous phases kept in the report)";
    Log(m.str());
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

// --- F10 struct diff capture ----------------------------------------------
// Two-phase capture for locating defense-timer fields empirically:
//   baseline (5 s idle) learns which offsets move without input;
//   armed (F10 again / 30 s) logs high-signal transitions on non-noisy offsets:
//   Z = dword crossed zero (timer armed/expired), B = single-bit flag flip.
enum class DiffPhase { Idle, Baseline, Armed };

struct DiffChangeStats {
    std::uint32_t changes = 0;
    std::uint32_t zeroCrosses = 0;
    std::uint32_t bitToggles = 0;
    std::uint32_t lastOld = 0;
    std::uint32_t lastNew = 0;
};

struct DiffRegion {
    const char* name = "";
    std::uint8_t* base = nullptr;
    std::size_t size = 0;
    std::vector<std::uint8_t> prev;
    bool initialized = false;
    std::unordered_map<std::uint32_t, DiffChangeStats> baseline;
    std::unordered_set<std::uint32_t> noisy;
    std::unordered_map<std::uint32_t, DiffChangeStats> stats;
};

DiffPhase g_diffPhase = DiffPhase::Idle;
DiffRegion g_diffWeapon;
DiffRegion g_diffPlayer;
DiffRegion g_diffPool;
std::chrono::steady_clock::time_point g_diffDeadline{};
std::uint32_t g_diffFrame = 0;
std::uint32_t g_diffLineBudget = 0;
bool g_f10WasDown = false;

std::string FormatHex32(std::uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase << value;
    return out.str();
}

std::string FormatFloat32(std::uint32_t raw) {
    float value = 0.0f;
    std::memcpy(&value, &raw, sizeof(value));
    std::ostringstream out;
    out << value;
    return out.str();
}

void DiffScanRegion(DiffRegion& region, bool armed) {
    if (!region.base || region.size == 0 || !IsReadable(region.base, region.size)) {
        return;
    }
    if (!region.initialized) {
        region.prev.assign(region.base, region.base + region.size);
        region.initialized = true;
        return;
    }

    std::optional<ActionInfo> action;
    {
        std::scoped_lock lock(g_actionMutex);
        action = g_lastLongSwordAction;
    }

    auto* previous = reinterpret_cast<std::uint32_t*>(region.prev.data());
    const auto* current = reinterpret_cast<const std::uint32_t*>(region.base);
    const auto dwordCount = static_cast<std::uint32_t>(region.size / sizeof(std::uint32_t));
    auto& active = armed ? region.stats : region.baseline;
    for (std::uint32_t i = 0; i < dwordCount; ++i) {
        const auto oldValue = previous[i];
        const auto newValue = current[i];
        if (oldValue == newValue) {
            continue;
        }
        previous[i] = newValue;
        const auto offset = static_cast<std::uint32_t>(i * sizeof(std::uint32_t));
        auto& entry = active[offset];
        entry.changes += 1;
        entry.lastOld = oldValue;
        entry.lastNew = newValue;
        const bool zeroCross = oldValue == 0 || newValue == 0;
        const bool bitToggle = std::popcount(oldValue ^ newValue) == 1;
        if (zeroCross) {
            entry.zeroCrosses += 1;
        }
        if (bitToggle) {
            entry.bitToggles += 1;
        }
        if (armed && (zeroCross || bitToggle) && g_diffLineBudget > 0) {
            --g_diffLineBudget;
            std::ostringstream line;
            line << "DIFF[" << region.name << "] +0x" << std::hex << std::uppercase << offset
                 << (region.noisy.find(offset) != region.noisy.end() ? " n: "
                        : zeroCross ? " Z: " : " B: ")
                 << FormatHex32(oldValue) << "(" << FormatFloat32(oldValue) << ") -> "
                 << FormatHex32(newValue) << "(" << FormatFloat32(newValue) << ") @f"
                 << std::dec << g_diffFrame << " act=";
            if (action) {
                line << action->actionSet << ':' << action->actionId;
            } else {
                line << "--";
            }
            Log(line.str());
        }
    }
}

// The damage layer consults a per-player defense-pool object accessed as
// ([player+0x7D20] + 0x28) (see FUN_140aee690 call sites); its +8 holds the
// player pointer, timers at +0x84..0xC4, selector bools at +0x13D..0x177.
std::uint8_t* FindDefensePool(std::uint8_t* player) {
    auto* slot = player + 0x7D20;
    if (!IsReadable(slot, sizeof(void*))) {
        return nullptr;
    }
    auto* owner = *reinterpret_cast<std::uint8_t**>(slot);
    if (!owner || !IsReadable(owner, 0x10)) {
        return nullptr;
    }
    if (*reinterpret_cast<void* const*>(owner + 8) != player) {
        Log("DIFF: pool owner backlink mismatch; pool watch disabled");
        return nullptr;
    }
    auto* pool = owner + 0x28;
    if (!IsReadable(pool, 0x180)) {
        return nullptr;
    }
    return pool;
}

void StartDiffCapture() {
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
        Log("DIFF: player controller not locked yet; perform a Long Sword action first");
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    auto* weaponSlot = player + 0x76B0;
    if (!IsReadable(weaponSlot, sizeof(void*))) {
        Log("DIFF: weapon slot unreadable");
        return;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(weaponSlot);
    if (!weapon || !IsReadable(weapon, 0x2400)) {
        Log("DIFF: weapon struct unreadable");
        return;
    }

    g_diffWeapon = DiffRegion{};
    g_diffWeapon.name = "weapon";
    g_diffWeapon.base = weapon;
    g_diffWeapon.size = 0x2400;
    g_diffPlayer = DiffRegion{};
    g_diffPlayer.name = "player";
    g_diffPlayer.base = player;
    g_diffPlayer.size = 0x12608;
    if (!IsReadable(g_diffPlayer.base, g_diffPlayer.size)) {
        Log("DIFF: player struct tail unreadable; watching weapon only");
        g_diffPlayer.base = nullptr;
        g_diffPlayer.size = 0;
    }
    g_diffPool = DiffRegion{};
    g_diffPool.name = "pool";
    if (g_lastPlayerPool.load() != 0) {
        g_diffPool.base = reinterpret_cast<std::uint8_t*>(g_lastPlayerPool.load());
        g_diffPool.size = 0x180;
    } else if (g_diffPlayer.base) {
        auto* pool = FindDefensePool(player);
        if (pool) {
            g_diffPool.base = pool;
            g_diffPool.size = 0x180;
        }
    }
    if (!IsReadable(g_diffPool.base, g_diffPool.size)) {
        Log("DIFF: defense pool not located; watching weapon+player only");
        g_diffPool.base = nullptr;
        g_diffPool.size = 0;
    }

    g_diffPhase = DiffPhase::Baseline;
    g_diffDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    std::ostringstream message;
    message << "DIFF baseline started: weapon=0x" << std::hex << std::uppercase
            << reinterpret_cast<std::uintptr_t>(weapon) << " player=0x"
            << reinterpret_cast<std::uintptr_t>(player)
            << " pool=0x" << reinterpret_cast<std::uintptr_t>(g_diffPool.base)
            << " - stand still for 5 seconds";
    Log(message.str());
}

void PromoteDiffToArmed() {
    for (DiffRegion* region : {&g_diffWeapon, &g_diffPlayer, &g_diffPool}) {
        region->noisy.clear();
        for (const auto& [offset, entry] : region->baseline) {
            if (entry.changes > 0) {
                region->noisy.insert(offset);
            }
        }
        // Keep baseline stats: the report compares armed behavior against baseline behavior.
        region->stats.clear();
    }
    g_diffLineBudget = 20000;
    g_diffPhase = DiffPhase::Armed;
    g_diffDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    std::ostringstream message;
    message << "DIFF armed: perform the move now (F10 to finish, 30 s timeout)"
            << " noiseMask weapon=" << g_diffWeapon.noisy.size()
            << " player=" << g_diffPlayer.noisy.size();
    Log(message.str());
}

void FinishDiffCapture() {
    for (DiffRegion* region : {&g_diffWeapon, &g_diffPlayer, &g_diffPool}) {
        std::vector<std::pair<std::uint32_t, const DiffChangeStats*>> candidates;
        std::uint32_t totalChanges = 0;
        for (const auto& [offset, entry] : region->stats) {
            totalChanges += entry.changes;
            if (entry.zeroCrosses > 0 || entry.bitToggles > 0) {
                candidates.emplace_back(offset, &entry);
            }
        }
        std::sort(candidates.begin(), candidates.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

        std::ostringstream header;
        header << "DIFF REPORT [" << region->name << "] trackedOffsets=" << region->stats.size()
               << " totalChanges=" << totalChanges
               << " signalOffsets=" << candidates.size();
        if (g_diffLineBudget == 0) {
            header << " (line budget exhausted; live lines truncated)";
        }
        Log(header.str());

        constexpr std::uint32_t maxReportedOffsets = 400;
        std::uint32_t reported = 0;
        for (const auto& [offset, entry] : candidates) {
            if (reported >= maxReportedOffsets) {
                std::ostringstream trunc;
                trunc << "DIFF REPORT [" << region->name << "] ... "
                      << (candidates.size() - reported) << " more offsets truncated";
                Log(trunc.str());
                break;
            }
            ++reported;
            const bool noisy = region->noisy.find(offset) != region->noisy.end();
            const auto baselineIt = region->baseline.find(offset);
            const auto baselineChanges =
                baselineIt == region->baseline.end() ? 0u : baselineIt->second.changes;
            const auto baselineZeroCrosses =
                baselineIt == region->baseline.end() ? 0u : baselineIt->second.zeroCrosses;
            std::ostringstream line;
            line << "DIFF REPORT [" << region->name << "] +0x" << std::hex << std::uppercase
                 << offset << std::dec << " z=" << entry->zeroCrosses
                 << " b=" << entry->bitToggles << " ch=" << entry->changes
                 << " blCh=" << baselineChanges << " blZ=" << baselineZeroCrosses
                 << " last=" << FormatHex32(entry->lastOld) << "->" << FormatHex32(entry->lastNew)
                 << " (" << FormatFloat32(entry->lastOld) << "->" << FormatFloat32(entry->lastNew)
                 << ")" << (noisy ? " noisy" : "");
            Log(line.str());
        }
        region->stats.clear();
        region->baseline.clear();
        region->noisy.clear();
        region->initialized = false;
        region->prev.clear();
    }
    g_diffPhase = DiffPhase::Idle;
    Log("DIFF capture finished; press F10 for a new session");
}

bool DiffTargetsValid() {
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
        return false;
    }
    const auto* player = static_cast<const std::uint8_t*>(controller) - 0x61C8;
    const auto* weaponSlot = player + 0x76B0;
    if (!IsReadable(weaponSlot, sizeof(void*))) {
        return false;
    }
    const auto* weapon = *reinterpret_cast<std::uint8_t* const*>(weaponSlot);
    return weapon == g_diffWeapon.base && IsReadable(g_diffWeapon.base, g_diffWeapon.size);
}

void PollDiffCapture() {
    ++g_diffFrame;
    const bool f9Down = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
    const bool f9Pressed = f9Down && !g_f10WasDown;
    g_f10WasDown = f9Down;

    const auto now = std::chrono::steady_clock::now();
    switch (g_diffPhase) {
    case DiffPhase::Idle:
        if (f9Pressed) {
            StartDiffCapture();
        }
        return;
    case DiffPhase::Baseline:
        if (!DiffTargetsValid()) {
            Log("DIFF aborted: weapon pointer changed or became unreadable");
            g_diffPhase = DiffPhase::Idle;
            return;
        }
        DiffScanRegion(g_diffWeapon, false);
        DiffScanRegion(g_diffPlayer, false);
        DiffScanRegion(g_diffPool, false);
        if (f9Pressed || now >= g_diffDeadline) {
            PromoteDiffToArmed();
        }
        return;
    case DiffPhase::Armed:
        if (!DiffTargetsValid()) {
            Log("DIFF aborted: weapon pointer changed or became unreadable");
            g_diffPhase = DiffPhase::Idle;
            return;
        }
        DiffScanRegion(g_diffWeapon, true);
        DiffScanRegion(g_diffPlayer, true);
        DiffScanRegion(g_diffPool, true);
        if (f9Pressed || now >= g_diffDeadline) {
            FinishDiffCapture();
        }
        return;
    }
}

std::string DiffStatusText() {
    switch (g_diffPhase) {
    case DiffPhase::Idle:
        return "";
    case DiffPhase::Baseline:
        return "DIFF baseline: stand still";
    case DiffPhase::Armed:
        return "DIFF ARMED: do the move (F10 to finish)";
    }
    return "";
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
    PollDiffCapture();
    PollPropHistogram();
    PollReactionWatch();

    MinerCaptureFrame();
    const bool f5Down = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
    if (f5Down && !g_f9WasDown) {
        MinerHandleF9();
    }
    g_f9WasDown = f5Down;

    // Counter window: a spirit-key press while the window is open fires 刚1.
    const bool windowOpen = std::chrono::steady_clock::now() < g_counterWindowUntil;
    const bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    if (windowOpen && ctrlDown && !g_counterCtrlWasDown) {
        if (!g_forceArmed.exchange(true)) {
            Log("COUNTER INPUT: spirit press inside the counter window - force armed");
        }
    }
    g_counterCtrlWasDown = ctrlDown;

    const bool f6Down = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    if (f6Down && !g_f6WasDown) {
        const bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        if (shiftDown) {
            TryPropertyWrite();
        } else if (!g_forceArmed.exchange(true)) {
            std::ostringstream m;
            m << "FORCE armed: the next Long Sword transition executes 3:"
              << g_forceId;
            Log(m.str());
        }
    }
    g_f6WasDown = f6Down;

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
        const std::string diffStatus = DiffStatusText();
        if (!diffStatus.empty()) {
            draw->AddText(ImGui::GetFont(), 18.0f, ImVec2(0.0f, 26.0f),
                          IM_COL32(80, 255, 80, 255), diffStatus.c_str());
        }

        char rxnText[128]{};
        int off = snprintf(rxnText, sizeof(rxnText), "7C0=%u gate=%u nohit=%.2f",
                           g_rxnStandby, g_rxnGate, g_rxnNohit);
        for (int wi = 0; wi < g_watchCount && off > 0 && off < (int)sizeof(rxnText); ++wi) {
            off += snprintf(rxnText + off, sizeof(rxnText) - off, " p%u=[%d %d %d]",
                            g_watchIds[wi], static_cast<int>(g_watchCur[wi][0]),
                            static_cast<int>(g_watchCur[wi][1]),
                            static_cast<int>(g_watchCur[wi][2]));
        }
        draw->AddText(ImGui::GetFont(), 16.0f, ImVec2(0.0f, 48.0f),
                      g_rxnStandby != 0 ? IM_COL32(90, 220, 255, 255)
                                        : IM_COL32(170, 170, 170, 200),
                      rxnText);
        if (windowOpen) {
            draw->AddText(ImGui::GetFont(), 18.0f, ImVec2(0.0f, 70.0f),
                          IM_COL32(80, 220, 255, 255), "COUNTER WINDOW");
        }
        if (g_forceArmed.load()) {
            draw->AddText(ImGui::GetFont(), 18.0f, ImVec2(0.0f, 92.0f),
                          IM_COL32(255, 220, 80, 255), "FORCE ARMED");
        }
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
                InstallPoolQueryHook();
                InstallPropHistogramHooks();
                LoadPropWriteConfig();
                LoadPropWatchConfig();
                LoadForceActionConfig();
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
