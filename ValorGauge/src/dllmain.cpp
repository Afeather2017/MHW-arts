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
#include <cstdlib>
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
bool g_f6WasDown = false;
bool g_f9WasDown = false;
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
// Hit-based valor charging (mod build). >0: percent per CONNECTED hit
// (DmgCalcHook); 0: legacy charge-on-action-start; <0: charging disabled.
// Configured via nativePC/plugins/ValorGauge.hit-charge.txt.
std::atomic_int g_hitChargePercent{10};
std::atomic_int32_t g_lastObservedSpiritLevel = -1;
// Gauge-lock experiment (2026-09-24): per-frame overwrite of an LS gauge
// float. Round 1 (target 0x2374): game drains it 0.000556/frame but the
// visible bar did NOT follow -> 0x2374 is the level-duration timer. Round 2
// target: weapon+0x2368 (second HUD getter FUN_140616DD0 reads it x100; the
// per-move charge gains and the cost compare FUN_140B980C0 use it; the iai
// stance writes int 0/1/2 over the same dword = gauge-clear-on-stance).
// Config nativePC/plugins/ValorGauge.gauge-lock.txt = "[<target>] <value 0..1>"
//   target 1 = weapon+0x2374, 2 = weapon+0x2368; bare "<value>" = target 1.
// F5 toggles at runtime. BOTH fields are always watched: drift = the game
// wrote underneath, rate-limited logged as GAUGELOCK.
std::atomic<int> g_gaugeLockTarget{1};
std::atomic<float> g_gaugeLockValue{0.0f};
std::atomic_bool g_gaugeLockEnabled{false};
bool g_gaugeLockKeyWasDown = false;
// DrawGauge(Present)-thread only state. Index 0 = +0x2374, 1 = +0x2368.
float g_gaugeLockLastWritten[2] = {-1.0f, -1.0f};
float g_gaugeWatchDrift[2] = {0.0f, 0.0f};
std::uint32_t g_gaugeWatchFrames[2] = {0, 0};
std::chrono::steady_clock::time_point g_gaugeLockNextReport{};
// Gauge-event logger (GaugeAddHook): log every gauge delta with the caller
// RVA so one play session maps "which move moves the gauge by how much".
// Config nativePC/plugins/ValorGauge.gauge-log.txt = "<minAbsValue>"
// (default 0.01 filters the per-frame 0.000556 drains; 0 logs everything).
void (*g_originalGaugeAdd)(void* weapon, float value) = nullptr;
std::atomic<float> g_gaugeLogMinAbs{0.01f};
// Player-emitter notify logger (FUN_140ADD670): every body-effect id the game
// fires on the local player is logged with its caller RVA, so one session
// (iai, foresight, helm breaker...) maps which id = which aura.
void (*g_originalFxNotify)(void* emitter, std::uint32_t id, std::uint32_t arg) =
    nullptr;
// FX test (2026-09-24): fire a player-emitter notify directly to identify the
// spirit-slash body glow. Candidates seen in the per-move handlers:
//   0x7D2 / 0x7DC via func_140AE0D70 (gated: action 87/102 && level!=0, or +0x2398)
//   0x7D7         via FUN_140ADD670   (gated: +0x2398 stance flag)
// Config nativePC/plugins/ValorGauge.fx-test.txt = "<func> <id>" (func 0 =
// ADD670, 1 = AE0D70; id accepts 0x prefix). F7 fires once, Shift+F7 reloads.
void (__fastcall* g_fxNotifyA)(void*, std::uint32_t, std::uint32_t) = nullptr;
void (__fastcall* g_fxNotifyB)(void*, std::uint32_t, std::uint32_t) = nullptr;
std::atomic_int g_fxFireFunc{-1};
std::atomic<std::uint32_t> g_fxFireId{0};
int g_fxCfgFunc = 0;
std::uint32_t g_fxCfgId = 0x7D2;
bool g_fxKeyWasDown = false;
// Per-action gauge rules (nativePC/plugins/ValorGauge.gauge-rules.txt):
//   rewrite <set> <id> <delta> [origVal]  - replace the gauge delta of events
//       occurring while <set:id> is the current action; with origVal only
//       events matching origVal (+-0.005) are rewritten
//   cost <set> <id> <amount>              - subtract amount when the action
//       starts (moves with no native gauge event)
//   gate <set> <id> <minGauge> <nohitSec> - action starts with gauge >=
//       minGauge: inject a no-hit window and clear the gauge (见切 recipe)
//   regen <set> <id> <seconds> <variant>  - activate the spirit-regeneration
//       state on action start: variant 0 = helm-breaker timer (weapon+0x2378,
//       rate params 0x840/0x844 via color flag +0x237C), 1 = iai timer
//       (weapon+0x2388, rate params 0x850/0x854 via timed flag +0x2384).
//       The per-frame updater does the decay + gauge fill + HUD switch.
struct GaugeRule {
    enum class Type { Rewrite, Cost, Gate, Regen };
    Type type = Type::Rewrite;
    std::int32_t set = 3;
    std::int32_t id = 0;
    float a = 0.0f;  // Rewrite: new delta; Cost: amount; Gate: minGauge; Regen: seconds
    float b = 0.0f;  // Rewrite: origVal; Gate: nohitSec; Regen: variant (0=hb, 1=iai)
    bool hasOrig = false;
    float rate = 0.0f;      // Regen optional 5th token: override the rate param
    bool hasRate = false;   // (lower = faster; huge = glow without filling)
};
std::vector<GaugeRule> g_gaugeRules;
// option regen-freeze 1: while a regen timer is active the gauge never
// decreases (negative deltas are zeroed) - "glow without dropping".
std::atomic_bool g_gaugeRegenFreeze{false};

const GaugeRule* FindGaugeRule(GaugeRule::Type type, std::int32_t set,
                               std::int32_t id) {
    for (const auto& rule : g_gaugeRules) {
        if (rule.type == type && rule.set == set && rule.id == id) {
            return &rule;
        }
    }
    return nullptr;
}
void ApplyActionGaugeRules(std::int32_t actionSet, std::int32_t actionId);
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
bool g_negBitPrev = false;
float g_flashPrev = 0.0f;
bool g_fsuccBitPrev = false;
std::chrono::steady_clock::time_point g_counterWindowUntil{};
bool g_counterCtrlWasDown = false;
std::atomic<std::uintptr_t> g_moduleBase{0};
bool g_clangLatch = false; // one force per 见切 action
void TryQueuedForceWrite(const void* entity);
void TryQueuedFx(const void* entity);

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

// --- Local player identification (multiplayer correctness) --------------------
// The original ValorLS resolved the local player through its build's quest
// player-manager global: player = *( *( *(0x145073ED0) ) + 0x50 ). Its .data
// (0x1800329A8 = 0x50/0x76B0/0x00) confirms the 0x76B0 weapon-slot step. The
// current build has no .text reference to that VA (the global may have slid),
// so the result is trusted only after the weapon pointer reads a plausible
// weapon type (1..14). Returns nullptr when the chain does not validate;
// callers fall back to the DoAction first-set-3-controller lock heuristic.
bool IsReadable(const void* address, std::size_t size);

void* ResolveLocalPlayerViaManager() {
    constexpr std::uintptr_t kMgrGlobal = 0x145073ED0;
    constexpr std::uintptr_t kMgrToPlayer = 0x50;
    constexpr std::uintptr_t kPlayerToWeaponSlot = 0x76B0;
    const auto* globalSlot = reinterpret_cast<void* const*>(kMgrGlobal);
    if (!IsReadable(globalSlot, sizeof(void*))) {
        return nullptr;
    }
    auto* mgr = *globalSlot;
    if (!mgr) {
        return nullptr;
    }
    const auto* playerSlot = reinterpret_cast<void* const*>(
        static_cast<std::uint8_t*>(mgr) + kMgrToPlayer);
    if (!IsReadable(playerSlot, sizeof(void*))) {
        return nullptr;
    }
    auto* player = *playerSlot;
    if (!player) {
        return nullptr;
    }
    const auto* weaponSlot = reinterpret_cast<void* const*>(
        static_cast<std::uint8_t*>(player) + kPlayerToWeaponSlot);
    if (!IsReadable(weaponSlot, sizeof(void*))) {
        return nullptr;
    }
    auto* weapon = *weaponSlot;
    if (!weapon || !IsReadable(static_cast<std::uint8_t*>(weapon) + 0x9F8,
                               sizeof(std::uint32_t))) {
        return nullptr;
    }
    const auto weaponType = *reinterpret_cast<const volatile std::uint32_t*>(
        static_cast<std::uint8_t*>(weapon) + 0x9F8);
    if (weaponType < 1 || weaponType > 14) {
        return nullptr;
    }
    return player;
}

void Log(const char* message) {
#ifdef VALOR_PROBE
    constexpr const char* kLogFile = "nativePC/plugins/ValorProbe.log";
#else
    constexpr const char* kLogFile = "nativePC/plugins/ValorGauge.log";
#endif
    std::scoped_lock lock(g_logMutex);
    std::ofstream out(kLogFile, std::ios::app);
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
#ifdef VALOR_PROBE
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
#else
    (void)0; // state logger is probe-only
#endif
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

// --- Update-resilient address map --------------------------------------------
// A game patch moves code. Hook targets resolve by byte signature first
// (wildcards cover rel32 branches and rip-relative displacements), then by
// the recorded RVA of the last known build, then by a manual override file
// nativePC/plugins/ValorGauge.addresses.txt ("name=0xRVA" lines). Signatures
// are generated and verified offline against the unpacked dump exe:
//   python Reverse/sig_check.py --emit && python Reverse/sig_check.py --verify
enum HookId {
    HkCondEval, HkPoolQuery, HkDmgCalc, HkHitDispatch, HkPostEventId,
    HkSoundPost, HkPostEventName, HkPropInt, HkPropBool, HkPropFloat,
    HkGaugeAdd, HkFxNotify, HkCount
};

struct HookSig {
    std::uint8_t pattern[48];
    char mask[49];
    std::uint32_t fallbackRva;
};

constexpr HookSig kHookSigs[HkCount] = {
    // condEval = FUN_14118BDA0 condition-tree node evaluator
    {{0x4C, 0x8B, 0xDC, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x70,
      0x41, 0x83, 0x78, 0x10, 0x00, 0x44, 0x8B, 0xE1, 0x4D, 0x8B, 0x78, 0x08, 0x4D, 0x8B, 0xE8,
      0x48, 0x8B, 0xFA, 0x49, 0x8B, 0xCF, 0x49, 0x8B, 0x07, 0x0F, 0x86, 0x7F, 0x01, 0x00, 0x00,
      0x49, 0x89, 0x5B},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxx", 0x118BDA0},
    // poolQuery = FUN_140AEE050 defense/no-hit query
    {{0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x8D, 0x42, 0xFF, 0x0F, 0x29,
      0x74, 0x24, 0x20, 0x8B, 0xFA, 0x48, 0x8B, 0xD9, 0x0F, 0x57, 0xF6, 0x0F, 0x57, 0xC0, 0x83,
      0xF8, 0x05, 0x0F, 0x87, 0x4A, 0x01, 0x00, 0x00, 0x48, 0x8D, 0x0D, 0x83, 0x1F, 0x51, 0xFF,
      0x44, 0x8B, 0x84},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxx????xxx", 0xAEE050},
    // dmgCalc = FUN_141F66DD0 damage calculation entry
    {{0x48, 0x8B, 0xC4, 0x48, 0x89, 0x50, 0x10, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55,
      0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0x68, 0x98, 0x48, 0x81, 0xEC, 0x28, 0x01, 0x00, 0x00,
      0x48, 0x8B, 0xBA, 0xB8, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xF1, 0x0F, 0x29, 0x70, 0xA8, 0x49,
      0x8B, 0xD8, 0x0F},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 0x1F66DD0},
    // hitDispatch = FUN_141F71960 hit dispatcher
    {{0x40, 0x53, 0x57, 0x48, 0x83, 0xEC, 0x48, 0x48, 0x8B, 0xFA, 0x48, 0x8B, 0xD9, 0xE8, 0x4E,
      0xF8, 0xC8, 0xFF, 0x48, 0x8B, 0x07, 0x48, 0x8B, 0xCF, 0xFF, 0x50, 0x08, 0x83, 0xC0, 0xFA,
      0x83, 0xF8, 0x6A, 0x0F, 0x87, 0xF2, 0x08, 0x00, 0x00, 0x48, 0x89, 0x74, 0x24, 0x70, 0x48,
      0x8D, 0x15, 0x6D},
     "xxxxxxxxxxxxxx????xxxxxxxxxxxxxxxxx????xxxxxxxxx", 0x1F71960},
    // postEventId = AK::SoundEngine::PostEvent (id)
    {{0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18,
      0x4C, 0x89, 0x74, 0x24, 0x20, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x60, 0x44, 0x8B, 0xF9, 0x49,
      0x8B, 0xF1, 0x48, 0x8D, 0x0D, 0x8F, 0xAE, 0xCA, 0x02, 0x41, 0x8B, 0xE8, 0x4C, 0x8B, 0xF2,
      0xE8, 0x1A, 0x55},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxxxx", 0x28B8DD0},
    // soundPost = game audio wrapper FUN_1426D4C50
    {{0x48, 0x83, 0xEC, 0x48, 0x8B, 0xC2, 0x45, 0x8B, 0xD1, 0x41, 0x8B, 0xD0, 0x4C, 0x8D, 0x0D,
      0x8D, 0xF7, 0xFF, 0xFF, 0x45, 0x33, 0xC0, 0x44, 0x89, 0x44, 0x24, 0x38, 0x4C, 0x89, 0x44,
      0x24, 0x30, 0x44, 0x89, 0x44, 0x24, 0x28, 0x45, 0x8B, 0xC2, 0x48, 0x89, 0x4C, 0x24, 0x20,
      0x8B, 0xC8, 0xE8},
     "xxxxxxxxxxxxxxx????xxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 0x26D4C50},
    // postEventName = AK::SoundEngine::PostEvent (string)
    {{0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10, 0x48, 0x89, 0x7C, 0x24, 0x18,
      0x41, 0x54, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x60, 0x4C, 0x8B, 0xE1, 0x49, 0x8B,
      0xE9, 0x48, 0x8D, 0x0D, 0x80, 0xAC, 0xCA, 0x02, 0x45, 0x8B, 0xF0, 0x4C, 0x8B, 0xFA, 0xE8,
      0x0B, 0x53, 0x3C},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxxxxx", 0x28B8FE0},
    // propInt = FUN_141922190 FSM property int getter
    {{0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x41, 0x8B, 0xF8, 0x41, 0x8B,
      0xD9, 0x48, 0x85, 0xC9, 0x74, 0x34, 0xF6, 0x41, 0x60, 0x01, 0x74, 0x2E, 0x48, 0x8B, 0x89,
      0xD8, 0x26, 0x01, 0x00, 0xE8, 0x39, 0x5F, 0x9D, 0xFF, 0x85, 0xC0, 0x74, 0x1E, 0x48, 0x8B,
      0x0D, 0x86, 0xFE},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxxxx", 0x1922190},
    // propBool = FUN_1419221F0 FSM property bool getter
    {{0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x41, 0x8B, 0xD8, 0x48, 0x85, 0xC9, 0x74, 0x34, 0xF6,
      0x41, 0x60, 0x01, 0x74, 0x2E, 0x48, 0x8B, 0x89, 0xD8, 0x26, 0x01, 0x00, 0xE8, 0xE0, 0x5E,
      0x9D, 0xFF, 0x85, 0xC0, 0x74, 0x1E, 0x48, 0x8B, 0x0D, 0x2D, 0xFE, 0x6E, 0x03, 0x8B, 0xD0,
      0xE8, 0xDE, 0x04},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxx????xxxxx", 0x19221F0},
    // propFloat = FUN_141922240 FSM property float getter
    {{0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x0F, 0x29, 0x74, 0x24, 0x20, 0x0F, 0x28, 0xF3, 0x41,
      0x8B, 0xD8, 0x48, 0x85, 0xC9, 0x74, 0x43, 0xF6, 0x41, 0x60, 0x01, 0x74, 0x3D, 0x48, 0x8B,
      0x89, 0xD8, 0x26, 0x01, 0x00, 0xE8, 0x88, 0x5E, 0x9D, 0xFF, 0x85, 0xC0, 0x74, 0x2D, 0x48,
      0x8B, 0x0D, 0xD5},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxxx", 0x1922240},
    // gaugeAdd = FUN_142122410 the ONLY float writer of weapon+0x2368 (spirit
    // gauge): every per-move gain/cost funnels through it. The 0x14211EF90 twin
    // accumulates +0x236C instead; the call displacement bytes (DF 0F) keep the
    // signature pinned to the +0x2368 variant.
    {{0x40, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x0F, 0x29, 0x74, 0x24, 0x30, 0x48, 0x8B, 0xD9, 0x0F,
      0x29, 0x7C, 0x24, 0x20, 0x0F, 0x57, 0xF6, 0x0F, 0x28, 0xF9, 0x0F, 0x2F, 0xFE, 0x76, 0x21,
      0x48, 0x8B, 0x81, 0xB0, 0x09, 0x00, 0x00, 0x48, 0x8B, 0x88, 0x20, 0x7D, 0x00, 0x00, 0xE8,
      0xDF, 0x0F, 0x0A},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 0x2122410},
    // fxNotify = FUN_140ADD670 player-emitter effect notify (body auras). The
    // 0x201DCB0 twin shares 32 bytes; the post-call movss/lea tail (offsets
    // 36-46) discriminates. Wildcards: call rel32 + rip-relative disp32s.
    {{0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x81, 0xEC, 0x30,
      0x01, 0x00, 0x00, 0x48, 0x8B, 0xF1, 0x41, 0x8B, 0xD8, 0x48, 0x8D, 0x4C, 0x24, 0x40, 0x8B,
      0xFA, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xF3, 0x0F, 0x10, 0x05, 0x00, 0x00, 0x00, 0x00, 0x48,
      0x8D, 0x05},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxx????xxx", 0xADD670},
};

const char* const kHookNames[HkCount] = {
    "condEval", "poolQuery", "dmgCalc", "hitDispatch", "postEventId",
    "soundPost", "postEventName", "propInt", "propBool", "propFloat",
    "gaugeAdd", "fxNotify",
};

std::uintptr_t g_hookAddr[HkCount] = {};
std::uintptr_t g_addrAudioSingleton = 0x51C458; // audio manager singleton slot
std::uintptr_t g_addrPropRegistry = 0x5012048;  // FSM property registry (count +0xB8, rows +0xC8)
std::uintptr_t g_addrPropLookup = 0x462700;     // registry row lookup FUN_140462700
std::uintptr_t g_addrPropName = 0x1979770;      // property id -> name
std::uintptr_t g_addrDoAction = 0;              // set by InstallActionHook

std::map<std::string, std::uintptr_t> g_addrOverrides;

std::uintptr_t ModuleBaseAddr() {
    const HMODULE game = GetModuleHandleW(nullptr);
    MODULEINFO module{};
    if (!game || !GetModuleInformation(GetCurrentProcess(), game, &module, sizeof(module))) {
        return 0;
    }
    return reinterpret_cast<std::uintptr_t>(module.lpBaseOfDll);
}

void LoadAddressOverrides() {
    std::ifstream input("nativePC/plugins/ValorGauge.addresses.txt");
    if (!input) {
        return;
    }
    std::string line;
    int count = 0;
    while (std::getline(input, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const auto trim = [](std::string s) {
            while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
                s.erase(s.begin());
            }
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
                s.pop_back();
            }
            return s;
        };
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        if (key.empty() || value.empty()) {
            continue;
        }
        g_addrOverrides[key] = std::strtoull(value.c_str(), nullptr, 0);
        ++count;
    }
    if (count > 0) {
        std::ostringstream m;
        m << "MEMORYMAP: " << count << " address override(s) loaded from ValorGauge.addresses.txt";
        Log(m.str());
    }
}

std::uintptr_t OverrideOrZero(const char* name) {
    const auto it = g_addrOverrides.find(name);
    return it == g_addrOverrides.end() ? 0 : it->second;
}

// Resolve one hook target: override > unique signature > last-known RVA.
std::uintptr_t ResolveHookTarget(HookId id) {
    const HookSig& sig = kHookSigs[id];
    const std::uintptr_t base = ModuleBaseAddr();
    if (const std::uintptr_t overrideRva = OverrideOrZero(kHookNames[id])) {
        std::ostringstream m;
        m << "MEMORYMAP: " << kHookNames[id] << " via addresses.txt override";
        Log(m.str());
        return base + overrideRva;
    }
    auto* hit = FindPattern(sig.pattern, sig.mask, sizeof(sig.pattern));
    if (hit) {
        // Reject ambiguous patterns: a second match means the signature is too
        // weak to trust after an update.
        const auto start = reinterpret_cast<const std::uint8_t*>(base);
        const std::size_t next = static_cast<std::size_t>(hit - start) + 1;
        bool ambiguous = false;
        if (const std::uintptr_t moduleBase = ModuleBaseAddr()) {
            const HMODULE game = GetModuleHandleW(nullptr);
            MODULEINFO module{};
            if (game && GetModuleInformation(GetCurrentProcess(), game, &module, sizeof(module))) {
                const auto scanStart = static_cast<const std::uint8_t*>(module.lpBaseOfDll);
                const auto imageSize = static_cast<std::size_t>(module.SizeOfImage);
                for (std::size_t i = next; i + sizeof(sig.pattern) <= imageSize; ++i) {
                    bool matches = true;
                    for (std::size_t j = 0; j < sizeof(sig.pattern); ++j) {
                        if (sig.mask[j] == 'x' && scanStart[i + j] != sig.pattern[j]) {
                            matches = false;
                            break;
                        }
                    }
                    if (matches) {
                        ambiguous = true;
                        break;
                    }
                }
            }
        }
        if (ambiguous) {
            std::ostringstream m;
            m << "MEMORYMAP: " << kHookNames[id]
              << " signature matched more than once - using recorded RVA instead";
            Log(m.str());
        } else {
            return reinterpret_cast<std::uintptr_t>(hit);
        }
    }
    std::ostringstream m;
    m << "MEMORYMAP: " << kHookNames[id] << std::hex
      << " signature miss - falling back to RVA 0x" << sig.fallbackRva
      << " (game update? verify with Reverse/sig_check.py)";
    Log(m.str());
    return base + sig.fallbackRva;
}

void InitMemoryMap() {
    LoadAddressOverrides();
    for (int i = 0; i < HkCount; ++i) {
        g_hookAddr[i] = ResolveHookTarget(static_cast<HookId>(i));
    }
    const std::uintptr_t base = ModuleBaseAddr();
    if (const std::uintptr_t v = OverrideOrZero("audioSingleton")) {
        g_addrAudioSingleton = base + v;
    } else {
        g_addrAudioSingleton = base + g_addrAudioSingleton;
    }
    if (const std::uintptr_t v = OverrideOrZero("propRegistry")) {
        g_addrPropRegistry = base + v;
    } else {
        g_addrPropRegistry = base + g_addrPropRegistry;
    }
    if (const std::uintptr_t v = OverrideOrZero("propLookup")) {
        g_addrPropLookup = base + v;
    } else {
        g_addrPropLookup = base + g_addrPropLookup;
    }
    if (const std::uintptr_t v = OverrideOrZero("propName")) {
        g_addrPropName = base + v;
    } else {
        g_addrPropName = base + g_addrPropName;
    }
}

// One-time sanity report: every resolved target must be readable, and the two
// global data structures must still look like themselves. On failure the log
// names the exact anchor to re-derive (see Reverse/ADDRESS_RECOVERY.md).
void ValidateMemoryMap() {
    std::ostringstream d;
    d << "MEMORYMAP doAction "
      << (IsReadable(reinterpret_cast<void*>(g_addrDoAction), 16) ? "ok" : "BAD");
    Log(d.str());
    std::ostringstream m;
    m << "MEMORYMAP hooks:";
    for (int i = 0; i < HkCount; ++i) {
        m << ' ' << kHookNames[i] << '='
          << (IsReadable(reinterpret_cast<void*>(g_hookAddr[i]), 16) ? "ok" : "BAD");
    }
    Log(m.str());

    const char* registryStatus = "BAD";
    if (IsReadable(reinterpret_cast<void*>(g_addrPropRegistry), 0xD0)) {
        const auto count =
            *reinterpret_cast<const std::uint32_t*>(g_addrPropRegistry + 0xB8);
        const auto rowTable =
            *reinterpret_cast<void* const*>(g_addrPropRegistry + 0xC8);
        if (count >= 200 && count <= 2000000 && IsReadable(rowTable, sizeof(void*))) {
            registryStatus = "ok";
        }
        std::ostringstream r;
        r << "MEMORYMAP propRegistry count=" << count << " rowTable="
          << (IsReadable(rowTable, sizeof(void*)) ? "ok" : "BAD") << " -> " << registryStatus;
        Log(r.str());
    } else {
        Log("MEMORYMAP propRegistry UNREADABLE - base+0x5012048 moved?");
    }

    const char* audioStatus = "BAD";
    if (IsReadable(reinterpret_cast<void*>(g_addrAudioSingleton), sizeof(void*))) {
        const auto singleton = *reinterpret_cast<void* const*>(g_addrAudioSingleton);
        audioStatus = singleton ? "ok" : "null";
    }
    std::ostringstream a;
    a << "MEMORYMAP audioSingleton " << audioStatus;
    Log(a.str());
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

#ifndef VALOR_PROBE
// --- Mod weapon gate ---------------------------------------------------------
// The gameplay mod (gauge / clang counter / force) only drives the Long Sword.
// Weapon type = *(int32*)(weapon + 0x9F8) - the same value func_0x141F627F0
// returns and FUN_14118bda0 switches on; **Long Sword = 3** (its case holds
// the LS-only condition names 練気ゲージ判定/居合斬り/見切り回避成功後の攻撃ヒット;
// matches wp03 data folder, w03 FSM files and action set 3). 0xFF = no weapon.
// Override via nativePC/plugins/ValorGauge.weapon-type.txt if a patch
// renumbers the enum.
std::atomic<std::uint32_t> g_modWeaponType{3};

bool ModWeaponActive() {
    void* controller = g_lockedPlayerController;
    if (!controller) {
        return false;
    }
    auto* weaponSlot = static_cast<std::uint8_t*>(controller) - 0x61C8 + 0x76B0;
    if (!IsReadable(weaponSlot, sizeof(void*))) {
        return false;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(weaponSlot);
    if (!weapon || !IsReadable(weapon + 0x9F8, sizeof(std::uint32_t))) {
        return false;
    }
    const auto type = *reinterpret_cast<const volatile std::uint32_t*>(weapon + 0x9F8);
    const bool active = type == g_modWeaponType.load(std::memory_order_relaxed);
    static int gateLogs = 0;
    if (gateLogs < 6) {
        ++gateLogs;
        std::ostringstream m;
        m << "WEAPON GATE " << (active ? "OPEN" : "CLOSED") << ": weapon+0x9F8="
          << type << " expected=" << g_modWeaponType.load(std::memory_order_relaxed);
        Log(m.str());
    }
    return active;
}

void LoadModWeaponTypeConfig() {
    std::ifstream input("nativePC/plugins/ValorGauge.weapon-type.txt");
    int type = 0;
    if (input >> type && type >= 0 && type <= 0xFE) {
        g_modWeaponType.store(static_cast<std::uint32_t>(type), std::memory_order_relaxed);
    }
}

// --- Sheathed-state draw rewrite (收刀拔刀刚1) -------------------------------
// Vanilla routes sheathed R2 to {3,11} (draw spirit blade I) purely in code +
// per-weapon static tables; the weapon FSM has no sheathed links to edit
// (CLAUDE.md 2026-09-25). So the rewrite happens on the request itself, in
// DoActionHook, before the action tracker sees it. Sheathed test: the tracked
// previous action is set-1 (sheathed locomotion family 1:0/1:4/1:8/1:5);
// drawn locomotion never emits requests. weapon+0xC71 was NOT usable (read 0
// while sheathed; SHEATHPROBE lines print it for the research backlog).
// Gate (user rule): spirit level >= reqLevel (3 = red) and gauge
// (weapon+0x2368 float) above the threshold, else the vanilla sheathed draw
// spirit blade plays.
// Config nativePC/plugins/ValorGauge.sheath-rewrite.txt:
//   "<toId> <holdVK> <reqLevel> <minGauge> <fromId> [fromId...]"
//   default "86 0 3 0.15 0.2 11"  (toId 0 = off, holdVK 0 = no key gate,
//   reqLevel 0 = no level gate, minGauge < 0 = no gauge gate,
//   nohitSec <= 0 = no injection)
// I-frames: from the sheathed path the FSM stays in 待機(1000) (request does
// not enter node 28), so the vanilla TIML/FSM defense window never opens.
// The rewrite therefore injects the same pool+0xE4 no-hit window the clang
// triggers use, for <nohitSec> seconds starting at the move's first frame.
std::atomic<std::uint32_t> g_sheathRewriteToId{86};
std::atomic<std::uint32_t> g_sheathRewriteHoldVK{0};
std::atomic<std::uint32_t> g_sheathRewriteReqLevel{3};
std::atomic<float> g_sheathRewriteMinGauge{0.15f};
std::atomic<float> g_sheathRewriteNohitSec{0.2f};
std::vector<std::uint32_t> g_sheathRewriteFromIds{11};
std::mutex g_sheathRewriteMutex;

void InjectNoHitLocked(float seconds);

void LoadSheathRewriteConfig() {
    int toId = 86;
    int holdVk = 0;
    int reqLevel = 3;
    float minGauge = 0.15f;
    float nohitSec = 0.2f;
    std::vector<std::uint32_t> fromIds{11};
    std::ifstream input("nativePC/plugins/ValorGauge.sheath-rewrite.txt");
    int value = 0;
    if (input >> value && value >= 0 && value <= 0xFFFF) {
        toId = value;
        if (input >> value && value >= 0 && value <= 0xFF) {
            holdVk = value;
        }
        if (input >> value && value >= 0 && value <= 3) {
            reqLevel = value;
        }
        if (!(input >> minGauge)) {
            minGauge = 0.15f;
        }
        if (!(input >> nohitSec) || nohitSec < -1.0f || nohitSec > 3.0f) {
            nohitSec = 0.2f;
        }
        while (input >> value && value >= 0 && value <= 0xFFFF) {
            fromIds.push_back(static_cast<std::uint32_t>(value));
        }
    }
    if (fromIds.empty()) {
        fromIds.push_back(11);
    }
    {
        std::scoped_lock lock(g_sheathRewriteMutex);
        g_sheathRewriteFromIds = std::move(fromIds);
    }
    g_sheathRewriteToId.store(static_cast<std::uint32_t>(toId), std::memory_order_relaxed);
    g_sheathRewriteHoldVK.store(static_cast<std::uint32_t>(holdVk), std::memory_order_relaxed);
    g_sheathRewriteReqLevel.store(static_cast<std::uint32_t>(reqLevel), std::memory_order_relaxed);
    g_sheathRewriteMinGauge.store(minGauge, std::memory_order_relaxed);
    g_sheathRewriteNohitSec.store(nohitSec, std::memory_order_relaxed);
    std::ostringstream msg;
    msg << "Sheath rewrite: sheathed R2 requests 3:{";
    {
        std::scoped_lock lock(g_sheathRewriteMutex);
        for (std::size_t i = 0; i < g_sheathRewriteFromIds.size(); ++i) {
            if (i) {
                msg << ",";
            }
            msg << g_sheathRewriteFromIds[i];
        }
    }
    msg << "} -> 3:" << toId << " (toId 0 = off), holdVK=" << holdVk
        << ", reqLevel=" << reqLevel << ", minGauge=" << minGauge
        << ", nohit=" << nohitSec << "s";
    Log(msg.str());
}

// Local-player test for pre-lock requests: the session lock is authoritative;
// before the first set-3 action establishes it, accept the same manager-chain
// verdict the lock itself uses, so the very first sheathed R2 already rewrites.
bool IsLocalPlayerActionController(void* controller) {
    if (!controller) {
        return false;
    }
    if (controller == g_lockedPlayerController) {
        return true;
    }
    if (g_lockedPlayerController) {
        return false;
    }
    void* chainPlayer = ResolveLocalPlayerViaManager();
    return chainPlayer && chainPlayer == static_cast<std::uint8_t*>(controller) - 0x61C8;
}

void ApplySheathRewrite(void* controller, ActionInfo* action) {
    const auto toId = g_sheathRewriteToId.load(std::memory_order_relaxed);
    if (!toId) {
        return;
    }
    if (!IsLocalPlayerActionController(controller)) {
        return;
    }
    const auto fromId = static_cast<std::uint32_t>(action->actionId);
    {
        std::scoped_lock lock(g_sheathRewriteMutex);
        if (std::find(g_sheathRewriteFromIds.begin(), g_sheathRewriteFromIds.end(), fromId) ==
            g_sheathRewriteFromIds.end()) {
            return;
        }
    }
    const auto holdVk = g_sheathRewriteHoldVK.load(std::memory_order_relaxed);
    if (holdVk && (GetAsyncKeyState(static_cast<int>(holdVk)) & 0x8000) == 0) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (!IsReadable(player + 0x76B0, sizeof(void*))) {
        return;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
    if (!weapon || !IsReadable(weapon, 0x2378)) {
        return;
    }
    if (*reinterpret_cast<const volatile std::uint32_t*>(weapon + 0x9F8) !=
        g_modWeaponType.load(std::memory_order_relaxed)) {
        return;
    }
    // Sheathed test via the action tracker: set-1 requests are the sheathed
    // locomotion family only (1:0 idle / 1:4 walk / 1:8 run / 1:5 draw-prep);
    // drawn-state locomotion never emits DoAction requests, so a set-1
    // predecessor means sheathed. (weapon+0xC71 read 0 while sheathed in-game
    // - SHEATHPROBE keeps printing it to pin its real semantics.)
    std::int32_t lastSet = -1;
    std::int32_t lastId = -1;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction) {
            lastSet = g_lastLongSwordAction->actionSet;
            lastId = g_lastLongSwordAction->actionId;
        }
    }
    const auto c71 = static_cast<int>(*(weapon + 0xC71));
    const auto level = *reinterpret_cast<const volatile std::int32_t*>(weapon + 0x2370);
    const auto gauge = *reinterpret_cast<const volatile float*>(weapon + 0x2368);
    if (lastSet != 1) {
        static std::atomic_uint32_t probeLogs{0};
        if (probeLogs.fetch_add(1, std::memory_order_relaxed) < 12) {
            std::ostringstream m;
            m << "SHEATHPROBE 3:" << fromId << " blocked: last=" << lastSet << ":"
              << lastId << " c71=" << c71 << " level=" << level << " gauge=" << gauge;
            Log(m.str());
        }
        return;
    }
    const auto reqLevel = g_sheathRewriteReqLevel.load(std::memory_order_relaxed);
    const auto minGauge = g_sheathRewriteMinGauge.load(std::memory_order_relaxed);
    if (reqLevel && level < static_cast<std::int32_t>(reqLevel)) {
        static std::atomic_uint32_t rejectLogs{0};
        if (rejectLogs.fetch_add(1, std::memory_order_relaxed) < 12) {
            std::ostringstream m;
            m << "SHEATHREWRITE reject 3:" << fromId << " level=" << level
              << " (need >= " << reqLevel << ") gauge=" << gauge;
            Log(m.str());
        }
        return;
    }
    if (minGauge >= 0.0f && !(gauge > minGauge)) {
        static std::atomic_uint32_t rejectLogs{0};
        if (rejectLogs.fetch_add(1, std::memory_order_relaxed) < 12) {
            std::ostringstream m;
            m << "SHEATHREWRITE reject 3:" << fromId << " level=" << level
              << " gauge=" << gauge << " (need > " << minGauge << ")";
            Log(m.str());
        }
        return;
    }
    const auto nohitSec = g_sheathRewriteNohitSec.load(std::memory_order_relaxed);
    if (nohitSec > 0.0f) {
        // The FSM never left 待機(1000), so no vanilla defense window opens
        // for this move - inject the clang-trigger no-hit window instead.
        InjectNoHitLocked(nohitSec);
    }
    std::ostringstream m;
    m << "SHEATHREWRITE: 3:" << fromId << " -> 3:" << toId
      << " (level=" << level << " gauge=" << gauge << " nohit=" << nohitSec << "s)";
    Log(m.str());
    action->actionId = static_cast<std::int32_t>(toId);
}
#endif

bool __fastcall DoActionHook(void* controller, ActionInfo* action) {
    const bool validAction = controller && action && IsReadable(action, sizeof(ActionInfo));
    g_insideDoAction.store(true, std::memory_order_release);
    if (validAction) {
#ifdef VALOR_PROBE
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
#endif
    }

#ifdef VALOR_PROBE
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
#endif

#ifndef VALOR_PROBE
    // 收刀拔刀刚1: vanilla routes sheathed R2 to {3,11} in code; rewrite the
    // request BEFORE the tracker copies it so latch/rules/logs see the new id.
    if (validAction && action->actionSet == 3 && IsLongSwordController(controller)) {
        ApplySheathRewrite(controller, action);
    }
#endif

    if (validAction && IsLongSwordController(controller)) {
        const ActionInfo current = *action;
        bool changed = false;
        bool isLockedPlayer = false;
        {
            std::scoped_lock lock(g_actionMutex);
            // Weapon actions use set 3. Once observed, keep this controller for the session so
            // monster/secondary controllers that happen to pass the structural test are ignored.
            // Multiplayer: another hunter's weapon action must never win the lock. The manager
            // chain (when it validates) is authoritative; without it we only lock when no
            // chain-based objection exists, and log the refusal a few times.
            if (!g_lockedPlayerController && current.actionSet == 3) {
                void* chainPlayer = ResolveLocalPlayerViaManager();
                const auto candidatePlayer = static_cast<std::uint8_t*>(controller) - 0x61C8;
                if (chainPlayer && chainPlayer != candidatePlayer) {
                    static std::atomic_uint32_t foreignActionLogs{0};
                    if (foreignActionLogs.load(std::memory_order_relaxed) < 8) {
                        foreignActionLogs.fetch_add(1, std::memory_order_relaxed);
                        std::ostringstream refused;
                        refused << "FOREIGN set-3 action: controller 0x" << std::hex
                                << std::uppercase
                                << reinterpret_cast<std::uintptr_t>(controller)
                                << " is not the local player (manager chain player 0x"
                                << reinterpret_cast<std::uintptr_t>(chainPlayer)
                                << ") - not locking";
                        Log(refused.str());
                    }
                } else {
                g_lockedPlayerController = controller;
                if (chainPlayer) {
                    Log("Local player controller lock confirmed by manager chain");
                }
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
            }
            isLockedPlayer = controller == g_lockedPlayerController;
            changed = isLockedPlayer &&
                      (controller != g_lastLongSwordController || !g_lastLongSwordAction ||
                       g_lastLongSwordAction->actionSet != current.actionSet ||
                       g_lastLongSwordAction->actionId != current.actionId);
            if (changed) {
                g_lastLongSwordController = controller;
                g_lastLongSwordAction = current;
#ifndef VALOR_PROBE
                // Re-arm the clang latch only when a NEW foresight slash starts;
                // resetting on every action change caused a force loop (the
                // injected window kept producing rejections during the counter).
                if (current.actionSet == 3 && (current.actionId == 86 ||
                    current.actionId == 101 || current.actionId == 102)) {
                    g_clangLatch = false;
                }
                const bool normalAction = IsNormalModeChargingAction(current) &&
                                          ModWeaponActive();
                if (g_valorMode.load() && normalAction) {
                    g_valorMode = false;
                    g_valorCharge = 0;
                    Log("Valor mode exited via normal-mode action; charging re-enabled");
                }
                // Legacy charging: +10 on action START (fires even on a whiff).
                // Only active when hit-charge is set to 0 in hit-charge.txt;
                // the default path charges in DmgCalcHook on CONNECTED hits.
                if (normalAction && g_hitChargePercent.load(std::memory_order_relaxed) == 0) {
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
#endif
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
#ifndef VALOR_PROBE
    if (result && validAction && controller == g_lockedPlayerController &&
        action->actionSet == 3) {
        ApplyActionGaugeRules(action->actionSet, action->actionId);
    }
#endif
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
std::atomic_uint32_t g_rollDispatchLog{0};
std::atomic_uint32_t g_vseekLogCount{0};
bool ActionIsForesightSlash() {
    std::scoped_lock lock(g_actionMutex);
    auto& a = g_lastLongSwordAction;
    return a && a->actionSet == 3 && a->actionId == 86;
}

// Drawn-state dodge roll (user-identified: action 3:18). Research-only: the
// probe watches its i-frames; the mod never triggers on it.
bool ActionIsRoll() {
    std::scoped_lock lock(g_actionMutex);
    auto& a = g_lastLongSwordAction;
    return a && a->actionSet == 3 && a->actionId == 18;
}

#ifdef VALOR_PROBE
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
#endif // VALOR_PROBE (pool query diagnostics)

// --- 刚1 clang detector (hit dispatcher) --------------------------------------
// FUN_141f71960 routes every hit attempt; 0xD returns are the per-frame
// ambient no-collision checks. Live diagnostics (log-verified): during 见切
// (3:86) a negated hit (the clang) returns a NON-0xD code and the damage calc
// (FUN_141f66dd0) is never entered; a real hit enters the damage calc. So:
// clang = non-0xD return while action is 3:86 and no damage processing ran
// for this dispatch. Fires on the game thread at the clang frame.
// FUN_141f66dd0 = damage calculation entry (3 args). a1 is the ATTACKER
// entity: its body dereferences a1+0x7D20 (the player mgr pointer), and for
// player-dealt hits a1 equals the player pointer. The hook is session-wide -
// in multiplayer it fires for every hunter's and monster's connected hits -
// so the mod filters a1 against the locked local player before charging.
using DmgCalcFn = long long(__fastcall*)(long long, long long, std::uint32_t*);
DmgCalcFn g_originalDmgCalc = nullptr;

#ifdef VALOR_PROBE
bool g_damageThisDispatch = false; // game thread only

long long __fastcall DmgCalcHook(long long a1, long long a2, std::uint32_t* a3) {
    g_damageThisDispatch = true;
    return g_originalDmgCalc(a1, a2, a3);
}
#else
// --- Hit-based valor charging -------------------------------------------------
// Hits from other entities in multiplayer, counted for diagnostics only.
std::atomic_uint32_t g_hitChargeSkips{0};

// nativePC/plugins/ValorGauge.hit-charge.txt: "<percent>"
//   >0  charge percent per CONNECTED hit of a whitelisted normal-mode move
//       (default 10; multi-hit moves tick per hit)
//    0  legacy behavior: charge on action start (pre-hit-detection builds)
//   <0  charging disabled entirely
void LoadHitChargeConfig() {
    std::ifstream input("nativePC/plugins/ValorGauge.hit-charge.txt");
    int percent = 0;
    if (input >> percent) {
        g_hitChargePercent.store(percent, std::memory_order_relaxed);
    }
    std::ostringstream m;
    m << "Hit-charge config: " << g_hitChargePercent.load(std::memory_order_relaxed)
      << "% per connected hit"
      << (g_hitChargePercent.load(std::memory_order_relaxed) == 0
              ? " (legacy action-start charging)"
              : "");
    Log(m.str());
}

// nativePC/plugins/ValorGauge.gauge-lock.txt: "[<target>] <value 0..1>"
//   target 1 = weapon+0x2374 (level-duration timer), 2 = weapon+0x2368 (the
//   visible spirit gauge candidate); bare "<value>" = target 1.
//   F5 toggles the lock at runtime. Both fields are watched either way.
void LoadGaugeLockConfig() {
    std::ifstream input("nativePC/plugins/ValorGauge.gauge-lock.txt");
    float first = 0.0f;
    float second = 0.0f;
    int target = 1;
    float value = 0.0f;
    if (input >> first >> second) {
        target = static_cast<int>(first);
        value = second;
    } else {
        input.clear();
        input.seekg(0);
        if (input >> first) {
            target = 1;
            value = first;
        }
    }
    if (target != 1 && target != 2) {
        target = 1;
    }
    g_gaugeLockTarget.store(target, std::memory_order_relaxed);
    g_gaugeLockValue.store(value, std::memory_order_relaxed);
    g_gaugeLockEnabled.store(value > 0.0f && value <= 1.0f,
                             std::memory_order_relaxed);
    std::ostringstream m;
    m << "Gauge-lock config: target=weapon+0x"
      << (target == 2 ? "2368" : "2374") << " value="
      << g_gaugeLockValue.load(std::memory_order_relaxed) << ", "
      << (g_gaugeLockEnabled.load(std::memory_order_relaxed)
              ? "ON (F5 toggles)"
              : "off");
    Log(m.str());
}

void LoadGaugeLogConfig() {
    std::ifstream input("nativePC/plugins/ValorGauge.gauge-log.txt");
    float minAbs = 0.01f;
    if (input >> minAbs) {
        g_gaugeLogMinAbs.store(minAbs < 0.0f ? 0.0f : minAbs,
                               std::memory_order_relaxed);
    }
    std::ostringstream m;
    m << "Gauge-event log: |delta| >= "
      << g_gaugeLogMinAbs.load(std::memory_order_relaxed)
      << " (ValorGauge.gauge-log.txt)";
    Log(m.str());
}

// FUN_142122410 choke point: every spirit-gauge delta (per-move gains, spirit
// attack costs, iai refills, the 见切 clear) passes here. Log the delta with
// the caller RVA + pending-cost slot + current action; one session maps the
// whole move->gauge table. Rate-capped at 40 lines/s. Rewrite rules (local
// player only) replace the delta before the original runs.
void __fastcall GaugeAddHook(void* weapon, float value) {
    void* returnAddress = _ReturnAddress();
    float before = 0.0f;
    float costSlot = 0.0f;
    bool hasWeapon = false;
    bool own = false;
    std::int32_t actSet = 0;
    std::int32_t actId = 0;
    bool hasAct = false;
    if (weapon &&
        IsReadable(static_cast<std::uint8_t*>(weapon) + 0x2374,
                   sizeof(float))) {
        hasWeapon = true;
        before = *reinterpret_cast<const volatile float*>(
            static_cast<std::uint8_t*>(weapon) + 0x2368);
        auto* playerSlot = static_cast<std::uint8_t*>(weapon) + 0x9B0;
        if (IsReadable(playerSlot, sizeof(void*))) {
            auto* player = *reinterpret_cast<std::uint8_t**>(playerSlot);
            // player+0x7810 holds a POINTER to the runtime w03p param table.
            if (player && IsReadable(player + 0x7810, sizeof(void*))) {
                auto* paramTable =
                    *reinterpret_cast<std::uint8_t**>(player + 0x7810);
                if (paramTable && IsReadable(paramTable + 0x804, sizeof(float))) {
                    costSlot = *reinterpret_cast<const volatile float*>(
                        paramTable + 0x804);
                }
            }
            std::scoped_lock lock(g_actionMutex);
            if (g_lockedPlayerController) {
                auto* lockedPlayer =
                    static_cast<std::uint8_t*>(g_lockedPlayerController) -
                    0x61C8;
                own = player == lockedPlayer;
            }
            if (g_lastLongSwordAction) {
                actSet = g_lastLongSwordAction->actionSet;
                actId = g_lastLongSwordAction->actionId;
                hasAct = true;
            }
        }
    }
    if (own && hasAct && !g_gaugeRules.empty()) {
        if (const auto* rule =
                FindGaugeRule(GaugeRule::Type::Rewrite, actSet, actId)) {
            constexpr float kMatchEpsilon = 0.005f;
            if (!rule->hasOrig ||
                (value > rule->b - kMatchEpsilon &&
                 value < rule->b + kMatchEpsilon)) {
                std::ostringstream m;
                m << "GAUGERULE rewrite act=" << actSet << ':' << actId
                  << " val " << value << " -> " << rule->a;
                Log(m.str());
                value = rule->a;
            }
        }
    }
    // regen-freeze: while a regeneration timer is running the gauge must not
    // drop - zero out negative deltas (the original still runs for its
    // cost-slot bookkeeping side effects).
    if (own && value < 0.0f &&
        g_gaugeRegenFreeze.load(std::memory_order_relaxed) && hasWeapon) {
        const float hb = *reinterpret_cast<const volatile float*>(
            static_cast<std::uint8_t*>(weapon) + 0x2378);
        const float iai = *reinterpret_cast<const volatile float*>(
            static_cast<std::uint8_t*>(weapon) + 0x2388);
        if (hb > 0.0f || iai > 0.0f) {
            std::ostringstream m;
            m << "GAUGERULE freeze: blocked " << value
              << " during regen (hb=" << hb << " iai=" << iai << ')';
            Log(m.str());
            value = 0.0f;
        }
    }
    if (g_originalGaugeAdd) {
        g_originalGaugeAdd(weapon, value);
    }
    if (!hasWeapon) {
        return;
    }
    const float after = *reinterpret_cast<const volatile float*>(
        static_cast<std::uint8_t*>(weapon) + 0x2368);
    const float absVal = value < 0.0f ? -value : value;
    if (absVal < g_gaugeLogMinAbs.load(std::memory_order_relaxed)) {
        return;
    }
    static std::chrono::steady_clock::time_point windowStart{};
    static std::uint32_t windowCount = 0;
    static std::uint32_t dropped = 0;
    const auto now = std::chrono::steady_clock::now();
    if (now - windowStart > std::chrono::seconds(1)) {
        windowStart = now;
        windowCount = 0;
        if (dropped != 0) {
            std::ostringstream m;
            m << "GAUGEEVT: " << dropped << " call(s) dropped by rate cap";
            Log(m.str());
            dropped = 0;
        }
    }
    if (++windowCount > 40) {
        ++dropped;
        return;
    }
    std::uintptr_t callerRva = 0;
    if (returnAddress) {
        callerRva = reinterpret_cast<std::uintptr_t>(returnAddress) -
                    g_moduleBase.load(std::memory_order_relaxed);
    }
    std::ostringstream m;
    m << "GAUGEEVT +0x" << std::hex << callerRva << std::dec
      << " val=" << value << " gauge=" << before << "->" << after
      << " cost=" << costSlot << " own=" << own;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction) {
            m << " act=" << g_lastLongSwordAction->actionSet << ':'
              << g_lastLongSwordAction->actionId;
        }
    }
    Log(m.str());
}

void LoadGaugeRulesConfig() {
    g_gaugeRules.clear();
    std::ifstream input("nativePC/plugins/ValorGauge.gauge-rules.txt");
    std::string line;
    while (std::getline(input, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        std::istringstream ss(line);
        std::string kind;
        if (!(ss >> kind)) {
            continue;
        }
        GaugeRule rule;
        if (kind == "rewrite") {
            rule.type = GaugeRule::Type::Rewrite;
            if (!(ss >> rule.set >> rule.id >> rule.a)) {
                continue;
            }
            float orig = 0.0f;
            if (ss >> orig) {
                rule.b = orig;
                rule.hasOrig = true;
            }
        } else if (kind == "cost") {
            rule.type = GaugeRule::Type::Cost;
            if (!(ss >> rule.set >> rule.id >> rule.a)) {
                continue;
            }
        } else if (kind == "gate") {
            rule.type = GaugeRule::Type::Gate;
            if (!(ss >> rule.set >> rule.id >> rule.a >> rule.b)) {
                continue;
            }
        } else if (kind == "regen") {
            rule.type = GaugeRule::Type::Regen;
            if (!(ss >> rule.set >> rule.id >> rule.a)) {
                continue;
            }
            float variant = 0.0f;
            if (ss >> variant) {
                rule.b = variant;
            } else {
                rule.b = 0.0f;
            }
            float rate = 0.0f;
            if (ss >> rate) {
                rule.rate = rate;
                rule.hasRate = true;
            }
        } else if (kind == "option") {
            std::string key;
            float v = 0.0f;
            if (ss >> key >> v && key == "regen-freeze") {
                g_gaugeRegenFreeze.store(v != 0.0f,
                                         std::memory_order_relaxed);
            }
            continue;
        } else {
            continue;
        }
        g_gaugeRules.push_back(rule);
    }
    std::ostringstream m;
    m << "Gauge rules: " << g_gaugeRules.size() << " loaded"
      << (g_gaugeRules.empty() ? "" : " from ValorGauge.gauge-rules.txt");
    Log(m.str());
}

// Shared by the counter window and gate rules: extend the defense pool's
// no-hit window (pool = *(mgr+0x28), seconds float at pool+0xE4).
void InjectNoHitLocked(float seconds) {
    if (seconds <= 0.0f) {
        return;
    }
    std::uintptr_t pool = 0;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lockedPlayerController) {
            auto* player =
                static_cast<std::uint8_t*>(g_lockedPlayerController) - 0x61C8;
            if (IsReadable(player + 0x7D20, sizeof(void*))) {
                auto* mgr = *reinterpret_cast<std::uint8_t**>(player + 0x7D20);
                if (mgr && IsReadable(mgr + 0x28, sizeof(void*))) {
                    pool = reinterpret_cast<std::uintptr_t>(
                        *reinterpret_cast<void**>(mgr + 0x28));
                }
            }
        }
    }
    if (pool != 0) {
        auto* window = reinterpret_cast<float*>(pool + 0xE4);
        if (IsWritable(window, sizeof(float))) {
            *window = *window > seconds ? *window : seconds;
        }
    }
}

// Action-start rules (cost / gate), applied from DoActionHook after the
// original accepted a locked-player set-3 action.
void ApplyActionGaugeRules(std::int32_t actionSet, std::int32_t actionId) {
    if (g_gaugeRules.empty() || !g_lockedPlayerController || !g_originalGaugeAdd) {
        return;
    }
    auto* player =
        static_cast<std::uint8_t*>(g_lockedPlayerController) - 0x61C8;
    if (!IsReadable(player + 0x76B0, sizeof(void*))) {
        return;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
    if (!weapon || !IsReadable(weapon + 0x2374, sizeof(float))) {
        return;
    }
    auto readGauge = [&]() {
        return *reinterpret_cast<const volatile float*>(weapon + 0x2368);
    };
    if (const auto* rule = FindGaugeRule(GaugeRule::Type::Cost, actionSet, actionId)) {
        const float g0 = readGauge();
        g_originalGaugeAdd(weapon, -rule->a);
        std::ostringstream m;
        m << "GAUGERULE cost act=" << actionSet << ':' << actionId << " -"
          << rule->a << " gauge=" << g0 << "->" << readGauge();
        Log(m.str());
    }
    if (const auto* rule = FindGaugeRule(GaugeRule::Type::Gate, actionSet, actionId)) {
        const float g0 = readGauge();
        if (g0 >= rule->a) {
            g_originalGaugeAdd(weapon, -g0);
            InjectNoHitLocked(rule->b);
            std::ostringstream m;
            m << "GAUGERULE gate act=" << actionSet << ':' << actionId
              << " gauge=" << g0 << " >= " << rule->a << " -> no-hit "
              << rule->b << "s, gauge cleared";
            Log(m.str());
        }
    }
    if (const auto* rule = FindGaugeRule(GaugeRule::Type::Regen, actionSet, actionId)) {
        const bool iaiTimer = rule->b >= 0.5f;
        auto* timer = reinterpret_cast<volatile float*>(
            weapon + (iaiTimer ? 0x2388 : 0x2378));
        const float cur = *timer;
        if (cur < rule->a) {
            *timer = rule->a;
        }
        std::ostringstream m;
        m << "GAUGERULE regen act=" << actionSet << ':' << actionId << " "
          << (iaiTimer ? "iai(+0x2388)" : "helm(+0x2378)") << " "
          << cur << " -> " << (cur < rule->a ? rule->a : cur) << "s";
        // Optional rate override: the per-frame fill is dt / rateParam, so a
        // huge rate = glow without visible fill, small rate = fast fill.
        // Variant 0 writes the helm-breaker pair (+0x840/+0x844), variant 1
        // the iai pair (+0x850/+0x854) - both color variants at once.
        if (rule->hasRate && rule->rate > 0.0f) {
            if (IsReadable(player + 0x7810, sizeof(void*))) {
                auto* paramTable =
                    *reinterpret_cast<std::uint8_t**>(player + 0x7810);
                if (paramTable && IsWritable(paramTable + 0x844, sizeof(float))) {
                    auto* params = reinterpret_cast<volatile float*>(paramTable);
                    const float rate = rule->rate;
                    if (iaiTimer) {
                        params[0x850 / 4] = rate;
                        params[0x854 / 4] = rate;
                    } else {
                        params[0x840 / 4] = rate;
                        params[0x844 / 4] = rate;
                    }
                    m << " rate->" << rate;
                } else {
                    m << " rate-param not writable";
                }
            }
        }
        Log(m.str());
    }
}

// FUN_140ADD670 choke point: every player-emitter body effect (0x7D1-0x7DC
// family, 2000 parametric family, ...) passes here. Log local-player calls
// with the caller RVA; suppress exact repeats within 0.5s and hard-cap 30/s.
void __fastcall FxNotifyHook(void* emitter, std::uint32_t id,
                             std::uint32_t arg) {
    void* returnAddress = _ReturnAddress();
    bool own = false;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lockedPlayerController) {
            auto* player =
                static_cast<std::uint8_t*>(g_lockedPlayerController) - 0x61C8;
            if (IsReadable(player + 0x8808, sizeof(void*))) {
                own = emitter == *reinterpret_cast<void**>(player + 0x8808);
            }
        }
    }
    if (g_originalFxNotify) {
        g_originalFxNotify(emitter, id, arg);
    }
    if (!own) {
        return;
    }
    std::uintptr_t callerRva = 0;
    if (returnAddress) {
        callerRva = reinterpret_cast<std::uintptr_t>(returnAddress) -
                    g_moduleBase.load(std::memory_order_relaxed);
    }
    // Skip callers outside the game module (our own F7 test gun produces
    // garbage RVAs that would pollute the log).
    if (callerRva == 0 || callerRva > 0x4000000) {
        return;
    }
    static std::uint32_t lastId = 0;
    static std::uint32_t lastArg = 0;
    static std::uintptr_t lastCaller = 0;
    static std::chrono::steady_clock::time_point lastTime{};
    static std::chrono::steady_clock::time_point windowStart{};
    static std::uint32_t windowCount = 0;
    const auto now = std::chrono::steady_clock::now();
    if (now - windowStart > std::chrono::seconds(1)) {
        windowStart = now;
        windowCount = 0;
    }
    if (++windowCount > 30) {
        return;
    }
    if (id == lastId && arg == lastArg && callerRva == lastCaller &&
        now - lastTime < std::chrono::milliseconds(500)) {
        return;
    }
    lastId = id;
    lastArg = arg;
    lastCaller = callerRva;
    lastTime = now;
    std::ostringstream m;
    m << "FXNOTIFY id=0x" << std::hex << id << std::dec << " arg=" << arg
      << " caller=+0x" << std::hex << callerRva << std::dec;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction) {
            m << " act=" << g_lastLongSwordAction->actionSet << ':'
              << g_lastLongSwordAction->actionId;
        }
    }
    Log(m.str());
}

bool LockedPlayerActionChargesValor() {
    std::scoped_lock lock(g_actionMutex);
    if (!g_lastLongSwordAction) {
        return false;
    }
    return IsNormalModeChargingAction(*g_lastLongSwordAction);
}

void NotifyPlayerHitConnected(long long attacker) {
    void* controller = g_lockedPlayerController;
    if (!controller) {
        return;
    }
    const auto localPlayer =
        reinterpret_cast<long long>(static_cast<std::uint8_t*>(controller) - 0x61C8);
    const bool localAttacker = attacker == localPlayer;
    // First entries log both pointers: empirically confirms a1 is the ATTACKER
    // entity (dealt hits match, received hits carry the monster's pointer).
    static std::atomic_uint32_t hitAttLogs{0};
    if (hitAttLogs.load(std::memory_order_relaxed) < 8) {
        hitAttLogs.fetch_add(1, std::memory_order_relaxed);
        std::ostringstream m;
        m << "HITATT a1=0x" << std::hex << std::uppercase << attacker << " localPlayer=0x"
          << static_cast<std::uintptr_t>(localPlayer)
          << (localAttacker ? " MATCH" : " skip");
        Log(m.str());
    }
    if (!localAttacker) {
        // Another hunter's / monster's / follower's hit: count it, never charge.
        g_hitChargeSkips.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    const int percent = g_hitChargePercent.load(std::memory_order_relaxed);
    if (percent <= 0 || g_valorMode.load(std::memory_order_relaxed) || !ModWeaponActive()) {
        return;
    }
    if (!LockedPlayerActionChargesValor()) {
        return;
    }
    const auto oldCharge = g_valorCharge.load();
    const auto newCharge = static_cast<std::uint32_t>(
        std::min(100, static_cast<int>(oldCharge) + percent));
    if (newCharge == oldCharge) {
        return;
    }
    g_valorCharge = newCharge;
    static std::atomic_uint32_t hitChargeLogs{0};
    if (hitChargeLogs.load(std::memory_order_relaxed) < 64) {
        hitChargeLogs.fetch_add(1, std::memory_order_relaxed);
        std::ostringstream m;
        m << "HITCHARGE " << oldCharge << "% -> " << newCharge << "% (+" << percent << ") attacker=0x"
          << std::hex << std::uppercase << attacker;
        Log(m.str());
    }
    if (newCharge == 100) {
        ActivateValorMode(controller);
    }
}

long long __fastcall DmgCalcHook(long long a1, long long a2, std::uint32_t* a3) {
    NotifyPlayerHitConnected(a1);
    return g_originalDmgCalc(a1, a2, a3);
}
#endif

#ifdef VALOR_PROBE
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
    // Roll-mining: log EVERY damage-class dispatch during the roll. A type-6
    // dispatch with no damage = the hitbox overlapped the player but the
    // i-frame window ate it - the only place a successful through-roll could
    // be visible. Pure whiffs never reach the dispatcher as type 6.
    if (interactType == 6 && ActionIsRoll() && g_rollDispatchLog.load() < 400) {
        ++g_rollDispatchLog;
        std::ostringstream m;
        m << "ROLLDISPATCH type=6" << std::hex << std::uppercase << " code=0x"
          << code << " dmg=" << (g_damageThisDispatch ? 1 : 0);
        Log(m.str());
    }
    const bool attackRejected = interactType == 6 && code != 0xD &&
                                !g_damageThisDispatch;
    const bool rollRejected = attackRejected && ActionIsRoll();
    if ((attackRejected && ActionIsForesightSlash()) || rollRejected) {
        // Superseded trigger path (bit15/bit250 own the mod triggers now):
        // probe keeps observation only - latch + capped log.
        if (!g_clangLatch) {
            g_clangLatch = true;
        }
        if (g_fsdLogCount.load() < 4096) {
            ++g_fsdLogCount;
            const auto clangN = g_clangCount.fetch_add(1) + 1;
            if (clangN <= 16) {
                std::ostringstream m;
                m << "HITDISPATCH #" << std::dec << clangN << ": rejected during "
                  << (rollRejected ? "roll(3:18)" : "见切")
                  << std::hex << std::uppercase << " (type=" << interactType
                  << " code=0x" << code << ")";
                Log(m.str());
            }
        }
    }
    return result;
}
#endif // VALOR_PROBE (hit dispatch + damage calc diagnostics)

#ifndef VALOR_PROBE
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
    if (isForesightSuccess && result != 0 && ModWeaponActive()) {
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
    void* target = reinterpret_cast<void*>(g_hookAddr[HkCondEval]);
    if (MH_CreateHook(target, reinterpret_cast<void*>(&CondEvalHook),
                      reinterpret_cast<void**>(&g_originalCondEval)) != MH_OK ||
        MH_EnableHook(target) != MH_OK) {
        Log("Condition evaluator hook failed");
        return false;
    }
    Log("Condition evaluator hook installed");
    return true;
}
#endif // !VALOR_PROBE (condition evaluator = mod trigger path)

// --- Wwise sound-event logging (叮 source hunt) --------------------------------
#ifdef VALOR_PROBE
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
    auto* singletonPtr = reinterpret_cast<std::uint8_t**>(g_addrAudioSingleton);
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

#endif // VALOR_PROBE (wwise diagnostics)

bool InstallPoolQueryHook() {
    const HMODULE game = GetModuleHandleW(nullptr);
    MODULEINFO module{};
    if (!game || !GetModuleInformation(GetCurrentProcess(), game, &module, sizeof(module))) {
        Log("Hook install failed: module info unavailable");
        return false;
    }
    g_moduleBase = reinterpret_cast<std::uintptr_t>(module.lpBaseOfDll);
    InitMemoryMap();
#ifdef VALOR_PROBE
    void* target = reinterpret_cast<void*>(g_hookAddr[HkPoolQuery]);
    if (MH_CreateHook(target, reinterpret_cast<void*>(&Aee050Hook),
                      reinterpret_cast<void**>(&g_originalAee050)) != MH_OK) {
        Log("Pool query hook failed: MH_CreateHook");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        Log("Pool query hook failed: MH_EnableHook");
        return false;
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkDmgCalc]),
                      reinterpret_cast<void*>(&DmgCalcHook),
                      reinterpret_cast<void**>(&g_originalDmgCalc)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkDmgCalc])) != MH_OK) {
        Log("Damage calc hook failed");
    } else {
        Log("Damage calc hook installed");
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkHitDispatch]),
                      reinterpret_cast<void*>(&HitDispatchHook),
                      reinterpret_cast<void**>(&g_originalHitDispatch)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkHitDispatch])) != MH_OK) {
        Log("Hit dispatcher hook failed");
    } else {
        Log("Hit dispatcher hook installed");
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkPostEventId]),
                      reinterpret_cast<void*>(&PostEventIdHook),
                      reinterpret_cast<void**>(&g_originalPostEventId)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkPostEventId])) != MH_OK) {
        Log("Wwise PostEvent(id) hook failed");
    } else {
        Log("Wwise PostEvent(id) hook installed");
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkSoundPost]),
                      reinterpret_cast<void*>(&SoundPostHook),
                      reinterpret_cast<void**>(&g_originalSoundPost)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkSoundPost])) != MH_OK) {
        Log("Sound post hook failed");
    } else {
        Log("Sound post hook installed");
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkPostEventName]),
                      reinterpret_cast<void*>(&PostEventNameHook),
                      reinterpret_cast<void**>(&g_originalPostEventName)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkPostEventName])) != MH_OK) {
        Log("Wwise PostEvent(name) hook failed");
    } else {
        Log("Wwise PostEvent(name) hook installed");
    }
    std::ostringstream m;
    m << "Pool query hook installed at 0x" << std::hex << std::uppercase
      << reinterpret_cast<std::uintptr_t>(target);
    Log(m.str());
#else
    InstallCondEvalHook();
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkDmgCalc]),
                      reinterpret_cast<void*>(&DmgCalcHook),
                      reinterpret_cast<void**>(&g_originalDmgCalc)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkDmgCalc])) != MH_OK) {
        Log("Damage calc hook failed (hit charging inactive)");
    } else {
        Log("Damage calc hook installed (hit-based valor charging)");
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkGaugeAdd]),
                      reinterpret_cast<void*>(&GaugeAddHook),
                      reinterpret_cast<void**>(&g_originalGaugeAdd)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkGaugeAdd])) != MH_OK) {
        Log("Gauge add hook failed");
    } else {
        Log("Gauge add hook installed (GAUGEEVT logging)");
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkFxNotify]),
                      reinterpret_cast<void*>(&FxNotifyHook),
                      reinterpret_cast<void**>(&g_originalFxNotify)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkFxNotify])) != MH_OK) {
        Log("FX notify hook failed");
    } else {
        Log("FX notify hook installed (FXNOTIFY logging)");
    }
#endif
    ValidateMemoryMap();
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

#ifdef VALOR_PROBE
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
#endif // VALOR_PROBE (prop call histogram)

#ifndef VALOR_PROBE
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
    if (!ModWeaponActive()) {
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

void LoadFxTestConfig() {
    std::ifstream input("nativePC/plugins/ValorGauge.fx-test.txt");
    std::string funcToken;
    std::string idToken;
    if (input >> funcToken >> idToken) {
        g_fxCfgFunc = std::strtol(funcToken.c_str(), nullptr, 0);
        g_fxCfgId =
            static_cast<std::uint32_t>(std::strtoul(idToken.c_str(), nullptr, 0));
    }
    std::ostringstream m;
    m << "FX test config: func=" << g_fxCfgFunc << " id=0x" << std::hex
      << g_fxCfgId << std::dec << " (F7 fires, Shift+F7 reloads)";
    Log(m.str());
}

void TryQueuedFx(const void* entity) {
    const int func = g_fxFireFunc.exchange(-1, std::memory_order_acq_rel);
    if (func < 0 || !g_lockedPlayerController) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(g_lockedPlayerController) - 0x61C8;
    if (entity != player || !IsReadable(player + 0x8808, sizeof(void*))) {
        return;
    }
    void* emitter = *reinterpret_cast<void**>(player + 0x8808);
    const auto id = g_fxFireId.load(std::memory_order_relaxed);
    std::ostringstream m;
    m << "FXTEST: fire func=" << func << " id=0x" << std::hex << id;
    if (func == 0 && g_fxNotifyA) {
        g_fxNotifyA(emitter, id, 0);
        m << " via ADD670";
    } else if (func == 1 && g_fxNotifyB) {
        g_fxNotifyB(emitter, id, 0);
        m << " via AE0D70";
    } else {
        m << " <unresolved func>";
    }
    Log(m.str());
}

int __fastcall PropIntHook(void* entity, void* id, std::uint32_t index, int defaultValue) {
    TryQueuedForceWrite(entity);
    TryQueuedFx(entity);
    const int result = g_originalPropInt(entity, id, index, defaultValue);
#ifdef VALOR_PROBE
    RecordPropCall(entity, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(id)),
                   index, 0, _ReturnAddress(), result);
#endif
    return result;
}

// The force path rides the property-int getter (game thread, every frame).
// The probe build installs this via the F11 histogram; the mod build needs it
// standalone.
bool InstallForceHook() {
    const HMODULE game = GetModuleHandleW(nullptr);
    MODULEINFO module{};
    if (!game || !GetModuleInformation(GetCurrentProcess(), game, &module, sizeof(module))) {
        Log("Force hook failed: module info unavailable");
        return false;
    }
    void* target = reinterpret_cast<void*>(g_hookAddr[HkPropInt]);
    if (MH_CreateHook(target, reinterpret_cast<void*>(&PropIntHook),
                      reinterpret_cast<void**>(&g_originalPropInt)) != MH_OK ||
        MH_EnableHook(target) != MH_OK) {
        Log("Force hook (prop int getter) failed");
        return false;
    }
    Log("Force hook (prop int getter) installed");
    return true;
}
#endif // !VALOR_PROBE (force path via prop getter)

#ifdef VALOR_PROBE
// Probe variant of the int getter: histogram only, no force path.
int __fastcall PropIntHook(void* entity, void* id, std::uint32_t index, int defaultValue) {
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
    const auto nameFn = reinterpret_cast<PropNameFn>(g_addrPropName);
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
    auto* table = reinterpret_cast<std::uint8_t*>(g_addrPropRegistry);
    if (!IsReadable(table, 0xC0)) {
        Log("REGISTRY unavailable: table unreadable");
        return;
    }
    const auto count = *reinterpret_cast<const std::int32_t*>(table + 0xB8);
    std::ostringstream header;
    header << "REGISTRY BEGIN count=" << std::dec << count;
    Log(header.str());
    if (count > 0) {
        const auto lookup = reinterpret_cast<PropRegistryLookupFn>(g_addrPropLookup);
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
        {0, reinterpret_cast<void*>(&PropIntHook),
         reinterpret_cast<void**>(&g_originalPropInt), "int"},
        {0, reinterpret_cast<void*>(&PropBoolHook),
         reinterpret_cast<void**>(&g_originalPropBool), "bool"},
        {0, reinterpret_cast<void*>(&PropFloatHook),
         reinterpret_cast<void**>(&g_originalPropFloat), "float"},
    };
    const std::uintptr_t hookAddrs[3] = {g_hookAddr[HkPropInt], g_hookAddr[HkPropBool],
                                         g_hookAddr[HkPropFloat]};
    bool allOk = true;
    for (int i = 0; i < 3; ++i) {
        const auto& entry = entries[i];
        void* target = reinterpret_cast<void*>(hookAddrs[i]);
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
#endif // VALOR_PROBE (property histogram + F11)

#ifndef VALOR_PROBE
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
    std::ifstream autoCfg("nativePC/plugins/ValorGauge.counter-auto.txt");
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
    LoadModWeaponTypeConfig();
    LoadHitChargeConfig();
    LoadGaugeLockConfig();
    LoadGaugeLogConfig();
    LoadGaugeRulesConfig();
    LoadSheathRewriteConfig();
    std::ostringstream wt;
    wt << "Mod weapon gate: only weapon type "
       << g_modWeaponType.load(std::memory_order_relaxed)
       << " (Long Sword) activates the gauge/counter (ValorGauge.weapon-type.txt)";
    Log(wt.str());
}

void OpenCounterWindow(const char* reason) {
    g_counterWindowUntil = std::chrono::steady_clock::now() +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(g_counterWindowSec));
    // Pool pointer derived straight from the player (mgr = *(player+0x7D20),
    // pool = mgr+0x28) - the mod build installs no pool-query hook.
    std::uintptr_t pool = 0;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lockedPlayerController) {
            auto* player = static_cast<std::uint8_t*>(g_lockedPlayerController) - 0x61C8;
            if (IsReadable(player + 0x7D20, sizeof(void*))) {
                auto* mgr = *reinterpret_cast<std::uint8_t**>(player + 0x7D20);
                if (mgr && IsReadable(mgr + 0x28, sizeof(void*))) {
                    pool = reinterpret_cast<std::uintptr_t>(
                        *reinterpret_cast<void**>(mgr + 0x28));
                }
            }
        }
    }
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
#endif // !VALOR_PROBE (counter window / force config)

#ifdef VALOR_PROBE
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
    auto* registry = reinterpret_cast<std::uint8_t*>(g_addrPropRegistry);
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

// Full property-key table dump (ids 0..799): run once when the controller
// locks. Diffing the dump before vs after adding an XFsm condition reveals
// which property id that condition bound (its key goes 0 -> nonzero).
void LogKeyTableDump(std::uint8_t* player) {
    auto* componentSlot = player + 0x126D8;
    if (!IsReadable(componentSlot, sizeof(void*))) {
        Log("KEYDUMP: component slot unreadable");
        return;
    }
    auto* comp = *reinterpret_cast<std::uint8_t**>(componentSlot);
    if (!comp || g_moduleBase.load() == 0) {
        Log("KEYDUMP: component unavailable");
        return;
    }
    auto* registry = reinterpret_cast<std::uint8_t*>(g_addrPropRegistry);
    if (!IsReadable(registry, 0xD0)) {
        Log("KEYDUMP: registry unreadable");
        return;
    }
    Log("KEYDUMP BEGIN (key table, ids 0..799; format id=key)");
    for (std::uint32_t base = 0; base < 800; base += 40) {
        std::ostringstream m;
        m << "KEYDUMP";
        for (std::uint32_t id = base; id < base + 40 && id < 800; ++id) {
            auto* keyAddress = comp + 0x84 + static_cast<std::size_t>(id) * 24;
            int key = -1;
            if (IsReadable(keyAddress, sizeof(std::int32_t))) {
                key = *reinterpret_cast<const std::int32_t*>(keyAddress);
            }
            m << ' ' << id << '=' << key;
        }
        Log(m.str());
    }
    Log("KEYDUMP END");
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
    auto* registry = reinterpret_cast<std::uint8_t*>(g_addrPropRegistry);
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
#endif // VALOR_PROBE (RXN watcher prop resolution + chain diagnostics)

#ifndef VALOR_PROBE
// --- Mod per-frame trigger poll (gauge/counter build only) -------------------
// Three independent clang triggers, any of which opens the counter window,
// injects the no-hit coverage and (auto-force on) queues the 刚1 interrupt:
//   1. iai negation     - event bit 250 (player+0x8954 bit 26) during 101/102
//   2. foresight latch  - event bit 15 (player+0x8938) rising during 3:86
//   3. deflect flash    - weapon+0x23A0 crossing 0.5 upward during 3:86
// Everything is gated by ModWeaponActive(): other weapons never trigger.
void PollModTriggers() {
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
        return;
    }
    if (!ModWeaponActive()) {
        // Still track the latch so re-drawing the Long Sword does not see a
        // stale rising edge.
        g_fsuccBitPrev = false;
        g_negBitPrev = false;
        g_flashPrev = 0.0f;
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

    // Semantic sanity probes: if a game update shifts the struct layout these
    // invariants break first - log once and point at the offending offset
    // (recovery procedure in Reverse/ADDRESS_RECOVERY.md).
    static bool warnedSpirit = false;
    static bool warnedStandby = false;
    static bool warnedNohit = false;
    if (mgr && IsReadable(mgr, 0x7D0)) {
        if (!warnedStandby && standby > 1) {
            warnedStandby = true;
            Log("MEMORYMAP SANITY: mgr+0x7C0 standby > 1 - offset may have shifted");
        }
    }
    if (!warnedNohit && IsReadable(mgr + 0x28 + 0xE4, sizeof(float)) && nohit < -1.0f) {
        warnedNohit = true;
        Log("MEMORYMAP SANITY: pool+0xE4 no-hit < -1 - offset may have shifted");
    }
    if (!warnedSpirit) {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction && IsReadable(player + 0x76B0, sizeof(void*))) {
            auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
            if (weapon && IsReadable(weapon + 0x2370, sizeof(std::int32_t))) {
                const auto spirit =
                    *reinterpret_cast<const std::int32_t*>(weapon + 0x2370);
                if (spirit < 0 || spirit > 3) {
                    warnedSpirit = true;
                    Log("MEMORYMAP SANITY: weapon+0x2370 spirit level outside "
                        "[0,3] - offset may have shifted");
                }
            }
        }
    }

    bool inEvasionNow = false;
    {
        std::scoped_lock lock(g_actionMutex);
        inEvasionNow = g_lastLongSwordAction &&
                       g_lastLongSwordAction->actionSet == 3 &&
                       (g_lastLongSwordAction->actionId == 86 ||
                        g_lastLongSwordAction->actionId == 101 ||
                        g_lastLongSwordAction->actionId == 102);
    }

    // 1) IAI negation bit 250: player+0x8954 bit 26 (mined: clang dword
    //    0x1C000000 vs whiff 0x18000000/0x18400000; self-clears).
    if (inEvasionNow) {
        auto* negBitAddr = reinterpret_cast<std::uint32_t*>(player + 0x8954);
        if (IsReadable(negBitAddr, sizeof(std::uint32_t))) {
            const bool negBit = (*negBitAddr & 0x04000000u) != 0;
            if (negBit && !g_negBitPrev) {
                g_clangLatch = true;
                OpenCounterWindow("negation bit 250 set (player+0x8954)");
                if (g_counterAutoForce && !g_forceArmed.exchange(true)) {
                    Log("CLANG: negation bit set - auto-force armed");
                }
            }
            g_negBitPrev = negBit;
        }
    } else {
        g_negBitPrev = false;
    }

    // 2) Deflect flash: weapon+0x23A0 crosses 0.5 upward during 3:86
    //    (mined 4/4 clang vs 0/5 whiff).
    if (ActionIsForesightSlash()) {
        auto* weaponSlot = player + 0x76B0;
        if (IsReadable(weaponSlot, sizeof(void*))) {
            auto* weapon = *reinterpret_cast<std::uint8_t**>(weaponSlot);
            if (weapon && IsReadable(weapon + 0x23A0, sizeof(float))) {
                const float flash =
                    *reinterpret_cast<const float*>(weapon + 0x23A0);
                if (flash > 0.5f && g_flashPrev <= 0.5f) {
                    g_clangLatch = true;
                    OpenCounterWindow("deflect flash (weapon+0x23A0)");
                    if (g_counterAutoForce && !g_forceArmed.exchange(true)) {
                        Log("CLANG: deflect flash detected - auto-force armed");
                    }
                }
                g_flashPrev = flash;
            }
        }
    } else {
        g_flashPrev = 0.0f;
    }

    // 3) Vanilla foresight success latch: event bit 15 (player+0x8938) -
    //    the input remapper FUN_14118C320 picks follow-up 763 vs 764 with it.
    {
        std::uint32_t ev0 = 0;
        if (IsReadable(player + 0x8938, sizeof(std::uint32_t))) {
            ev0 = *reinterpret_cast<const std::uint32_t*>(player + 0x8938);
        }
        std::uint8_t m660 = 0, m661 = 0, m662 = 0;
        if (mgr && IsReadable(mgr + 0x660, 3)) {
            m660 = mgr[0x660];
            m661 = mgr[0x661];
            m662 = mgr[0x662];
        }
        const bool bit15 = (ev0 & 0x8000u) != 0;
        const bool in86 = ActionIsForesightSlash();
        static std::uint8_t pm660 = 0, pm661 = 0, pm662 = 0;
        static std::uint32_t pev0 = 0;
        if (bit15 != g_fsuccBitPrev || m660 != pm660 || m661 != pm661 ||
            m662 != pm662 || (in86 && ev0 != pev0)) {
            if (g_vseekLogCount.load() < 240) {
                ++g_vseekLogCount;
                std::ostringstream m;
                m << "VSEEK bit15=" << bit15 << std::hex << std::uppercase
                  << " ev0=0x" << ev0 << std::dec
                  << " m660=" << static_cast<int>(m660)
                  << " m661=" << static_cast<int>(m661)
                  << " m662=" << static_cast<int>(m662);
                {
                    std::scoped_lock lock(g_actionMutex);
                    if (g_lastLongSwordAction) {
                        m << " act=" << g_lastLongSwordAction->actionSet << ':'
                          << g_lastLongSwordAction->actionId;
                    }
                }
                Log(m.str());
            }
            pm660 = m660;
            pm661 = m661;
            pm662 = m662;
            pev0 = ev0;
        }
        if (in86 && bit15 && !g_fsuccBitPrev) {
            g_clangLatch = true;
            OpenCounterWindow("foresight success bit15 (player+0x8938)");
            if (g_counterAutoForce && !g_forceArmed.exchange(true)) {
                Log("CLANG: vanilla foresight success latch - auto-force armed");
            }
        }
        g_fsuccBitPrev = bit15;
    }
}
#endif // !VALOR_PROBE

#ifdef VALOR_PROBE
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

    // Mined-candidate probes: active only during special evasions. These
    // offsets flipped in clang runs but never in whiff runs (F5 miner, 9 runs).
    static std::uint32_t lpW2398 = 0, lpW23A0 = 0, lpB220 = 0, lpB224 = 0,
                         lpB228 = 0, lpB240 = 0, lpB24C = 0, lpB260 = 0,
                         lpB264 = 0, lpB268 = 0, lp8954 = 0, lp8984 = 0,
                         lp89B4 = 0, lp89E4 = 0;
    bool inEvasionNow = false;
    {
        std::scoped_lock lock(g_actionMutex);
        inEvasionNow = g_lastLongSwordAction &&
                       g_lastLongSwordAction->actionSet == 3 &&
                       (g_lastLongSwordAction->actionId == 86 ||
                        g_lastLongSwordAction->actionId == 101 ||
                        g_lastLongSwordAction->actionId == 102 ||
                        g_lastLongSwordAction->actionId == 18);
    }
    if (inEvasionNow && g_clangProbeLogCount.load() < 600) {
        auto* weaponSlot = player + 0x76B0;
        std::uint8_t* weapon = nullptr;
        if (IsReadable(weaponSlot, sizeof(void*))) {
            weapon = *reinterpret_cast<std::uint8_t**>(weaponSlot);
        }
        struct MProbe {
            std::uint8_t* addr;
            std::uint32_t* last;
            const char* tag;
        };
        MProbe mprobes[] = {
            {weapon ? weapon + 0x2398 : nullptr, &lpW2398, "w2398"},
            {weapon ? weapon + 0x23A0 : nullptr, &lpW23A0, "w23A0"},
            {player + 0xB220, &lpB220, "pB220"},
            {player + 0xB224, &lpB224, "pB224"},
            {player + 0xB228, &lpB228, "pB228"},
            {player + 0xB240, &lpB240, "pB240"},
            {player + 0xB24C, &lpB24C, "pB24C"},
            {player + 0xB260, &lpB260, "pB260"},
            {player + 0xB264, &lpB264, "pB264"},
            {player + 0xB268, &lpB268, "pB268"},
            {player + 0x8954, &lp8954, "ev8954"},
            {player + 0x8984, &lp8984, "ev8984"},
            {player + 0x89B4, &lp89B4, "ev89B4"},
            {player + 0x89E4, &lp89E4, "ev89E4"},
        };
        std::ostringstream changed;
        for (auto& mp : mprobes) {
            if (!mp.addr || !IsReadable(mp.addr, sizeof(std::uint32_t))) {
                continue;
            }
            const auto v = *reinterpret_cast<const std::uint32_t*>(mp.addr);
            if (v != *mp.last) {
                changed << ' ' << mp.tag << '=' << v;
                *mp.last = v;
            }
        }
        if (!changed.str().empty()) {
            ++g_clangProbeLogCount;
            Log("EPROBE" + changed.str());
        }
    }
    // (iai bit250 / deflect-flash triggers live in the mod build's
    //  PollModTriggers; the probe only observes the raw values above.)

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
#endif // VALOR_PROBE (RXN watcher)

#ifdef VALOR_PROBE
// Optional nativePC\plugins\ValorGauge.prop-watch.txt: property ids to watch,
// up to 6, e.g. "222 631".
void LoadPropWatchConfig() {
    std::ifstream trigger("nativePC/plugins/ValorGauge.prop-trigger.txt");
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
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (!g_minerActive) {
        g_minerActive = true;
        g_minerPhase = 1;
        Log("MINER auto-started: recording 见切/居合/翻滚(3:18) as runs. "
            "F5 marks a new phase + dumps a snapshot.");
        static bool keyDumped = false;
        if (!keyDumped) {
            keyDumped = true;
            LogKeyTableDump(player);
        }
    }

    bool inEvasion = false;
    {
        std::scoped_lock lock(g_actionMutex);
        inEvasion = g_lastLongSwordAction && g_lastLongSwordAction->actionSet == 3 &&
               (g_lastLongSwordAction->actionId == 86 ||
                g_lastLongSwordAction->actionId == 101 ||
                g_lastLongSwordAction->actionId == 102 ||
                g_lastLongSwordAction->actionId == 18);
    }
    if (inEvasion && !g_minerInRun) {
        MinerBeginRun(player);
    }
    if (!inEvasion && g_minerInRun) {
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
#endif // VALOR_PROBE (prop watch/write + F5 miner)

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
    g_addrDoAction = reinterpret_cast<std::uintptr_t>(target);
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

#ifdef VALOR_PROBE
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
#endif // VALOR_PROBE (state logger + F10 diff capture)

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

// Gauge-lock experiment: runs on the Present/render thread AFTER the game's
// frame update, so values read here are whatever the game left in the fields
// this frame. Drift from our last write = the game wrote underneath (its own
// drain, a level-change refill, ...). Both fields are always watched; only
// the configured target is written. Reported every 2s.
#ifndef VALOR_PROBE
void PollGaugeLock() {
    const float value = g_gaugeLockValue.load(std::memory_order_relaxed);
    const bool enabled = g_gaugeLockEnabled.load(std::memory_order_relaxed);
    if (value <= 0.0f || value > 1.0f) {
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
    if (!ModWeaponActive()) {
        g_gaugeLockLastWritten[0] = -1.0f;
        g_gaugeLockLastWritten[1] = -1.0f;
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (!IsReadable(player + 0x76B0, sizeof(void*))) {
        return;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
    if (!weapon || !IsReadable(weapon + 0x2374, sizeof(float))) {
        return;
    }
    constexpr std::uintptr_t kGaugeFields[2] = {0x2374, 0x2368};
    constexpr float kEpsilon = 1.0e-6f;
    for (int i = 0; i < 2; ++i) {
        auto* slot =
            reinterpret_cast<volatile float*>(weapon + kGaugeFields[i]);
        const float current = *slot;
        if (g_gaugeLockLastWritten[i] >= 0.0f &&
            (current > g_gaugeLockLastWritten[i] + kEpsilon ||
             current < g_gaugeLockLastWritten[i] - kEpsilon)) {
            ++g_gaugeWatchFrames[i];
            g_gaugeWatchDrift[i] = current;
        }
        if (enabled && g_gaugeLockTarget.load(std::memory_order_relaxed) ==
                           i + 1) {
            *slot = value;
            g_gaugeLockLastWritten[i] = value;
        } else {
            g_gaugeLockLastWritten[i] = current;
        }
    }

    const auto now = std::chrono::steady_clock::now();
    if (now >= g_gaugeLockNextReport) {
        g_gaugeLockNextReport = now + std::chrono::seconds(2);
        std::ostringstream m;
        m << "GAUGELOCK: lock=0x"
          << (g_gaugeLockTarget.load(std::memory_order_relaxed) == 2 ? "2368"
                                                                     : "2374")
          << "@" << value << (enabled ? " ON" : " OFF")
          << " | 0x2374 raw=" << g_gaugeWatchDrift[0]
          << " drift=" << g_gaugeWatchFrames[0] << "/2s"
          << " | 0x2368 raw=" << g_gaugeWatchDrift[1]
          << " drift=" << g_gaugeWatchFrames[1] << "/2s";
        Log(m.str());
        g_gaugeWatchFrames[0] = 0;
        g_gaugeWatchFrames[1] = 0;
    }
}
#endif // !VALOR_PROBE (gauge-lock experiment = mod build only)

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

#ifdef VALOR_PROBE
    const bool f7Down = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
    if (f7Down && !g_f7WasDown) {
        LoadStateAddress();
        g_stateLoggerEnabled = true;
        Log("State logger reloaded with F7");
    }
    g_f7WasDown = f7Down;
    PollStateLogger();
    PollDiffCapture();
    PollPropHistogram();
    PollReactionWatch();
    MinerCaptureFrame();
    const bool f5Down = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
    if (f5Down && !g_f9WasDown) {
        MinerHandleF9();
    }
    g_f9WasDown = f5Down;
    const bool f6Down = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    if (f6Down && !g_f6WasDown) {
        TryPropertyWrite();
    }
    g_f6WasDown = f6Down;
#else
    PollValorLifecycle();
    PollModTriggers();
    PollGaugeLock();

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
    if (f6Down && !g_f6WasDown && ModWeaponActive()) {
        if (!g_forceArmed.exchange(true)) {
            std::ostringstream m;
            m << "FORCE armed: the next Long Sword transition executes 3:"
              << g_forceId;
            Log(m.str());
        }
    }
    g_f6WasDown = f6Down;

    // Gauge-lock experiment toggle (F5). Only meaningful when a lock value is
    // configured in ValorGauge.gauge-lock.txt.
    const bool f5Down = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
    if (f5Down && !g_gaugeLockKeyWasDown &&
        g_gaugeLockValue.load(std::memory_order_relaxed) > 0.0f) {
        const bool newEnabled =
            !g_gaugeLockEnabled.load(std::memory_order_relaxed);
        g_gaugeLockEnabled.store(newEnabled, std::memory_order_relaxed);
        std::ostringstream m;
        m << "GAUGELOCK " << (newEnabled ? "ON" : "OFF")
          << " (F5): target weapon+0x"
          << (g_gaugeLockTarget.load(std::memory_order_relaxed) == 2 ? "2368"
                                                                     : "2374")
          << " <- " << g_gaugeLockValue.load(std::memory_order_relaxed)
          << " per frame";
        Log(m.str());
    }
    g_gaugeLockKeyWasDown = f5Down;

    // FX test trigger: F7 fires the configured player-emitter notify once;
    // Shift+F7 reloads ValorGauge.fx-test.txt (sweep candidate ids).
    const bool f7Down = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
    if (f7Down && !g_fxKeyWasDown) {
        if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) {
            LoadFxTestConfig();
        } else {
            g_fxFireFunc.store(g_fxCfgFunc, std::memory_order_release);
            g_fxFireId.store(g_fxCfgId, std::memory_order_relaxed);
            std::ostringstream m;
            m << "FXTEST: queued func=" << g_fxCfgFunc << " id=0x" << std::hex
              << g_fxCfgId;
            Log(m.str());
        }
    }
    g_fxKeyWasDown = f7Down;
#endif

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
#ifdef VALOR_PROBE
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
#else
        if (ModWeaponActive()) {
            draw->AddText(ImGui::GetFont(), 22.0f, ImVec2(0.0f, 0.0f),
                          IM_COL32(255, 55, 55, 255), actionText);
            if (windowOpen) {
                draw->AddText(ImGui::GetFont(), 18.0f, ImVec2(0.0f, 70.0f),
                              IM_COL32(80, 220, 255, 255), "COUNTER WINDOW");
            }
            if (g_forceArmed.load()) {
                draw->AddText(ImGui::GetFont(), 18.0f, ImVec2(0.0f, 92.0f),
                              IM_COL32(255, 220, 80, 255), "FORCE ARMED");
            }
            const float lockValue =
                g_gaugeLockValue.load(std::memory_order_relaxed);
            if (lockValue > 0.0f) {
                char lockText[112]{};
                snprintf(lockText, sizeof(lockText),
                         "GLOCK t=0x%s %.2f %s | 74:%.4f x%u 68:%.4f x%u",
                         g_gaugeLockTarget.load(std::memory_order_relaxed) == 2
                             ? "2368"
                             : "2374",
                         lockValue,
                         g_gaugeLockEnabled.load(std::memory_order_relaxed)
                             ? "ON"
                             : "OFF",
                         g_gaugeWatchDrift[0], g_gaugeWatchFrames[0],
                         g_gaugeWatchDrift[1], g_gaugeWatchFrames[1]);
                draw->AddText(ImGui::GetFont(), 18.0f, ImVec2(0.0f, 114.0f),
                              g_gaugeLockEnabled.load(std::memory_order_relaxed)
                                  ? IM_COL32(80, 255, 120, 255)
                                  : IM_COL32(170, 170, 170, 200),
                              lockText);
            }
        }
#endif
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
    // Never co-install: both builds hook the same game functions from their
    // own MinHook instances - the second hook on already-patched bytes would
    // corrupt the trampoline. Stand down if the sibling DLL is present.
    if (GetModuleHandleW(
#ifdef VALOR_PROBE
            L"ValorGauge.dll"
#else
            L"ValorProbe.dll"
#endif
        ) != nullptr) {
        Log("Sibling Valor DLL detected - standing down (only one build may be installed)");
        return 0;
    }
#ifdef VALOR_PROBE
    Log("ValorProbe loading");
#else
    Log("ValorGauge (mod) loading");
#endif
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
#ifdef VALOR_PROBE
                InstallPropHistogramHooks();
                LoadPropWriteConfig();
                LoadPropWatchConfig();
#else
                InstallForceHook();
                LoadForceActionConfig();
                {
                    const std::uintptr_t base = ModuleBaseAddr();
                    const std::uintptr_t rvaA =
                        OverrideOrZero("fxNotifyA") != 0
                            ? OverrideOrZero("fxNotifyA")
                            : 0xADD670;
                    const std::uintptr_t rvaB =
                        OverrideOrZero("fxNotifyB") != 0
                            ? OverrideOrZero("fxNotifyB")
                            : 0xAE0D70;
                    g_fxNotifyA = reinterpret_cast<void (__fastcall*)(
                        void*, std::uint32_t, std::uint32_t)>(base + rvaA);
                    g_fxNotifyB = reinterpret_cast<void (__fastcall*)(
                        void*, std::uint32_t, std::uint32_t)>(base + rvaB);
                    std::ostringstream fxm;
                    fxm << "FX test notify resolved: ADD670="
                        << (g_fxNotifyA ? "ok" : "null") << " AE0D70="
                        << (g_fxNotifyB ? "ok" : "null");
                    Log(fxm.str());
                }
                LoadFxTestConfig();
#endif
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
