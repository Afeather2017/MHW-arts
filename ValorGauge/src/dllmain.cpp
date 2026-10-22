#include <Windows.h>
#include <mmsystem.h>
#include <d3d11.h>
#include <dxgi.h>
#include <Psapi.h>

#include <atomic>
#include <algorithm>
#include <bit>
#include <chrono>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <intrin.h>
#include <mutex>
#include <map>
#include <memory>
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
// FUN_142122410 returns the clamped gauge in xmm0 - the type must say float
// so the detour forwards it (return-value clobbering = caller eats garbage).
float (*g_originalGaugeAdd)(void* weapon, float value) = nullptr;
std::atomic<float> g_gaugeLogMinAbs{0.01f};
// Player-emitter notify logger (FUN_140ADD670): every body-effect id the game
// fires on the local player is logged with its caller RVA, so one session
// (iai, foresight, helm breaker...) maps which id = which aura.
// FUN_140ADD670 RETURNS the created effect handle in rax ("mov rax,rbx; ret")
// - callers may keep using it (GS charge glow does; discarding it crashed the
// game at MonsterHunterWorld.exe+0x232E245), so the detour must forward it.
void* (*g_originalFxNotify)(void* emitter, std::uint32_t id,
                            std::uint32_t arg) = nullptr;
// FX test (2026-09-24): fire a player-emitter notify directly to identify the
// spirit-slash body glow. Candidates seen in the per-move handlers:
//   0x7D2 / 0x7DC via func_140AE0D70 (gated: action 87/102 && level!=0, or +0x2398)
//   0x7D7         via FUN_140ADD670   (gated: +0x2398 stance flag)
// Config nativePC/plugins/ValorGauge.fx-test.txt = "<func> <id>" (func 0 =
// ADD670, 1 = AE0D70; id accepts 0x prefix). F7 fires once, Shift+F7 reloads.
// 2026-10-21 red-glow chain test funcs (RVA 0x21224D0 = FUN_1421224d0, the
// generic emitter; weapon+0x2398 = "spirit gauge cost paid" arm flag):
//   2 = call native emitter (fires only if +0x2398 armed, or action 87/102
//       with level != 0 — the native gates run as-is)
//   3 = write weapon+0x2398 = 1 (arm; next native emitter call glows)
//   4 = arm + call native emitter (decisive: manual glow bypassing cost/bit0)
//   5 = write weapon+0x2398 = 0 (clear)
// <id> is unused by funcs 2-5 (keep a 0 in the file).
void* (__fastcall* g_fxNotifyA)(void*, std::uint32_t, std::uint32_t) = nullptr;
void* (__fastcall* g_fxNotifyB)(void*, std::uint32_t, std::uint32_t) = nullptr;
void (__fastcall* g_glowEmit)(void* weapon) = nullptr;
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
//       BEGINS (moves with no native gauge event). Begin = the move actually
//       becomes current, not the DoAction accept: buffered combo follow-ups
//       accept during the PREVIOUS move, so accept-time firing would deduct
//       before a hit-cancel can eat the request.
//   fx <set> <id> <fxId>                  - fire a player-emitter body effect
//       (FUN_140ADD670 via g_fxNotifyA, same path as the F7 test) when the
//       action begins. 0x7D2 = the spirit-slash red body glow (user-verified).
//       Custom/remapped moves have no per-move FX handler - the vanilla ones
//       are hardcoded per action id - so id detection is the only route.
//   gate <set> <id> <minGauge> <nohitSec> - action starts with gauge >=
//       minGauge: inject a no-hit window and clear the gauge (见切 recipe)
//   regen <set> <id> <seconds> <variant>  - activate the spirit-regeneration
//       state on action start: variant 0 = helm-breaker timer (weapon+0x2378,
//       rate params 0x840/0x844 via color flag +0x237C), 1 = iai timer
//       (weapon+0x2388, rate params 0x850/0x854 via timed flag +0x2384).
//       The per-frame updater does the decay + gauge fill + HUD switch.
//   timer <set> <id> <seconds>            - extend the level-drop countdown
//       (weapon+0x2374, the timer that auto-drops the aura level) by
//       <seconds> when the action starts (whiffs count)
//   timerhit <set> <id> <seconds>         - extend it on CONNECTED hits of
//       the action (per hit; multi-hit moves stack). id 0 = any set-3 action.
//       Both convert seconds with the live w03p Gauge_Duration of the current
//       level - the same primitive as the vanilla 见切-success extension.
//   gainhit <set> <id> <amount>           - add <amount> gauge on CONNECTED
//       hits of the action (same hit gate as timerhit; whiffs never gain).
//       Goes through the accumulator, so it is clamped and regen/HUD stay
//       consistent. For moves whose native per-hit gain must be replaced
//       deterministically (pair with rewrite <set> <id> 0).
struct GaugeRule {
    enum class Type { Rewrite, Cost, Gate, Regen, Timer, TimerHit, Fx, Trail, GainHit };
    Type type = Type::Rewrite;
    std::int32_t set = 3;
    std::int32_t id = 0;
    float a = 0.0f;  // Rewrite: new delta; Cost: amount; Gate: minGauge; Regen: seconds
    float b = 0.0f;  // Rewrite: origVal; Gate: nohitSec; Regen: variant (0=hb, 1=iai)
    bool hasOrig = false;
    float rate = 0.0f;      // Regen optional 5th token: override the rate param
    bool hasRate = false;   // (lower = faster; huge = glow without filling)
    std::uint32_t fxId = 0; // Fx: player-emitter effect id (0x prefix accepted)
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
// force-action.txt 的配置默认值(刚1 目标)。g_forceSet/g_forceId 是"待发目标",
// 会被 sheath 规则/吞单武装覆盖; 弹反链武装前从这里恢复, 两者分离后互不污染。
std::atomic_uint32_t g_forceCfgSet{3};
std::atomic_uint32_t g_forceCfgId{104};
float g_forceInvulnSec = 0.5f;
// Frame seek: jump the action-instance frame counter (instance+8) to a chosen
// start frame right after a forced action begins. <0 = off; the 4th token of
// force-action.txt. FRAMETICK diagnostics arm on every accepted force so an
// ordinary F6 run confirms the field semantics before trusting the write.
std::atomic_bool g_frameWatchArmed{false};
std::atomic_bool g_seekArmed{false};
std::int32_t g_frameWatchSet = 3;
std::int32_t g_frameWatchId = 0;
std::int32_t g_frameWatchTicksLeft = 0;
std::int32_t g_frameTickLogsLeft = 0;
bool g_frameWatchSeen = false;
float g_frameLastLogged = -1.0f;
float g_frameLastDt = -1.0f;
bool g_seekFired = false;
std::int32_t g_seekWrites = 0;
float g_seekFrame = -1.0f;
// Clock miner v2: while a forced action runs, scan candidate regions for
// floats whose per-tick delta is a CONSTANT step (any magnitude >= 0.005).
// v1 only matched +-dt and missed nothing because the motion clock likely
// counts in TIML frames (+2.0/tick at the 30Hz logic rate) or lives deeper.
struct ClkRegion {
    const char* name;
    std::uint32_t size;
    std::vector<float> prev;
    std::vector<float> step;
    std::vector<std::uint8_t> valid;
    std::vector<std::uint8_t> hits;
    const void* lastBase = nullptr;
};
ClkRegion g_clkRegions[7];
std::int32_t g_clkTicks = 0;
bool g_clkReported = false;
float g_clkLastFrame = -1.0f;

void ClkResetRegions() {
    static const struct {
        const char* name;
        std::uint32_t size;
    } defs[] = {
        {"core", 0xCD0},        {"anim468", 0x400},  {"anim470", 0x200},
        {"weapon", 0x2450},     {"playerA", 0x500},  {"playerTail", 0x200},
        {"inst", 0x60},
    };
    for (int i = 0; i < 7; ++i) {
        auto& r = g_clkRegions[i];
        r.name = defs[i].name;
        r.size = defs[i].size;
        r.prev.assign(r.size / 4, 0.0f);
        r.step.assign(r.size / 4, 0.0f);
        r.valid.assign(r.size / 4, 0);
        r.hits.assign(r.size / 4, 0);
        r.lastBase = nullptr;
    }
    g_clkTicks = 0;
    g_clkReported = false;
    g_clkLastFrame = -1.0f;
}
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
void TryFrameSeek(const void* entity);

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
    HkCondEval, HkPoolQuery, HkDmgCalc, HkHitDispatch,
    HkPropInt, HkPropBool, HkPropFloat,
    HkGaugeAdd, HkFxNotify, HkNegrEvent, HkTurnSet, HkHpDelta, HkStamDelta,
    HkVitSet, HkVitAdd, HkCount
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
    // negrEvent = FUN_1421038B0 weapon-side handler of interaction event
    // type 8: the arbitration-visible negation delivery (叮). Prologue is a
    // stock spill + two call/jcc rel32 pairs; the "ff 50 08 83 f8 08" mid-run
    // (virtual GetType + cmp eax,8) is the discriminating run.
    {{0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B,
      0xFA, 0x48, 0x8B, 0xD9, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x07,
      0x48, 0x8B, 0xCF, 0xFF, 0x50, 0x08, 0x83, 0xF8, 0x08, 0x0F, 0x85, 0x00,
      0x00, 0x00, 0x00, 0x48, 0x8B, 0x7F, 0x18, 0x48, 0x8B, 0xCF, 0xE8, 0x00},
     "xxxxxxxxxxxxxxxxx????xxxxxxxxxxxxxx????xxxxxxx??", 0x21038B0},
    // turnSet = FUN_141F755C0 em turn-request setter. The mode branch sits in
    // the first 48 bytes (41 83 E9 01 / 74 08 / 41 83 F9 01 / 74 2D / EB 33 =
    // turnKind-1 / cmp 1 chains); direct [rcx+disp32] loads stay exact on
    // purpose - a layout shift of +0xB220 must invalidate the signature.
    {{0x40, 0x53, 0x48, 0x81, 0xEC, 0x90, 0x00, 0x00, 0x00, 0x0F, 0x29, 0xB4,
      0x24, 0x80, 0x00, 0x00, 0x00, 0x0F, 0x28, 0xF2, 0x48, 0x8B, 0xD9, 0x45,
      0x85, 0xC9, 0x74, 0x2C, 0x41, 0x83, 0xE9, 0x01, 0x74, 0x08, 0x41, 0x83,
      0xF9, 0x01, 0x74, 0x2D, 0xEB, 0x33, 0xF3, 0x0F, 0x10, 0x81, 0x20, 0xB2},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 0x1F755C0},
    // hpDelta = FUN_141F518E0 player vitals single entry (delta applied to
    // *(player+0x7630)+0x64 with the (0,1]->1.0 floor). The prologue reads
    // [rcx+0x7D20] then byte-tests mgr+0x660/0x661 (heal-block gate) - that
    // run discriminates it from same-shaped float setters.
    {{0x40, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x8B, 0x81, 0x20, 0x7D, 0x00,
      0x00, 0x48, 0x8B, 0xD9, 0x0F, 0x29, 0x74, 0x24, 0x30, 0x0F, 0x28, 0xF1,
      0x0F, 0x29, 0x7C, 0x24, 0x20, 0x0F, 0x57, 0xFF, 0x80, 0xB8, 0x60, 0x06,
      0x00, 0x00, 0x00, 0x75, 0x09, 0x80, 0xB8, 0x61, 0x06, 0x00, 0x00, 0x00},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 0x1F518E0},
    // stamDelta = FUN_141C49F10 player stamina consume/recover entry (writes
    // the *(player+0x7670) box, raises the 0x7de UI event on the way out).
    // Wildcards: call rel32 + two jcc rel32.
    {{0x40, 0x57, 0x48, 0x81, 0xEC, 0x50, 0x01, 0x00, 0x00, 0x0F, 0x29, 0xB4,
      0x24, 0x40, 0x01, 0x00, 0x00, 0x48, 0x8B, 0xF9, 0x0F, 0x28, 0xF1, 0xE8,
      0x00, 0x00, 0x00, 0x00, 0x84, 0xC0, 0x0F, 0x85, 0x00, 0x00, 0x00, 0x00,
      0x38, 0x87, 0x28, 0x09, 0x02, 0x00, 0x0F, 0x85, 0x00, 0x00, 0x00, 0x00},
     "xxxxxxxxxxxxxxxxxxxxxxxx????xxxx????xxxxxxxx????", 0x1C49F10},
    // vitSet = FUN_141216940 box cur setter (min(v, [rcx+0x60]) with the
    // r8b floor-to-1.0 branch; both paths store [rcx+0x64]). Shared by every
    // entity, so the detour only OBSERVES the local player's HP/stamina box.
    {{0x45, 0x84, 0xC0, 0x74, 0x1F, 0xF3, 0x0F, 0x5F, 0x0D, 0x00, 0x00, 0x00,
      0x00, 0x0F, 0x57, 0xC0, 0x0F, 0x2F, 0xC8, 0x72, 0x09, 0xF3, 0x0F, 0x10,
      0x41, 0x60, 0xF3, 0x0F, 0x5D, 0xC1, 0xF3, 0x0F, 0x11, 0x41, 0x64, 0xC3,
      0x0F, 0x2F, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x73, 0xE0, 0xF3, 0x0F, 0x10},
     "xxxxxxxxx????xxxxxxxxxxxxxxxxxxxxxxx????xxxx", 0x1216940},
    // vitAdd = FUN_141216880 box cur += delta clamp (heal/damage add path).
    // 32-byte function; the rip-relative comiss disp32 is wildcarded.
    {{0xF3, 0x0F, 0x58, 0x49, 0x64, 0x0F, 0x2F, 0x0D, 0x00, 0x00, 0x00, 0x00,
      0x73, 0x12, 0xF3, 0x0F, 0x10, 0x41, 0x60, 0x0F, 0x57, 0xC9, 0xF3, 0x0F,
      0x5D, 0xC1, 0xF3, 0x0F, 0x11, 0x41, 0x64, 0xC3},
     "xxxxxxxx????xxxxxxxxxxxxxxxxxxxx", 0x1216880},
};

const char* const kHookNames[HkCount] = {
    "condEval", "poolQuery", "dmgCalc", "hitDispatch",
    "propInt", "propBool", "propFloat",
    "gaugeAdd", "fxNotify", "negrEvent", "turnSet", "hpDelta", "stamDelta",
    "vitSet", "vitAdd",
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
//   default "112 0 3 0.15 0 11"  (toId 0 = off, holdVK 0 = no key gate,
//   reqLevel 0 = no level gate, minGauge < 0 = no gauge gate,
//   nohitSec <= 0 = no injection)
// Defense (2026-10-11 user decision): the counter moved off 见切 86's i-frame
// recipe onto 刚见切 3:112 (LMT 345) whose TIML carries the native guard
// window (PMC mFlag 0x90080000 f0->f12) - angle-limited engine guard
// arbitration instead of all-angle i-frames. The GUARD borrow window
// (guard.txt ids) swallows the engine reaction and rolls chip back, the
// 刚1 gauge scope below takes the 0.15 cost. nohitSec stays available for
// i-frame style targets but ships as 0.
std::atomic<std::uint32_t> g_sheathRewriteToId{112};
std::atomic<std::uint32_t> g_sheathRewriteHoldVK{0};
std::atomic<std::uint32_t> g_sheathRewriteReqLevel{3};
std::atomic<float> g_sheathRewriteMinGauge{0.15f};
std::atomic<float> g_sheathRewriteNohitSec{0.2f};
// 11 = standing sheathed R2 draw; 12 = WALKING/RUNNING sheathed R2 draw
// variant (log-confirmed 2026-10-04: 1:4 -> 3:12 x187 vs 1:0 -> 3:11 x158).
// With 11 alone, R2 while moving silently played the vanilla walking draw.
// NOTE: the effective default lives in LoadSheathRewriteConfig - its local
// fromIds overwrites this global on every load (init + Shift+F6).
std::vector<std::uint32_t> g_sheathRewriteFromIds{11, 12};
std::mutex g_sheathRewriteMutex;
// 刚1 gauge accounting (2026-10-04, user-aligned semantics): the cost is
// 0.15 UNCONDITIONALLY - hit/parry/whiff irrelevant, the only condition is
// "the user used the move". The counter move (3:112, LMT 345) has native
// gauge handlers that move weapon+0x2368 through SEVERAL paths (GAUGEEVT-
// visible events AND direct writes like the whiff full-clear), so those
// writers are BYPASSED, not intercepted:
//   phase 1 (waiting): from the rewrite until the restore moment - the FIRST
//     of: the 叮 (bit15 rising) / the move reaching frame 56 (TIML) / a
//     different set-3 action becoming current (ended or canceled) / the 5s
//     deadline. Vanilla does whatever in the meantime, no guard.
//   settle: write gauge = clamp(snapshot - 0.15), expected = same.
//   phase 2 (guard tail, 1s): FUN_142122410 events feed `expected` (negative
//     only - chain-move costs stack; positive vanilla rewards stay
//     suppressed per the 0.15-net spec). Any drift between the real gauge
//     and `expected` = a direct write (the whiff clear) -> reverted.
// Net cost of a sheathed 刚1 press = 0.15, always.
std::atomic<std::int32_t> g_gang1Phase{0};  // 0 idle / 1 waiting / 2 guard tail
std::atomic<float> g_gang1Snapshot{0.0f};
std::atomic<float> g_gang1Expected{0.0f};
std::atomic<std::int64_t> g_gang1DeadlineMs{0};
// Event bit 15 (the vanilla 见切 success latch, player+0x8938 & 0x8000) at
// arm time - success = the bit RISING during the scope, not its stale value.
// Dormant for the 112 scope: bit15 is set by the vanilla action-86 machinery
// only, so the parry restore never fires and the cost lands unconditionally
// via frame56/left/deadline (user design: 0.15 net, always).
std::atomic_bool g_gang1Bit15AtArm{false};
constexpr float kGang1GaugeCost = 0.15f;
// Restore timing (user design, 2026-10-04): the cost is 0.15 UNCONDITIONALLY
// - hit/parry/whiff irrelevant; the only condition is "the user used the
// move". Mid-move vanilla writers are bypassed, not intercepted: snapshot x
// at the rewrite, then restore x-0.15 at the FIRST of (1) the 叮 (bit15
// rising), (2) the move reaching frame 56 (TIML frames, 60/s), (3) a
// different set-3 action becoming current (ended/canceled - the move came
// out, the cost is owed), (4) the 5s deadline. The 1s guard tail then
// reverts any straggler direct writes (the whiff clear).
constexpr float kGang1RestoreFrame = 56.0f;

void InjectNoHitLocked(float seconds);

void LoadSheathRewriteConfig() {
    int toId = 112;
    int holdVk = 0;
    int reqLevel = 3;
    float minGauge = 0.15f;
    float nohitSec = 0.2f;
    // THIS local default is the one that ships: it unconditionally overwrites
    // the g_sheathRewriteFromIds global below (config-file trailing fromIds
    // APPEND to it). 12 = the walking/running sheathed draw variant
    // (log-confirmed 2026-10-04: 1:4 -> 3:12 x187 vs 1:0 -> 3:11 x158) -
    // editing only the global default leaves 12 leaks invisible.
    std::vector<std::uint32_t> fromIds{11, 12};
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
        fromIds.push_back(12);
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
            // Not a rewrite target. If this arrives while SHEATHED (set-1
            // predecessor) it silently plays vanilla - the exact leak class
            // that hid the walking variant 3:12 for weeks (non-matching ids
            // used to return with zero logging). Rate-limited visibility so
            // remaining variants (climb/swim draws, ...) surface in one run.
            std::scoped_lock actionLock(g_actionMutex);
            if (g_lastLongSwordAction && g_lastLongSwordAction->actionSet == 1) {
                static std::atomic_uint32_t seenLogs{0};
                if (seenLogs.fetch_add(1, std::memory_order_relaxed) < 30) {
                    std::ostringstream m;
                    m << "SHEATHSEEN: sheathed request 3:" << fromId
                      << " not in fromIds (last=1:"
                      << g_lastLongSwordAction->actionId << ") - vanilla plays";
                    Log(m.str());
                }
            }
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
    // ALSO sheathed: the sheathe ANIMATION itself (set-3, tracked as the last
    // action while it plays). 9 = the common sheathe (log-proven: after 9 ->
    // 1:15/1:8/1:4 sheathed locomotion x651; R2 during it was blocked and the
    // vanilla draw popped out mid-sheathe - the user's "1:8 -> 1:4 transition"
    // feel), 6 = the valor-hold release sheathe. Deliberately NOT 3:0/3:1/3:2
    // (drawn idle/walk/run: 3:0 -> 3:11 x347 is the drawn R2, correct vanilla)
    // and not 3:5 (rare, semantics unpinned - SHEATHSEEN watches it).
    static constexpr std::uint32_t kSheatheActionIds[] = {9, 6};
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
    const bool sheathed =
        lastSet == 1 ||
        (lastSet == 3 &&
         std::find(std::begin(kSheatheActionIds), std::end(kSheatheActionIds),
                   static_cast<std::uint32_t>(lastId)) !=
             std::end(kSheatheActionIds));
    if (!sheathed) {
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

// SHEATHREASSERT: set by a sheath-counter rule fire; while active, an
// incoming engine guard-reaction load ({3,37}) is swallowed in DoActionHook
// so it cannot overwrite the just-forced counter move.
std::atomic_int64_t g_sheathReassertUntilMs{0};
std::atomic_int32_t g_sheathReassertSet{3};
std::atomic_int32_t g_sheathReassertId{77};

bool SheathWatchedAction(std::uint32_t actionSet, std::uint32_t actionId);
void SheathArmHitRule(std::uint32_t holdId, std::uint32_t reactionId);

// Arms the 刚1 gauge scope for ANY local-player 112 accept - the sheath
// rewrite target IS the 刚1 move (3:112, LMT 345, TIML guard frames) with
// the same unconditional 0.15 cost per user design.
// Deterministically pre-cost: DoAction accept happens before the updater's
// begin runs the start-cost handler. Skips while a scope is already waiting
// (phase 1); a settled/tail scope (phase 2) is re-armed by the next press.
// Hard LS gate: 86 collides across weapon move tables (the GS lesson).
#ifndef VALOR_PROBE
void ArmGang1GaugeScope(void* controller) {
    if (g_gang1Phase.load(std::memory_order_acquire) == 1) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (!IsReadable(player + 0x76B0, sizeof(void*))) {
        return;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
    if (!weapon || !IsReadable(weapon + 0x2368, sizeof(float)) ||
        !IsReadable(weapon + 0x9F8, sizeof(std::uint32_t)) ||
        *reinterpret_cast<const volatile std::uint32_t*>(weapon + 0x9F8) !=
            g_modWeaponType.load(std::memory_order_relaxed)) {
        return;
    }
    g_gang1Snapshot.store(
        *reinterpret_cast<const volatile float*>(weapon + 0x2368),
        std::memory_order_relaxed);
    g_gang1DeadlineMs.store(
        static_cast<std::int64_t>(GetTickCount64()) + 5000,
        std::memory_order_relaxed);
    {
        const bool bit15 =
            IsReadable(player + 0x8938, sizeof(std::uint32_t)) &&
            (*reinterpret_cast<const volatile std::uint32_t*>(
                 player + 0x8938) &
             0x8000) != 0;
        g_gang1Bit15AtArm.store(bit15, std::memory_order_relaxed);
    }
    g_gang1Phase.store(1, std::memory_order_release);
}
#endif // !VALOR_PROBE

#ifndef VALOR_PROBE
// GUARD shared state (defined here so DoActionHook can consult it; the rest
// of the guard system lives below LoadSheathCounterConfig).
struct GuardConfig {
    std::vector<std::uint32_t> ids;
    // Subset of ids where the mod-side directional arm fallback (and the
    // window-wide lethal override) may negate. All currently watched ids
    // (112 / 98 / 99) carry NATIVE guard points (TIML PMC bit28), so
    // direction gating is the engine's and this list ships empty-by-config
    // ("armIds 0"): the block negation runs off the reaction rollback only.
    // The legacy entry was 86 - the old i-frame counter had no native guard
    // point, so direction gating was ours. Empty list = ALL ids (legacy
    // behavior) - write "armIds 0" to express none (0 is never current
    // while a window is open: the window closes on any non-watched action).
    std::vector<std::uint32_t> armIds;
    // Turn-clamp scope (GuardTurnSetHook): clamp turning to startYaw +/- maxTurn
    // ONLY while the CURRENT action is one of these ids. Default {112}: 112
    // (like the old 86 counter) is a short unsteered counter, while 98/99 are
    // HELD stances where vanilla lets the player steer with WASD - clamping
    // there froze the valor-sheathe turn at the stance-entry yaw (2026-10-11
    // user A/B: DLL renamed out = angle fine; TURNCLAMP yaw 180 -> start+30
    // in the log). Config "turnIds <ids>" overrides; "turnIds 0" disables
    // the clamp everywhere (0 is never a live set-3 id).
    std::vector<std::uint32_t> turnIds{112};
    float arcHalfDeg = 90.0f;
    float maxTurnDeg = 30.0f;
    int guardLevel = 2;
    bool staminaRefund = true;
    bool gaugeRefill = true;
    float maxWindowSec = 6.0f;
    bool armFallback = true;
    bool swallowReaction = true;
    bool counterOnBlock = false;
};
GuardConfig g_guardCfg;
std::atomic<bool> g_guardActive{false};
// GUARD forward decls (definitions below LoadSheathCounterConfig).
void GuardOnReactionAction(std::uint32_t actionSet, std::uint32_t actionId);
void GuardOnActionAccepted(void* controller, std::uint32_t actionId);
void PollGuardWindow();
void PollHpLock();
void PollHpWatch();
void StaminaOnActionAccepted(std::uint32_t actionId);
void PollStaminaControl();
#endif
bool __fastcall DoActionHook(void* controller, ActionInfo* action) {
    const bool validAction = controller && action && IsReadable(action, sizeof(ActionInfo));
    g_insideDoAction.store(true, std::memory_order_release);
    // Snapshot the PREVIOUS action before the tracker update below replaces
    // it with the incoming id: the guard-reaction swallow further down must
    // test the action the player was IN (98), not the reaction itself (35).
    std::optional<ActionInfo> prevSheathAction;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction) {
            prevSheathAction = *g_lastLongSwordAction;
        }
    }
#ifndef VALOR_PROBE
    const bool guardReactionFamily = validAction && action->actionSet == 3 &&
        (action->actionId >= 35 && action->actionId <= 38);
    const bool reassertActive =
        g_sheathReassertUntilMs.load(std::memory_order_relaxed) >
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    const bool watchedHold = prevSheathAction &&
        prevSheathAction->actionSet == 3 &&
        SheathWatchedAction(3, static_cast<std::uint32_t>(prevSheathAction->actionId));
    // GUARDREACT: while a guard window is open, the engine routing a hit to
    // the guard-reaction family ({3,35..38}, per-weapon DAMAGE_GUARD_S/M/L
    // tables) IS the "blocked" confirmation - always latch it for the
    // deferred HP write. Swallow the reaction (keep the move playing) only
    // when the sheath-counter path does NOT own this hold: there the
    // swallow+SheathArmHitRule flow is the configured counter trigger and
    // must keep working (GUARDHP negation still runs off the latch).
    if (validAction && controller == g_lockedPlayerController &&
        guardReactionFamily && g_guardActive.load(std::memory_order_acquire)) {
        GuardOnReactionAction(
            static_cast<std::uint32_t>(action->actionSet),
            static_cast<std::uint32_t>(action->actionId));
        if (g_guardCfg.swallowReaction && !(reassertActive || watchedHold)) {
            g_insideDoAction.store(false, std::memory_order_release);
            return true;
        }
    }
    // SHEATHREASSERT: swallow the engine's guard-reaction family (35/36/37)
    if (validAction && controller == g_lockedPlayerController &&
        guardReactionFamily && (reassertActive || watchedHold)) {
        // A guarded hit loads its reaction exactly once, so the swallow IS
        // the per-hit signal: arm the matching rule's target (heavy hits
        // pulse no event bits - bit222 only covers light ones).
        if (watchedHold && prevSheathAction) {
            SheathArmHitRule(
                static_cast<std::uint32_t>(prevSheathAction->actionId),
                action->actionId);
        }
        std::ostringstream m;
        m << "SHEATHREASSERT: swallowed guard reaction 3:"
          << action->actionId << " (keeping 3:"
          << g_sheathReassertId.load(std::memory_order_relaxed) << ")";
        Log(m.str());
        g_insideDoAction.store(false, std::memory_order_release);
        return true;
    }
#endif
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
        // GUARD window open/close on the accepted action id (after the
        // sheath rewrite above, so rewritten ids arm the window too).
        GuardOnActionAccepted(controller,
                              static_cast<std::uint32_t>(action->actionId));
        // Stamina control: one-shot cost (见切) + reset the empty-exit latch.
        StaminaOnActionAccepted(
            static_cast<std::uint32_t>(action->actionId));
        ApplyActionGaugeRules(action->actionSet, action->actionId);
        // 刚1 gauge scope: arm for EVERY local-player 112 accept (rewritten
        // sheathed draws and any natural 112 - the 刚1 move, unconditional
        // 0.15 net cost). Snapshot is pre-cost: the updater's begin
        // (start-cost handler) runs after this.
        if (action->actionId == 112) {
            ArmGang1GaugeScope(controller);
        }
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

// --- Sheathe i-frame counter (特殊纳刀 family 98/99/100) ----------------------
// The TIML b26+mTrigger recipe grants these motions raw i-frames, but the
// vanilla negation latches (bit15 handler = 86, bit250 handler = 101/102) are
// action-gated and never fire for the sheathe family - "hit during i-frames"
// is undetectable there without mining. This watcher both MINES the shared
// event-bit array (player+0x8938..) during the watched actions (F5 = manual
// "got hit" marker) and fires config rules: a rising edge on the configured
// event bit opens the counter window and force-queues the switch target.
// Config nativePC/plugins/ValorGauge.sheath-counter.txt (default inert):
//   rule <duringIds> <eventBit> <toSet> <toId>
//   mine <duringIds>
// duringIds = comma list of set-3 action ids; eventBit = index into the
// event-bit array (dword*32+bit, e.g. 250 = dword7 bit26).
struct SheathCounterRule {
    std::vector<std::uint32_t> duringIds;
    std::uint32_t eventBit = 0;
    std::uint32_t toSet = 0;
    std::uint32_t toId = 0;
    bool prev = false;
    bool dispatchBased = false; // drule: fire on rejected type-6 dispatch
    bool armorBased = false;    // arule: fire on connected hit (armor window)
    bool keyBased = false;      // krel: fire on sheathe-key release (down->up)
    unsigned vk = 0;            // krel: watch key (VK_XBUTTON2=6 mouse side-up)
    // Per-rule one-shot: re-armed when the watched action id changes. The
    // global g_clangLatch must NOT gate these rules - it only re-arms on
    // 86/101/102, so a single fire would block every later sheathe guard.
    bool fired = false;
    std::uint32_t lastActId = 0;
};
std::vector<SheathCounterRule> g_sheathRules;

bool SheathIdsContain(const std::vector<std::uint32_t>& ids, std::uint32_t id);
void SheathArmHitRule(std::uint32_t holdId, std::uint32_t reactionId);
// A guarded hit during a watched action loads its reaction exactly once -
// swallowing it IS the per-hit signal. Arm the matching rule's target
// (heavy hits pulse no event bits; bit222 only covers light ones).
void SheathArmHitRule(std::uint32_t holdId, std::uint32_t reactionId) {
    for (auto& rule : g_sheathRules) {
        if (rule.keyBased || rule.dispatchBased) {
            continue;
        }
        if (!SheathIdsContain(rule.duringIds, holdId)) {
            continue;
        }
        if (!g_forceArmed.load(std::memory_order_acquire)) {
            g_forceSet = rule.toSet;
            g_forceId = rule.toId;
            g_forceArmed.store(true, std::memory_order_release);
            std::ostringstream m;
            m << "SHEATHGUARD: reaction 3:" << reactionId
              << " swallowed during 3:" << holdId << " - force "
              << rule.toSet << ':' << rule.toId;
            Log(m.str());
        }
        break;
    }
}

bool SheathWatchedAction(std::uint32_t actionSet, std::uint32_t actionId) {
    if (actionSet != 3) {
        return false;
    }
    for (const auto& rule : g_sheathRules) {
        if (SheathIdsContain(rule.duringIds, actionId)) {
            return true;
        }
    }
    return false;
}

std::vector<std::uint32_t> g_sheathMineIds;
std::vector<std::uint32_t> g_sheathBlueIds; // bluesim: mimic the 见切/居合
// per-move handlers while inside these actions. The vanilla handlers
// (FUN_142125060 for 86, FUN_142124F70 for 101/102) are dispatched by action
// id via function-pointer tables, so custom actions never run them; bluesim
// writes the same outputs: +0x2399 latch, +0x2CED, per-frame
// +0x23A0/+0x23A4 <- playerParam+0x8A8/0x8B8, FX 0x7D7 (blue flash) and
// 0x7D4 (iai sound) once per action.
std::atomic_uint32_t g_sheathMineLogCount{0};
std::uint32_t g_sheathBlueActId = 0;
bool g_sheathBlueFx = false;
std::uint32_t g_sheathMinePrevEv[8] = {};
bool g_sheathMinePrimed = false;

bool ParseIdList(const std::string& text, std::vector<std::uint32_t>& out) {
    out.clear();
    std::size_t i = 0;
    while (i <= text.size()) {
        std::size_t j = text.find(',', i);
        if (j == std::string::npos) j = text.size();
        if (j > i) {
            try {
                out.push_back(
                    static_cast<std::uint32_t>(std::stoul(text.substr(i, j - i))));
            } catch (...) {
                return false;
            }
        }
        i = j + 1;
        if (j == text.size()) break;
    }
    return !out.empty();
}

bool SheathIdsContain(const std::vector<std::uint32_t>& ids, std::uint32_t id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
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
// Set by whichever DmgCalcHook is installed while inside a hit-dispatch call.
// A type-6 dispatch that finishes without this flag = the hit was negated
// (见切-style arbitration rejection or a no-hit window eating the hit).
bool g_damageThisDispatch = false; // game thread only

#ifdef VALOR_PROBE

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
float __fastcall GaugeAddHook(void* weapon, float value) {
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
    // 刚1 guard-tail bookkeeping: legit costs reaching the accumulator (the
    // chain moves' native handlers) shift `expected` so the drift guard does
    // not revert them. Negative only - positive vanilla rewards (the 见切
    // success refill) stay suppressed per the 0.15-net spec.
    if (own && g_gang1Phase.load(std::memory_order_acquire) == 2 &&
        value < 0.0f) {
        const float e = g_gang1Expected.load(std::memory_order_relaxed);
        g_gang1Expected.store(std::clamp(e + value, 0.0f, 1.0f),
                              std::memory_order_relaxed);
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
    float result = 0.0f;
    if (g_originalGaugeAdd) {
        result = g_originalGaugeAdd(weapon, value);
    }
    if (!hasWeapon) {
        return result;
    }
    const float after = *reinterpret_cast<const volatile float*>(
        static_cast<std::uint8_t*>(weapon) + 0x2368);
    const float absVal = value < 0.0f ? -value : value;
    if (absVal < g_gaugeLogMinAbs.load(std::memory_order_relaxed)) {
        return result;
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
        return result;
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
    return result;
}

void LoadGaugeRulesConfig() {
    g_gaugeRules.clear();
    // Valor-chain constants (2026-10-04, user-defined): the custom spirit
    // moves share one profile - 0.25 gauge cost + the spirit red body glow
    // (0x7D2) on begin. 105/106 = 一文字, 107 = 无双斩, 108 = third chain
    // move. Seeded in code so the behavior survives a missing config file;
    // explicit lines in ValorGauge.gauge-rules.txt replace these by
    // (type,set,id). 刚1's own gate is the separate sheath-rewrite default
    // (0.15) - NOT part of this table.
    for (const std::uint32_t chainId : {105u, 106u, 107u, 108u}) {
        GaugeRule cost;
        cost.type = GaugeRule::Type::Cost;
        cost.set = 3;
        cost.id = static_cast<std::int32_t>(chainId);
        cost.a = 0.25f;
        g_gaugeRules.push_back(cost);
        GaugeRule fx;
        fx.type = GaugeRule::Type::Fx;
        fx.set = 3;
        fx.id = static_cast<std::int32_t>(chainId);
        fx.fxId = 0x7D2;
        g_gaugeRules.push_back(fx);
    }
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
        } else if (kind == "timer" || kind == "timerhit") {
            rule.type = kind == "timer" ? GaugeRule::Type::Timer
                                        : GaugeRule::Type::TimerHit;
            if (!(ss >> rule.set >> rule.id >> rule.a)) {
                continue;
            }
        } else if (kind == "gainhit") {
            rule.type = GaugeRule::Type::GainHit;
            if (!(ss >> rule.set >> rule.id >> rule.a)) {
                continue;
            }
        } else if (kind == "fx") {
            rule.type = GaugeRule::Type::Fx;
            if (!(ss >> rule.set >> rule.id)) {
                continue;
            }
            std::string fxToken;
            if (!(ss >> fxToken)) {
                continue;
            }
            rule.fxId = static_cast<std::uint32_t>(
                std::strtoul(fxToken.c_str(), nullptr, 0));
        } else if (kind == "trail") {
            // 2026-10-21: hold weapon+0x2398 = 1 every frame while <set> <id>
            // is the current action. The flag means "spirit gauge cost paid"
            // and gates both the generic red-glow emitter and the per-frame
            // weapon trail; native arming only happens inside the kijin move
            // begin handlers (cost>0 && gauge pays), so custom/imported moves
            // never arm it on their own.
            rule.type = GaugeRule::Type::Trail;
            if (!(ss >> rule.set >> rule.id)) {
                continue;
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
        // Explicit config lines replace the seeded valor-chain defaults
        // (FindGaugeRule returns the first match, so replace instead of add).
        std::erase_if(g_gaugeRules, [&](const GaugeRule& seeded) {
            return seeded.type == rule.type && seeded.set == rule.set &&
                   seeded.id == rule.id;
        });
        g_gaugeRules.push_back(rule);
    }
    std::ostringstream m;
    m << "Gauge rules: " << g_gaugeRules.size() << " loaded (incl. valor-chain"
      << " seeds 105/106/107/108)"
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

// timer rules accept id 0 as a wildcard for any action in the set.
const GaugeRule* FindGaugeRuleLoose(GaugeRule::Type type, std::int32_t set,
                                    std::int32_t id) {
    if (const auto* rule = FindGaugeRule(type, set, id)) {
        return rule;
    }
    return id == 0 ? nullptr : FindGaugeRule(type, set, 0);
}

// timer / timerhit rules: extend the level-drop countdown (weapon+0x2374,
// 0..1 fraction of the current level's Gauge_Duration) - the same primitive
// as the vanilla 见切-success extension (FUN_142123510:
// +0x2374 += playerParam+0x888). Seconds convert with the live w03p duration
// (playerParam+0x858/85C/860 by level) so "1.0" buys one countdown second.
void ExtendLevelDropTimer(std::uint8_t* player, float seconds,
                          std::int32_t actionSet, std::int32_t actionId,
                          bool fromHit) {
    if (seconds <= 0.0f || !player) {
        return;
    }
    if (!IsReadable(player + 0x76B0, sizeof(void*))) {
        return;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
    if (!weapon || !IsReadable(weapon + 0x2374, sizeof(float))) {
        return;
    }
    // Defense in depth: the level timer / duration params only exist on the
    // Long Sword weapon struct - never touch a foreign weapon.
    if (!IsReadable(weapon + 0x9F8, sizeof(std::uint32_t)) ||
        *reinterpret_cast<const volatile std::uint32_t*>(weapon + 0x9F8) !=
            g_modWeaponType.load(std::memory_order_relaxed)) {
        return;
    }
    const auto level =
        *reinterpret_cast<const volatile std::int32_t*>(weapon + 0x2370);
    if (level < 1 || level > 3) {
        return;  // no aura level to sustain
    }
    if (!IsReadable(player + 0x7810, sizeof(void*))) {
        return;
    }
    auto* paramTable = *reinterpret_cast<std::uint8_t**>(player + 0x7810);
    if (!paramTable) {
        return;
    }
    const float duration = *reinterpret_cast<const volatile float*>(
        paramTable + 0x858 + (level - 1) * 4);
    if (!(duration > 0.0f)) {
        return;
    }
    auto* timer = reinterpret_cast<volatile float*>(weapon + 0x2374);
    const float cur = *timer;
    const float next = std::min(1.0f, cur + seconds / duration);
    if (next == cur) {
        return;
    }
    *timer = next;
    static std::atomic_uint32_t timerRuleLogs{0};
    if (timerRuleLogs.load(std::memory_order_relaxed) < 128) {
        timerRuleLogs.fetch_add(1, std::memory_order_relaxed);
        std::ostringstream m;
        m << "GAUGERULE timer" << (fromHit ? "-hit" : "") << " act="
          << actionSet << ':' << actionId << " +" << seconds << "s lv=" << level
          << " dur=" << duration << " countdown=" << cur * duration << "s -> "
          << next * duration << "s";
        Log(m.str());
    }
}

// Shared preamble for both rule paths (accept-time and begin-time): resolve
// the locked player's weapon and hard-gate on the modded weapon type. Every
// weapon routes its set-3 actions through the same DoAction path, and rule
// ids are LS move ids - without this check a colliding action id (or a
// wildcard rule) would apply an LS rule - and the LS-only gauge natives /
// weapon offsets - to a foreign weapon struct. Returns nullptr when out of
// scope; caller re-derives the player from g_lockedPlayerController.
std::uint8_t* GaugeRuleWeapon() {
    if (g_gaugeRules.empty() || !g_lockedPlayerController || !g_originalGaugeAdd) {
        return nullptr;
    }
    auto* player =
        static_cast<std::uint8_t*>(g_lockedPlayerController) - 0x61C8;
    if (!IsReadable(player + 0x76B0, sizeof(void*))) {
        return nullptr;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
    if (!weapon || !IsReadable(weapon + 0x2374, sizeof(float))) {
        return nullptr;
    }
    if (!IsReadable(weapon + 0x9F8, sizeof(std::uint32_t)) ||
        *reinterpret_cast<const volatile std::uint32_t*>(weapon + 0x9F8) !=
            g_modWeaponType.load(std::memory_order_relaxed)) {
        return nullptr;
    }
    return weapon;
}

// Accept-time rules (gate / regen / timer), applied from DoActionHook after
// the original accepted a locked-player set-3 action. cost + fx live in the
// begin-time watcher below - buffered combo follow-ups accept while the
// previous move is still playing.
void ApplyActionGaugeRules(std::int32_t actionSet, std::int32_t actionId) {
    auto* weapon = GaugeRuleWeapon();
    if (!weapon) {
        return;
    }
    auto* player =
        static_cast<std::uint8_t*>(g_lockedPlayerController) - 0x61C8;
    auto readGauge = [&]() {
        return *reinterpret_cast<const volatile float*>(weapon + 0x2368);
    };
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
    if (const auto* rule =
            FindGaugeRuleLoose(GaugeRule::Type::Timer, actionSet, actionId)) {
        ExtendLevelDropTimer(player, rule->a, actionSet, actionId, false);
    }
}

// FUN_140ADD670 choke point: every player-emitter body effect (0x7D1-0x7DC
// family, 2000 parametric family, ...) passes here. Log local-player calls
// with the caller RVA; suppress exact repeats within 0.5s and hard-cap 30/s.
// MUST return the original's handle (rax) - see the note on g_originalFxNotify.
void* __fastcall FxNotifyHook(void* emitter, std::uint32_t id,
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
    void* result = nullptr;
    if (g_originalFxNotify) {
        result = g_originalFxNotify(emitter, id, arg);
    }
    if (!own) {
        return result;
    }
    std::uintptr_t callerRva = 0;
    if (returnAddress) {
        callerRva = reinterpret_cast<std::uintptr_t>(returnAddress) -
                    g_moduleBase.load(std::memory_order_relaxed);
    }
    // Skip callers outside the game module (our own F7 test gun produces
    // garbage RVAs that would pollute the log).
    if (callerRva == 0 || callerRva > 0x4000000) {
        return result;
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
        return result;
    }
    if (id == lastId && arg == lastArg && callerRva == lastCaller &&
        now - lastTime < std::chrono::milliseconds(500)) {
        return result;
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
    return result;
}

// FUN_1421038B0: weapon-side handler of interaction event type 8 - the
// arbitration-visible negation delivery (叮). Only reached when a hit is
// nullified AS an interaction; query-level exclusions (pool no-hit) never
// produce it. Inside, byte [weapon+0x3168] routes to the family processors
// (0 -> FUN_142125060 见切 id==86, 1 -> FUN_142124f70 居合 id 101/102) which
// write the once-latches +0x2399/+0x2CED + FX 0x7D4. Passive log of every
// local-player arrival with the selector + post-call latch states answers
// the generalization question: do arbitrary moves with authored TIML no-hit
// windows produce event 8 at all (latches stay 0 on non-family ids)?
void (__fastcall* g_originalNegrEvent)(void*, void*) = nullptr;

void __fastcall NegrEventHook(void* weapon, void* event) {
    if (g_originalNegrEvent) {
        g_originalNegrEvent(weapon, event);
    }
    // Diagnostic build: log the first 40 arrivals UNFILTERED with the full
    // pointer identity (handler weapon vs *(player+0x76B0) vs owner) - the
    // own==weapon filter silently ate every call in the first deployment.
    static std::atomic_uint32_t logCount{0};
    if (!weapon ||
        logCount.load(std::memory_order_relaxed) >= 40) {
        return;
    }
    logCount.fetch_add(1, std::memory_order_relaxed);
    auto* wp = static_cast<const std::uint8_t*>(weapon);
    const unsigned sel =
        IsReadable(wp + 0x3168, 1) ? wp[0x3168] : 0xFF;
    const unsigned latchEvade =
        IsReadable(wp + 0x2399, 1) ? wp[0x2399] : 0xFF;
    const unsigned latchIai =
        IsReadable(wp + 0x2CED, 1) ? wp[0x2CED] : 0xFF;
    void* owner = IsReadable(wp + 0xB40, sizeof(void*))
                      ? *reinterpret_cast<void* const*>(wp + 0xB40)
                      : nullptr;
    void* curWeapon = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lockedPlayerController) {
            auto* player =
                static_cast<std::uint8_t*>(g_lockedPlayerController) - 0x61C8;
            if (IsReadable(player + 0x76B0, sizeof(void*))) {
                curWeapon = *reinterpret_cast<void**>(player + 0x76B0);
            }
        }
    }
    std::ostringstream m;
    m << "NEGREVT weapon=0x" << std::hex << reinterpret_cast<std::uintptr_t>(weapon)
      << " owner=0x" << reinterpret_cast<std::uintptr_t>(owner)
      << " curWpn=0x" << reinterpret_cast<std::uintptr_t>(curWeapon)
      << " match=" << (weapon == curWeapon ? 1 : 0) << std::dec
      << " sel=" << static_cast<unsigned>(sel)
      << " latch2399=" << static_cast<unsigned>(latchEvade)
      << " latch2ced=" << static_cast<unsigned>(latchIai);
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
    // NOTE: HITATT is NOT a 见切 success signal - the counter slash physically
    // connects whether or not the parry happened, and un-guarding on it let
    // the vanilla whiff clear through (user-tested: still cleared). The real
    // success latch is event bit 15, checked in PollGang1GaugeScope.
    // timerhit rules: connected hits of the current action extend the
    // level-drop countdown. Independent of the valor-charge gates below so
    // it also runs in valor mode / with charging off.
    if (!g_gaugeRules.empty() && ModWeaponActive()) {
        std::int32_t actSet = 0;
        std::int32_t actId = 0;
        bool hasAct = false;
        {
            std::scoped_lock lock(g_actionMutex);
            if (g_lastLongSwordAction) {
                actSet = g_lastLongSwordAction->actionSet;
                actId = g_lastLongSwordAction->actionId;
                hasAct = true;
            }
        }
        if (hasAct && actSet == 3) {
            if (const auto* rule =
                    FindGaugeRuleLoose(GaugeRule::Type::TimerHit, actSet, actId)) {
                ExtendLevelDropTimer(
                    static_cast<std::uint8_t*>(controller) - 0x61C8, rule->a,
                    actSet, actId, true);
            }
            // gainhit: the connected-hit gate is upstream (this only runs for
            // the local player's landed hits), so the amount goes straight
            // through the accumulator.
            if (const auto* gain =
                    FindGaugeRuleLoose(GaugeRule::Type::GainHit, actSet, actId)) {
                auto* weapon = *reinterpret_cast<std::uint8_t**>(
                    static_cast<std::uint8_t*>(controller) - 0x61C8 + 0x76B0);
                if (weapon && g_originalGaugeAdd &&
                    IsReadable(weapon + 0x2368, sizeof(float))) {
                    const float before = *reinterpret_cast<volatile float*>(
                        weapon + 0x2368);
                    g_originalGaugeAdd(weapon, gain->a);
                    const float after = *reinterpret_cast<volatile float*>(
                        weapon + 0x2368);
                    std::ostringstream m;
                    m << "GAUGERULE gainhit act=" << actSet << ':' << actId
                      << " +" << gain->a << " gauge=" << before << "->" << after;
                    Log(m.str());
                }
            }
        }
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

long long __fastcall DmgCalcHook(long long a1, long long a2, std::uint32_t* a3);
void SheathArmorHitCheck(long long attacker, long long defenderCtx);
void NotifyGuardDamageHit(long long attacker, long long defenderCtx);
void* ResolveActionInstance(void* ctrl, std::int32_t set, std::int32_t id);
#ifndef VALOR_PROBE
// GUARD hook forward decls (definitions below LoadSheathCounterConfig).
void __fastcall GuardTurnSetHook(void* em, float yawDeg, float duration,
                                 int mode, int turnKind);
void __fastcall GuardHpDeltaHook(void* player, float delta);
void __fastcall GuardStamDeltaHook(void* player, float delta);
void __fastcall GuardVitSetHook(void* box, float value, std::uint8_t floorFlag);
void __fastcall GuardVitAddHook(void* box, float delta);
using GuardTurnFn = void(__fastcall*)(void*, float, float, int, int);
using GuardDeltaFn = void(__fastcall*)(void*, float);
using GuardVitSetFn = void(__fastcall*)(void*, float, std::uint8_t);
extern GuardTurnFn g_originalTurnSet;
extern GuardDeltaFn g_originalHpDelta;
extern GuardDeltaFn g_originalStamDelta;
extern GuardVitSetFn g_originalVitSet;
extern GuardDeltaFn g_originalVitAdd;
// Vitals slot config (definitions below LoadSheathCounterConfig).
extern std::atomic<std::size_t> g_vitalsHpSlot;
extern std::atomic<bool> g_vitalsHpSlotCustom;
std::uint8_t* GuardVitalsBox(std::uint8_t* player, std::size_t slot);
void GuardClearHealingQuota(std::uint8_t* box);
void GuardApplyBlockBenefits(std::uint8_t* player, std::uint8_t* box);
bool GuardCurrentActionInArmIds();
#endif
// MOTIONTRACE: log current-motion id changes (instance+0x30, entry|0xC000)
// during set-3 actions so mid-action motion switches (FSM LinkMotion 衔接)
// become visible against the damage-window timeline.
static std::atomic<std::int32_t> g_motionTraceLeft{150};
static std::uint32_t g_motionTraceLast = 0xFFFFFFFFu;
static void MotionTraceTick(const void* entity);
// HITFRAME: log every connected local-player hit with the current action and
// the motion clock (anim+0x10C, TIML frames) so authored attack windows can be
// correlated with real hit timing (ichimonji mid-window hole investigation).
static std::atomic<std::int32_t> g_hitFrameLogsLeft{80};
static void LogHitFrame(long long attacker) {
    if (g_hitFrameLogsLeft.load(std::memory_order_relaxed) <= 0) return;
    void* ctrl = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        ctrl = g_lockedPlayerController;
    }
    if (!ctrl) return;
    const auto localPlayer =
        reinterpret_cast<std::uint8_t*>(ctrl) - 0x61C8;
    if (reinterpret_cast<long long>(localPlayer) != attacker) return;
    auto* bytes = static_cast<std::uint8_t*>(ctrl);
    const std::int32_t set = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xAC);
    const std::int32_t id = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xB0);
    if (set != 3) return;
    float frame = -1.0f;
    void* inst = ResolveActionInstance(ctrl, set, id);
    if (inst) frame = *reinterpret_cast<volatile float*>(static_cast<std::uint8_t*>(inst) + 8);
    float mot = -1.0f;
    if (IsReadable(localPlayer + 0x468, sizeof(void*))) {
        void* anim = *reinterpret_cast<void* const*>(localPlayer + 0x468);
        if (anim && IsReadable(static_cast<std::uint8_t*>(anim) + 0x10C, 4)) {
            mot = *reinterpret_cast<volatile float*>(static_cast<std::uint8_t*>(anim) + 0x10C);
        }
    }
    g_hitFrameLogsLeft.fetch_sub(1, std::memory_order_relaxed);
    float motion = -1.0f;
    void* inst2 = ResolveActionInstance(ctrl, set, id);
    if (inst2) {
        motion = static_cast<float>(
            *reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uint8_t*>(inst2) + 0x30));
    }
    std::ostringstream m;
    m << "HITFRAME act=" << set << ':' << id << " frame=" << frame << " mot=" << mot
      << " motion=" << motion;
    Log(m.str());
}
static void MotionTraceTick(const void* entity) {
    if (g_motionTraceLeft.load(std::memory_order_relaxed) <= 0) return;
    void* ctrl = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        ctrl = g_lockedPlayerController;
    }
    if (!ctrl || !ModWeaponActive()) return;
    if (entity != static_cast<const void*>(static_cast<std::uint8_t*>(ctrl) - 0x61C8)) return;
    auto* bytes = static_cast<std::uint8_t*>(ctrl);
    if (*reinterpret_cast<volatile std::int32_t*>(bytes + 0xA8) != 2) return;
    const std::int32_t set = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xAC);
    const std::int32_t id = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xB0);
    if (set != 3) return;
    void* inst = ResolveActionInstance(ctrl, set, id);
    if (!inst) return;
    const std::uint32_t motion =
        *reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uint8_t*>(inst) + 0x30);
    if (motion == g_motionTraceLast) return;
    g_motionTraceLast = motion;
    g_motionTraceLeft.fetch_sub(1, std::memory_order_relaxed);
    float mot = -1.0f;
    const auto player = static_cast<std::uint8_t*>(ctrl) - 0x61C8;
    if (IsReadable(player + 0x468, sizeof(void*))) {
        void* anim = *reinterpret_cast<void* const*>(player + 0x468);
        if (anim && IsReadable(static_cast<std::uint8_t*>(anim) + 0x10C, 4)) {
            mot = *reinterpret_cast<volatile float*>(static_cast<std::uint8_t*>(anim) + 0x10C);
        }
    }
    std::ostringstream m;
    m << "MOTIONTRACE act=" << set << ':' << id << " motion=0x" << std::hex << motion
      << std::dec << " (entry " << (motion & 0x1FF) << ") mot=" << mot;
    Log(m.str());
}
long long __fastcall DmgCalcHook(long long a1, long long a2, std::uint32_t* a3) {
    g_damageThisDispatch = true;
    NotifyPlayerHitConnected(a1);
    NotifyGuardDamageHit(a1, a2);
    LogHitFrame(a1);
    long long result = g_originalDmgCalc(a1, a2, a3);
    SheathArmorHitCheck(a1, a2);
    return result;
}

// Armor-window hit reaction (arule): a monster hit CONNECTED while the player
// is inside a watched sheathe action (the super-armor tail of LMT306; the
// i-frame head self-protects this rule from early hits). Clamps HP so the
// hit cannot kill (design: eat the damage, survive with 1), then fires the
// configured switch via the counter-window + force path. HP chain pinned in
// the decompile: FUN_141f62320 = mov rcx,[rcx+0x7630]; jmp FUN_141216910 =
// movss xmm0,[rcx+0x64] -> HP = *(float*)(*(player+0x7630)+0x64)
// (+0x60 in the same object reads as the max-HP getter twin, unverified).
void OpenCounterWindow(const char* reason);
void SheathArmorHitCheck(long long attacker, long long defenderCtx) {
    bool hasArmorRule = false;
    for (const auto& rule : g_sheathRules) {
        if (rule.armorBased) {
            hasArmorRule = true;
            break;
        }
    }
    if (!hasArmorRule) {
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
    const auto localPlayer =
        reinterpret_cast<long long>(static_cast<std::uint8_t*>(controller) - 0x61C8);
    if (attacker == localPlayer) {
        return; // player-dealt hit, not received
    }
    ActionInfo cur;
    bool haveCur = false;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction) {
            cur = *g_lastLongSwordAction;
            haveCur = true;
        }
    }
    if (!haveCur || cur.actionSet != 3) {
        return;
    }
    const std::uint32_t id = cur.actionId;
    SheathCounterRule* match = nullptr;
    for (auto& rule : g_sheathRules) {
        if (rule.armorBased && SheathIdsContain(rule.duringIds, id)) {
            match = &rule;
            break;
        }
    }
    if (!match) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    // Pin the defender field for future tightening: log a2 vs player once.
    static std::atomic_uint32_t armorDefLogs{0};
    if (armorDefLogs.load() < 8) {
        armorDefLogs.fetch_add(1);
        std::ostringstream m;
        m << "SHEADRULE hit a2=0x" << std::hex << std::uppercase
          << static_cast<std::uintptr_t>(defenderCtx)
          << " player=0x" << static_cast<std::uintptr_t>(localPlayer)
          << " (a2==player => defender field pinned) act=3:" << std::dec << id;
        Log(m.str());
    }
    // No-death clamp: survive with 1 HP. Only trusts an explicitly
    // configured HP slot (defaults = the sharpness box, see vitals.txt).
    if (g_vitalsHpSlotCustom.load(std::memory_order_relaxed) &&
        IsReadable(player + g_vitalsHpSlot.load(), sizeof(void*))) {
        auto* vitals =
            *reinterpret_cast<std::uint8_t**>(player + g_vitalsHpSlot.load());
        if (vitals && IsReadable(vitals + 0x64, sizeof(float))) {
            float hp = *reinterpret_cast<const float*>(vitals + 0x64);
            if (hp < 1.0f) {
                *reinterpret_cast<float*>(vitals + 0x64) = 1.0f;
                std::ostringstream m;
                m << "SHEADRULE: lethal hit during 3:" << id
                  << " - HP clamped to 1 (no-death)";
                Log(m.str());
            }
        }
    }
    // One interrupt per hold instance (fired/lastActId, re-armed when the
    // action id changes). g_clangLatch must NOT gate this: it only re-arms
    // on 86/101/102, so a second hold after one hit would stay blocked.
    if (match->fired && id == match->lastActId) {
        return;
    }
    match->fired = true;
    match->lastActId = id;
    g_forceSet = match->toSet;
    g_forceId = match->toId;
    std::ostringstream m;
    m << "SHEADRULE: armor-window hit during 3:" << id << " - force "
      << match->toSet << ':' << match->toId;
    Log(m.str());
    OpenCounterWindow("sheath arule");
    if (g_counterAutoForce && !g_forceArmed.exchange(true)) {
        Log("SHEADRULE: auto-force armed");
    }
}

// --- Sheathe i-frame dispatch monitor + drule trigger ------------------------
// FUN_141f71960 routes every hit attempt; 0xD returns are the per-frame
// ambient checks. Evidence so far: 见切/居合 negated hits arrive here as type-6
// with a non-0xD code and no damage-calc entry (arbitration-level deflect),
// while roll i-frames produce ZERO type-6 traffic (query-level exclusion).
// The TIML b26 windows on 98/99/100 behave like rolls in-game (no 叮), but
// whether ANY type-6 traffic exists for them is exactly what SHEATHDISPATCH
// logging answers: if negated hits during the window show up here, the drule
// gives the switch-on-hit for pure i-frames; if the log stays empty, the
// exclusion is upstream of the dispatcher and the armor-window route is the
// fallback.
using HitDispatchFn = std::int32_t* (__fastcall*)(std::int64_t*, std::int64_t*);
HitDispatchFn g_originalHitDispatch = nullptr;
void OpenCounterWindow(const char* reason);

std::int32_t* __fastcall SheathDispatchHook(std::int64_t* a1, std::int64_t* a2) {
    // Info only: mirror the dispatcher's interaction-type read (vtable+8).
    // NOTE: an earlier build gated all logging on this == 6 and produced zero
    // output across sessions (the read itself never yields 6 - the old
    // "[log only]"-era code that verified clang=non-0xD had no such gate).
    // Log semantics back to the verified form: non-0xD return = real
    // interaction attempt; 0xD = the per-frame ambient flood (excluded).
    long long interactType = -1;
    if (a2 && IsReadable(a2, sizeof(void*))) {
        void* obj = *reinterpret_cast<void**>(a2);
        if (obj && IsReadable(obj, 0x10)) {
            void* vtbl = *reinterpret_cast<void**>(obj);
            if (IsReadable(reinterpret_cast<char*>(vtbl) + 8, sizeof(void*))) {
                using TypeGetterFn = long long (__fastcall*)(std::int64_t*);
                auto fn = *reinterpret_cast<TypeGetterFn*>(
                    reinterpret_cast<char*>(vtbl) + 8);
                if (IsReadable(reinterpret_cast<const void*>(fn), 1)) {
                    interactType = fn(a2);
                }
            }
        }
    }
    g_damageThisDispatch = false;
    auto* result = g_originalHitDispatch(a1, a2);
    const auto code = reinterpret_cast<std::uintptr_t>(result);
    if (code == 0xD) {
        return result; // ambient per-frame flood
    }
    const bool rejected = !g_damageThisDispatch;
    ActionInfo cur;
    bool haveCur = false;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction) {
            cur = *g_lastLongSwordAction;
            haveCur = true;
        }
    }
    if (!haveCur || cur.actionSet != 3) {
        return result;
    }
    const std::uint32_t id = cur.actionId;
    bool watched = SheathIdsContain(g_sheathMineIds, id) ||
                   id == 86; // 见切 = known-clang calibration control
    for (const auto& rule : g_sheathRules) {
        if (rule.dispatchBased && SheathIdsContain(rule.duringIds, id)) {
            watched = true;
            break;
        }
    }
    if (!watched) {
        return result;
    }
    if (g_sheathMineLogCount.load() < 400) {
        ++g_sheathMineLogCount;
        std::ostringstream m;
        m << "SHEATHDISPATCH" << std::hex << std::uppercase
          << " code=0x" << code << " type=" << interactType << std::dec
          << " dmg=" << (g_damageThisDispatch ? 1 : 0)
          << (rejected ? " REJECTED" : " DAMAGE") << " act=3:" << id;
        Log(m.str());
    }
    if (!rejected) {
        return result;
    }
    for (auto& rule : g_sheathRules) {
        if (!rule.dispatchBased || !SheathIdsContain(rule.duringIds, id)) {
            continue;
        }
        // Per-instance latch (g_clangLatch only re-arms on 86/101/102 and
        // would dead-stop every later frontal hit).
        if (rule.fired && id == rule.lastActId) {
            break;
        }
        rule.fired = true;
        rule.lastActId = id;
        g_forceSet = rule.toSet;
        g_forceId = rule.toId;
        // Swallow the engine's {3,37} guard-reaction load briefly so the
        // hold does not flicker through 37 before the force lands.
        g_sheathReassertSet.store(
            static_cast<std::int32_t>(rule.toSet), std::memory_order_relaxed);
        g_sheathReassertId.store(
            static_cast<std::int32_t>(rule.toId), std::memory_order_relaxed);
        g_sheathReassertUntilMs.store(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                    .count() + 1200,
            std::memory_order_relaxed);
        std::ostringstream m;
        m << "SHEATHDRULE: dispatch-rejected hit during 3:" << id
          << " - force " << rule.toSet << ':' << rule.toId;
        Log(m.str());
        OpenCounterWindow("sheath drule");
        if (g_counterAutoForce && !g_forceArmed.exchange(true)) {
            Log("SHEATHDRULE: auto-force armed");
        }
        break;
    }
    return result;
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
    if (isForesightSuccess && result != 0 && ModWeaponActive() &&
        g_counterAutoForce && !g_clangLatch) {
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


// MOTIONCOND (mod): conditional motion swap for one action (valor design:
// qi-blade-3 action 68 -> LMT 142 with W held, 141 without, vanilla 108
// unless the spirit gauge is red). While the watched action is fresh
// (frame < 0.2 s) the instance motion id (+0x30) is rewritten to
// entry|0xC000 and the motion restarts via player vtable+0x288 (the same
// 3-arg call the native mid-motion swap helpers use).
struct MotionBind {
    std::uint32_t actionId = 0; // the action always plays this LMT entry
    std::uint32_t entry = 0;
    bool swapped = false;
    std::uint32_t lastActId = 0;
    std::uint32_t missTicks = 0; // transient id-read glitches must not re-arm
    float prevFrame = 0.0f;      // backwards jump = the action re-began
};
std::vector<MotionBind> g_motionBinds;

#ifndef VALOR_PROBE
// SHEATHEMOTION: patch the 见切 (86) action-class factory motion id so the
// vanilla foresight flow plays the custom sheathe motion. The deflect/arm/
// FSM machinery is code-side (per-action-class) and stays 100% vanilla -
// only the skeleton changes. Class table: LS set-3 (RVA 0x2EF5C30, 113
// entries, stride 0x20, {u32 id, u32 set, void* classPtr, pad}); the class
// factories (class vtable+8/+0x10) embed the motion id immediate
// entry|0xC000 (86 -> 0xC08C, direct hit verified 2026-09-30).
// Config nativePC/plugins/ValorGauge.motion-swap.txt "<fromEntry> <toEntry>"
// (missing = off; e.g. "140 306").
static void ApplySheatheMotionPatch() {
    std::ifstream input("nativePC/plugins/ValorGauge.motion-swap.txt");
    std::string token;
    unsigned long fromEntry = 0, toEntry = 0;
    bool patchMotion = false;
    if (!(input >> token)) {
        Log("Sheathe motion swap: no ValorGauge.motion-swap.txt - off");
        return;
    }
    if (token != "cond") {
        // factory-patch form: "<fromEntry> <toEntry>" first line; "bind ..."
        // lines (and cond-only files) skip the factory patch.
        fromEntry = std::strtoul(token.c_str(), nullptr, 10);
        if ((input >> toEntry) && fromEntry < 0x200 && toEntry < 0x200 &&
            fromEntry != toEntry) {
            patchMotion = true;
        } else {
            Log("Sheathe motion swap: bad first pair - factory patch off");
        }
        input >> token; // optional "cond" keyword
    }
    while (input >> token) {
        if (token != "bind") {
            continue;
        }
        unsigned long actionId = 0, entry = 0;
        if (input >> actionId >> entry && actionId < 0x200 && entry < 0x200) {
            MotionBind bind;
            bind.actionId = static_cast<std::uint32_t>(actionId);
            bind.entry = static_cast<std::uint32_t>(entry);
            g_motionBinds.push_back(bind);
        }
    }
    if (!patchMotion && g_motionBinds.empty()) {
        Log("Sheathe motion swap: nothing configured - off");
        return;
    }
    const auto base = g_moduleBase.load(std::memory_order_relaxed);
    int patchCount = 0;
    if (patchMotion) {
        const std::uint32_t fromMotion =
            (static_cast<std::uint32_t>(fromEntry) & 0x1FF) | 0xC000;
        const std::uint32_t toMotion =
            (static_cast<std::uint32_t>(toEntry) & 0x1FF) | 0xC000;
        const auto* table = reinterpret_cast<const std::uint8_t*>(base + 0x2EF5C30);
        if (!IsReadable(table, 113 * 0x20)) {
            Log("Sheathe motion swap: class table unreadable");
            return;
        }
        for (int i = 0; i < 113; ++i) {
            const auto* entry = table + i * 0x20;
            std::uint32_t id = 0;
            std::memcpy(&id, entry, 4);
            void* classPtr = nullptr;
            std::memcpy(&classPtr, entry + 8, sizeof(void*));
            if (!classPtr || !IsReadable(classPtr, sizeof(void*))) {
                continue;
            }
            void* vtbl = *reinterpret_cast<void**>(classPtr);
            if (!vtbl || !IsReadable(reinterpret_cast<const std::uint8_t*>(vtbl) + 0x18,
                                     sizeof(void*) * 2)) {
                continue;
            }
            for (int fi = 1; fi <= 2; ++fi) {
                void* factory = nullptr;
                std::memcpy(&factory,
                            reinterpret_cast<const std::uint8_t*>(vtbl) + fi * 8,
                            sizeof(void*));
                if (!factory || !IsReadable(factory, 0x140)) {
                    continue;
                }
                for (std::size_t off = 0; off + 4 <= 0x140; off += 4) {
                    std::uint32_t imm = 0;
                    std::memcpy(&imm,
                                reinterpret_cast<const std::uint8_t*>(factory) + off, 4);
                    if (imm != fromMotion) {
                        continue;
                    }
                    void* target = reinterpret_cast<std::uint8_t*>(factory) + off;
                    DWORD oldProtect = 0;
                    if (!VirtualProtect(target, 4, PAGE_EXECUTE_READWRITE,
                                        &oldProtect)) {
                        Log("Sheathe motion swap: VirtualProtect failed");
                        continue;
                    }
                    std::memcpy(target, &toMotion, 4);
                    DWORD restored = 0;
                    VirtualProtect(target, 4, oldProtect, &restored);
                    FlushInstructionCache(GetCurrentProcess(), target, 4);
                    ++patchCount;
                    std::ostringstream m;
                    m << "SHEATHEMOTION: class id=" << id << " factory" << fi
                      << " +0x" << std::hex << off << ": motion 0x" << fromMotion
                      << " -> 0x" << toMotion << std::dec;
                    Log(m.str());
                }
            }
        }
    }
    std::ostringstream m;
    m << "Sheathe motion swap: " << patchCount << " immediate(s) patched";
    if (patchMotion) {
        m << " (" << fromEntry << " -> " << toEntry << ")";
    }
    for (const auto& bind : g_motionBinds) {
        m << "; bind " << bind.actionId << "->" << bind.entry;
    }
    Log(m.str());
}
#endif // !VALOR_PROBE


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
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkHitDispatch]),
                      reinterpret_cast<void*>(&SheathDispatchHook),
                      reinterpret_cast<void**>(&g_originalHitDispatch)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkHitDispatch])) != MH_OK) {
        Log("Hit dispatcher hook failed (sheath dispatch monitor inactive)");
    } else {
        Log("Hit dispatcher hook installed (SHEATHDISPATCH monitoring)");
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
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkNegrEvent]),
                      reinterpret_cast<void*>(&NegrEventHook),
                      reinterpret_cast<void**>(&g_originalNegrEvent)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkNegrEvent])) != MH_OK) {
        Log("Negation event hook failed");
    } else {
        Log("Negation event hook installed (NEGREVT logging)");
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkTurnSet]),
                      reinterpret_cast<void*>(&GuardTurnSetHook),
                      reinterpret_cast<void**>(&g_originalTurnSet)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkTurnSet])) != MH_OK) {
        Log("Turn setter hook failed (GUARD turn clamp inactive)");
    } else {
        Log("Turn setter hook installed (GUARD turn clamp)");
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkHpDelta]),
                      reinterpret_cast<void*>(&GuardHpDeltaHook),
                      reinterpret_cast<void**>(&g_originalHpDelta)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkHpDelta])) != MH_OK) {
        Log("HP delta hook failed (GUARD hit negation inactive)");
    } else {
        Log("HP delta hook installed (GUARD hit negation)");
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkStamDelta]),
                      reinterpret_cast<void*>(&GuardStamDeltaHook),
                      reinterpret_cast<void**>(&g_originalStamDelta)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkStamDelta])) != MH_OK) {
        Log("Stamina delta hook failed (GUARD stamina refund inactive)");
    } else {
        Log("Stamina delta hook installed (GUARD stamina refund)");
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkVitSet]),
                      reinterpret_cast<void*>(&GuardVitSetHook),
                      reinterpret_cast<void**>(&g_originalVitSet)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkVitSet])) != MH_OK) {
        Log("Vitals setter hook failed (VITSET trace inactive)");
    } else {
        Log("Vitals setter hook installed (VITSET trace)");
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkVitAdd]),
                      reinterpret_cast<void*>(&GuardVitAddHook),
                      reinterpret_cast<void**>(&g_originalVitAdd)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkVitAdd])) != MH_OK) {
        Log("Vitals add hook failed (VITADD trace inactive)");
    } else {
        Log("Vitals add hook installed (VITADD trace)");
    }
#endif
#ifndef VALOR_PROBE
    ApplySheatheMotionPatch();
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
// --- Frame seek / frame watch ------------------------------------------------
// Action instances are preallocated per (set,id) under the action controller
// at player+0x61C8: instArray = *(ctrl + 0x68 + set*0x10), instance =
// *(instArray + id*8). instance+8 (float) is the ACTION FRAME: zeroed at
// begin (FUN_14026a1e0), advanced by the per-frame dt (instance+0xC) in the
// controller updaters (FUN_14026a820 / FUN_141f78600, "frame += dt"). Motion
// sampling, TIML event windows and the auto-end threshold (ctrl+0x6A8) all
// key off the controller-visible timeline this counter drives. Writing it
// right after begin = "play this action from frame N" (TIML events between 0
// and N are skipped - they are edge-triggered on frame crossing).
void* ResolveActionInstance(void* ctrl, std::int32_t set, std::int32_t id) {
    if (set < 0 || set >= 4 || id < 0) {
        return nullptr;
    }
    auto* bytes = static_cast<std::uint8_t*>(ctrl);
    void* instArray =
        *reinterpret_cast<void* const*>(bytes + 0x68 + static_cast<std::uintptr_t>(set) * 0x10);
    if (!instArray ||
        !IsReadable(instArray, (static_cast<std::size_t>(id) + 1) * sizeof(void*))) {
        return nullptr;
    }
    void* inst = *reinterpret_cast<void* const*>(
        static_cast<std::uint8_t*>(instArray) + static_cast<std::size_t>(id) * sizeof(void*));
    if (!inst || !IsReadable(inst, 0x20)) {
        return nullptr;
    }
    return inst;
}

void ArmFrameSeekDiagnostics() {
    {
        std::scoped_lock lock(g_actionMutex);
        g_frameWatchSet = static_cast<std::int32_t>(g_forceSet);
        g_frameWatchId = static_cast<std::int32_t>(g_forceId);
    }
    g_frameWatchTicksLeft = 2000;
    g_frameTickLogsLeft = 24;
    g_frameWatchSeen = false;
    g_frameLastLogged = -1.0f;
    g_frameLastDt = -1.0f;
    g_seekFired = false;
    g_seekWrites = 0;
    ClkResetRegions();
    g_frameWatchArmed.store(true, std::memory_order_release);
    if (g_seekFrame >= 0.0f) {
        g_seekArmed.store(true, std::memory_order_release);
    }
}

// Runs on the property-getter hook (game thread, every tick). Watches the
// forced action's frame counter; when seek is configured and the action has
// actually begun (controller phase 2 = begin ran, frame restarted from 0),
// writes the start frame once.
// Track floats whose per-tick delta stays at a constant step: clocks (+),
// countdowns (-), frame counters. A region whose base pointer changes (object
// swap at action start) restarts its tracking. Offsets whose step wobbles get
// their hit counter reset.
void MineClocks(ClkRegion& r, const void* base) {
    if (!base || !IsReadable(base, r.size)) {
        r.lastBase = nullptr;
        return;
    }
    if (r.lastBase != base) {
        r.lastBase = base;
        std::fill(r.valid.begin(), r.valid.end(), std::uint8_t{0});
    }
    const std::uint32_t n = r.size / 4;
    auto* p = static_cast<const float*>(base);
    for (std::uint32_t i = 0; i < n; ++i) {
        const float v = p[i];
        if (!r.valid[i]) {
            r.valid[i] = 1;
            r.prev[i] = v;
            r.step[i] = 0.0f;
            continue;
        }
        const float d = v - r.prev[i];
        r.prev[i] = v;
        if (d == 0.0f) {
            continue;
        }
        const float ref = r.step[i];
        if (ref == 0.0f) {
            if (v == 0.0f || (d < 0.005f && d > -0.005f)) {
                continue; // noise floor: skip sub-tick drift (gauge decay etc.)
            }
            r.step[i] = d;
            r.hits[i] = 1;
            continue;
        }
        float tol = ref * 0.12f;
        if (tol < 0.0f) {
            tol = -tol;
        }
        if (tol < 0.002f) {
            tol = 0.002f;
        }
        if (d <= ref + tol && d >= ref - tol) {
            if (r.hits[i] < 255) {
                ++r.hits[i];
            }
        } else {
            r.step[i] = 0.0f;
            r.hits[i] = 0;
        }
    }
}

void TryFrameSeek(const void* entity) {
    if (!g_frameWatchArmed.load(std::memory_order_acquire)) {
        return;
    }
    void* ctrl = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        ctrl = g_lockedPlayerController;
    }
    if (!ctrl || !ModWeaponActive()) {
        return;
    }
    const void* player = static_cast<const std::uint8_t*>(ctrl) - 0x61C8;
    if (entity != player) {
        return;
    }
    auto* bytes = static_cast<std::uint8_t*>(ctrl);
    const std::int32_t phase = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xA8);
    const std::int32_t set = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xAC);
    const std::int32_t id = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xB0);

    if (g_frameWatchTicksLeft > 0) {
        --g_frameWatchTicksLeft;
    } else {
        g_frameWatchArmed.store(false, std::memory_order_release);
        Log(g_frameWatchSeen ? "FRAMEWATCH done"
                             : "FRAMEWATCH timeout (action never became current)");
        return;
    }
    if (set != g_frameWatchSet || id != g_frameWatchId) {
        if (g_seekArmed.load(std::memory_order_relaxed) && g_seekFired) {
            g_seekArmed.store(false, std::memory_order_release);
            Log("FRAMESEEK abandoned (action changed before settling)");
        }
        return;
    }
    g_frameWatchSeen = true;
    void* inst = ResolveActionInstance(ctrl, set, id);
    if (!inst) {
        return;
    }
    auto* instBytes = static_cast<std::uint8_t*>(inst);
    const float frame = *reinterpret_cast<volatile float*>(instBytes + 8);
    const float dt = *reinterpret_cast<volatile float*>(instBytes + 0xC);
    // The property getter fires ~10x per game frame; only log when the value
    // actually moves so the 24-line budget covers the whole action instead of
    // two frames. 2026-09-26 first run: dt = 1/30 -> the frame field counts
    // SECONDS (30Hz logic tick); TIML frames are 60/s, so TIML n = n/60 sec.
    auto* playerBytes = static_cast<std::uint8_t*>(ctrl) - 0x61C8;
    float motTime = -1.0f;
    void* anim = nullptr;
    if (IsReadable(playerBytes + 0x468, sizeof(void*))) {
        anim = *reinterpret_cast<void* const*>(playerBytes + 0x468);
        if (anim && IsReadable(static_cast<std::uint8_t*>(anim) + 0x10C, 4)) {
            motTime = *reinterpret_cast<volatile float*>(static_cast<std::uint8_t*>(anim) + 0x10C);
        }
    }
    if (g_frameTickLogsLeft > 0 && (frame != g_frameLastLogged || dt != g_frameLastDt)) {
        --g_frameTickLogsLeft;
        g_frameLastLogged = frame;
        g_frameLastDt = dt;
        const float endAt = *reinterpret_cast<volatile float*>(bytes + 0x6A8);
        std::ostringstream m;
        m << "FRAMETICK act=" << set << ':' << id << " phase=" << phase
          << " frame=" << frame << " dt=" << dt << " endAt=" << endAt
          << " mot=" << motTime;
        Log(m.str());
    }
    // Clock mining (until reported): resolve every candidate region each tick,
    // but only ONE mining pass per logic tick - property ticks fire ~10x per
    // game frame, and v2's report fired at "tick 28" after only ~3 frames, so
    // nothing had enough consistent-step hits to clear the threshold.
    if (!g_clkReported && g_clkTicks < 40 && frame != g_clkLastFrame) {
        g_clkLastFrame = frame;
        ++g_clkTicks;
        auto* playerBytes2 = static_cast<std::uint8_t*>(ctrl) - 0x61C8;
        const auto deref = [&](std::uintptr_t where) -> const void* {
            if (!IsReadable(reinterpret_cast<const void*>(where), sizeof(void*))) {
                return nullptr;
            }
            return *reinterpret_cast<void* const*>(where);
        };
        void* core = nullptr;
        if (IsReadable(instBytes + 0x10, sizeof(void*))) {
            core = *reinterpret_cast<void* const*>(instBytes + 0x10);
        }
        MineClocks(g_clkRegions[0], core);
        MineClocks(g_clkRegions[1], deref(reinterpret_cast<std::uintptr_t>(playerBytes2) + 0x468));
        MineClocks(g_clkRegions[2], deref(reinterpret_cast<std::uintptr_t>(playerBytes2) + 0x470));
        MineClocks(g_clkRegions[3], deref(reinterpret_cast<std::uintptr_t>(playerBytes2) + 0x76B0));
        MineClocks(g_clkRegions[4], playerBytes2 + 0x400);
        MineClocks(g_clkRegions[5], playerBytes2 + 0x1D600);
        MineClocks(g_clkRegions[6], instBytes);
        if (g_clkTicks == 28) {
            g_clkReported = true;
            for (int ri = 0; ri < 7; ++ri) {
                const auto& r = g_clkRegions[ri];
                if (!r.lastBase) {
                    continue;
                }
                std::ostringstream m;
                m << "CLK " << r.name << " base=0x" << std::hex
                  << reinterpret_cast<std::uintptr_t>(r.lastBase) << std::dec << " :";
                int shown = 0;
                for (std::uint32_t i = 0; i < r.size / 4 && shown < 14; ++i) {
                    if (r.hits[i] >= 14) {
                        m << (shown++ ? "," : "") << "+0x" << std::hex << i * 4 << std::dec
                          << " step=" << r.step[i] << " (" << static_cast<int>(r.hits[i])
                          << ')';
                    }
                }
                if (shown) {
                    Log(m.str());
                }
            }
        }
    }
    if (g_seekArmed.load(std::memory_order_relaxed)) {
        // v3: drive BOTH timelines. instance+8 is logic-side only (v2 test:
        // pinning it moved nothing on screen); the skeleton samples the MOTION
        // clock mot = *(float*)(*(player+0x468)+0x10C), which counts TIML
        // frames (+2.0/tick at the 30Hz logic rate, 60/s overall) and resets
        // to 0 on every motion (re)start. Seek target: config seconds -> x60
        // TIML frames. Same fresh-begin detection + pin-until-caught-up as the
        // frame write, symmetric for both fields.
        const float motTarget = g_seekFrame * 60.0f;
        const bool motOk = anim && motTime >= 0.0f;
        const bool framePast = g_seekFired && frame > g_seekFrame + dt * 0.5f;
        const bool motPast = !motOk || (g_seekFired && motTime > motTarget + 0.5f);
        if (framePast && motPast) {
            g_seekArmed.store(false, std::memory_order_release);
            std::ostringstream m;
            m << "FRAMESEEK settled act=" << set << ':' << id << " frame=" << frame
              << " mot=" << motTime << " (target " << g_seekFrame << "s, "
              << g_seekWrites << " writes)";
            Log(m.str());
        } else if (!g_seekFired || frame < g_seekFrame ||
                   (motOk && motTime < motTarget)) {
            if (g_seekWrites > 240) {
                g_seekArmed.store(false, std::memory_order_release);
                Log("FRAMESEEK gave up: action never reached the target");
            } else {
                if (!g_seekFired || frame < g_seekFrame) {
                    *reinterpret_cast<volatile float*>(instBytes + 8) = g_seekFrame;
                }
                if (motOk && (!g_seekFired || motTime < motTarget)) {
                    *reinterpret_cast<volatile float*>(static_cast<std::uint8_t*>(anim) +
                                                       0x10C) = motTarget;
                }
                ++g_seekWrites;
                if (!g_seekFired) {
                    g_seekFired = true;
                    std::ostringstream m;
                    m << "FRAMESEEK act=" << set << ':' << id << " frame " << frame
                      << " -> " << g_seekFrame << " mot " << motTime << " -> "
                      << motTarget << " (dt=" << dt << ')';
                    Log(m.str());
                }
            }
        }
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
    if (ok) {
        ArmFrameSeekDiagnostics();
        // The forced call goes through the trampoline, so the DoActionHook
        // never sees it: without this sync the tracker keeps reporting 见切
        // and every negation watcher stays armed throughout the counter move
        // (observed 2026-09-30: one clang forced 刚1 twice ~0.1s apart, the
        // second one re-triggered by the new motion's own event bit).
        {
            std::scoped_lock lock(g_actionMutex);
            ActionInfo forced{};
            forced.actionSet = static_cast<std::int32_t>(g_forceSet);
            forced.actionId = static_cast<std::int32_t>(g_forceId);
            g_lastLongSwordAction = forced;
            // One force per 见切 action: every trigger source is blocked until
            // the next 86/101/102 entry re-arms the latch (DoActionHook reset).
            g_clangLatch = true;
        }
        // Consume the counter window: a spirit press after the fire must not
        // arm a second force from the same clang.
        g_counterWindowUntil = {};
        std::ostringstream changed;
        changed << "Long Sword action changed (forced): set=" << g_forceSet
                << " id=" << g_forceId;
        Log(changed.str());
    }
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
    } else if (func >= 2 && func <= 5) {
        void* weapon = *reinterpret_cast<void**>(player + 0x76B0);
        if (!weapon || !IsReadable(weapon, 0x2400)) {
            m << " <no weapon>";
        } else {
            auto* wbytes = static_cast<std::uint8_t*>(weapon);
            const std::int32_t wtype =
                *reinterpret_cast<std::int32_t*>(wbytes + 0x9F8);
            auto* flag = wbytes + 0x2398;
            m << " wtype=" << wtype << " arm2398="
              << static_cast<int>(*flag);
            if (wtype != 3) {
                m << " <not long sword, skip>";
            } else if (!g_glowEmit) {
                m << " <glowEmit unresolved>";
            } else if (func == 2) {
                g_glowEmit(weapon);
                m << " -> glowEmit (native gates decide)";
            } else if (func == 3) {
                *flag = 1;
                m << " -> armed (next native emitter call glows)";
            } else if (func == 4) {
                *flag = 1;
                g_glowEmit(weapon);
                m << " -> armed + glowEmit fired";
            } else {
                *flag = 0;
                m << " -> cleared";
            }
        }
    } else {
        m << " <unresolved func>";
    }
    Log(m.str());
}

#ifndef VALOR_PROBE
// MOTIONBIND per-tick: see MotionBind. Runs on the game thread inside the
// property-getter hook (~10x/frame), same cadence as FRAMESEEK. Each bound
// action plays its bound LMT entry unconditionally (the FSM/routing decides
// WHICH action fires; the DLL only guarantees the action<->LMT pairing).
void PollMotionBind(const void* entity) {
    if (g_motionBinds.empty()) {
        return;
    }
    void* ctrl = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        ctrl = g_lockedPlayerController;
    }
    if (!ctrl || !ModWeaponActive()) {
        return;
    }
    if (entity != static_cast<const void*>(
                       static_cast<const std::uint8_t*>(ctrl) - 0x61C8)) {
        return;
    }
    auto* bytes = static_cast<std::uint8_t*>(ctrl);
    const std::int32_t set = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xAC);
    const std::int32_t id = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xB0);
    if (set != 3) {
        return;
    }
    for (auto& bind : g_motionBinds) {
        if (static_cast<std::uint32_t>(id) != bind.actionId) {
            if (++bind.missTicks > 3) {
                bind.swapped = false;
                bind.lastActId = 0;
                bind.missTicks = 0;
            }
            continue;
        }
        bind.missTicks = 0;
        if (bind.swapped) {
            continue;
        }
        if (bind.lastActId != static_cast<std::uint32_t>(id)) {
            bind.lastActId = static_cast<std::uint32_t>(id);
            bind.swapped = false;
        }
        void* inst = ResolveActionInstance(ctrl, set, id);
        if (!inst) {
            continue;
        }
        const float frame =
            *reinterpret_cast<volatile float*>(static_cast<std::uint8_t*>(inst) + 8);
        // A backwards frame jump = the action re-began (chained re-cast):
        // re-arm so the re-cast binds again. Our own restart sets prevFrame
        // = -1 so it does not re-trigger itself.
        if (frame + 0.5f < bind.prevFrame) {
            bind.swapped = false;
        }
        bind.prevFrame = frame;
        if (bind.swapped) {
            continue;
        }
        if (!(frame < 0.2f)) {
            bind.swapped = true; // missed the fresh window, keep vanilla
            std::ostringstream m;
            m << "MOTIONBIND act=3:" << id << " missed fresh window (frame "
              << frame << ") - keeping vanilla motion";
            Log(m.str());
            continue;
        }
        const std::uint32_t motionId = (bind.entry & 0x1FF) | 0xC000;
        *reinterpret_cast<volatile std::uint32_t*>(
            static_cast<std::uint8_t*>(inst) + 0x30) = motionId;
        bool restarted = false;
        auto* playerPtr = bytes - 0x61C8;
        if (IsReadable(playerPtr, sizeof(void*))) {
            auto* vtbl = *reinterpret_cast<void**>(playerPtr);
            auto* slot = reinterpret_cast<std::uint8_t*>(vtbl) + 0x288;
            if (IsReadable(slot, sizeof(void*))) {
                void* fn = *reinterpret_cast<void**>(slot);
                if (fn && IsReadable(fn, 1)) {
                    reinterpret_cast<void(__fastcall*)(void*, std::uint32_t,
                                                       std::uint32_t)>(fn)(
                        playerPtr, motionId, 0);
                    restarted = true;
                }
            }
        }
        bind.swapped = true;
        bind.prevFrame = -1.0f; // the restart below resets the frame
        std::ostringstream m;
        m << "MOTIONBIND act=3:" << id << " -> motion " << bind.entry
          << " (0x" << std::hex << motionId << ")" << std::dec
          << (restarted ? " restarted" : " <no restart>");
        Log(m.str());
    }
}
#endif // !VALOR_PROBE

// --- Begin-time gauge rules (cost / fx) -------------------------------------
// Fired when a move actually BEGINS, not when its DoAction request is
// accepted: combo follow-ups buffer during the PREVIOUS move, so accept-time
// firing would deduct before a hit-cancel can eat the buffered request and
// would play the glow over the old swing. Detection is per-frame (property
// getter hook, game thread): watch the controller's current action and treat
// an id change - or the instance frame restarting under the same id (chained
// re-cast through the buffer) - as one begin.
struct GaugeBeginState {
    std::int32_t set = -1;
    std::int32_t id = -1;
    float frame = -1.0f;
};
GaugeBeginState g_gaugeBegin;  // game thread only (property-getter hook)

void ApplyBeginGaugeRules(std::int32_t actionSet, std::int32_t actionId) {
    auto* weapon = GaugeRuleWeapon();
    if (!weapon) {
        return;
    }
    auto readGauge = [&]() {
        return *reinterpret_cast<const volatile float*>(weapon + 0x2368);
    };
    if (const auto* rule = FindGaugeRule(GaugeRule::Type::Cost, actionSet, actionId)) {
        const float g0 = readGauge();
        g_originalGaugeAdd(weapon, -rule->a);
        // Keep the 刚1 guard tail honest: cost rules call the TRAMPOLINE,
        // which bypasses GaugeAddHook, so without this feed the tail's drift
        // guard would revert our own deduction.
        if (g_gang1Phase.load(std::memory_order_acquire) == 2) {
            const float e = g_gang1Expected.load(std::memory_order_relaxed);
            g_gang1Expected.store(std::clamp(e - rule->a, 0.0f, 1.0f),
                                  std::memory_order_relaxed);
        }
        std::ostringstream m;
        m << "GAUGERULE cost act=" << actionSet << ':' << actionId << " -"
          << rule->a << " gauge=" << g0 << "->" << readGauge();
        Log(m.str());
    }
    if (const auto* rule = FindGaugeRule(GaugeRule::Type::Fx, actionSet, actionId)) {
        auto* player =
            static_cast<std::uint8_t*>(g_lockedPlayerController) - 0x61C8;
        if (IsReadable(player + 0x8808, sizeof(void*))) {
            void* emitter = *reinterpret_cast<void**>(player + 0x8808);
            if (emitter && g_fxNotifyA) {
                g_fxNotifyA(emitter, rule->fxId, 0);
                std::ostringstream m;
                m << "GAUGERULE fx act=" << actionSet << ':' << actionId
                  << " id=0x" << std::hex << rule->fxId << std::dec;
                Log(m.str());
            }
        }
    }
}

// 刚1 gauge accounting state machine (see the block comment at g_gang1Phase).
// Runs on the game thread (property-getter hook). Phase 1 waits for the
// request tracker to leave 112 (the rewritten request is BUFFERED while the
// previous move plays, so the controller current-action reads the old move -
// an immediate settle fired before the move's own cost in the first version).
// Phase 2 guards the settle against the vanilla whiff full-clear, which is a
// direct +0x2368 write that lands AFTER the settle and bypasses every hook.
void PollGang1GaugeScope() {
    const std::int32_t phase = g_gang1Phase.load(std::memory_order_acquire);
    if (phase == 0) {
        return;
    }
    const auto now = static_cast<std::int64_t>(GetTickCount64());
    if (phase == 1) {
        void* ctrl = g_lockedPlayerController;
        if (!ctrl) {
            return;
        }
        auto* bytes = static_cast<std::uint8_t*>(ctrl);
        const std::int32_t curSet =
            *reinterpret_cast<volatile std::int32_t*>(bytes + 0xAC);
        const std::int32_t curId =
            *reinterpret_cast<volatile std::int32_t*>(bytes + 0xB0);
        auto* player = bytes - 0x61C8;
        // Restore trigger 1: the 叮 (event bit 15 rising since arm).
        bool restore = false;
        const char* why = "";
        if (IsReadable(player + 0x8938, sizeof(std::uint32_t)) &&
            (*reinterpret_cast<const volatile std::uint32_t*>(
                 player + 0x8938) &
             0x8000) != 0 &&
            !g_gang1Bit15AtArm.load(std::memory_order_relaxed)) {
            restore = true;
            why = "parry(bit15)";
        }
        // Trigger 2: the move reached frame 56 (TIML frames, 60/s) - use the
        // 112 instance's own frame counter so a mid-move cancel freezes it.
        if (!restore && curSet == 3 && curId == 112) {
            void* inst = ResolveActionInstance(ctrl, 3, 112);
            if (inst) {
                const float frame = *reinterpret_cast<volatile float*>(
                    static_cast<std::uint8_t*>(inst) + 8);
                if (frame >= kGang1RestoreFrame / 60.0f) {
                    restore = true;
                    why = "frame56";
                }
            }
        }
        // Trigger 3: a DIFFERENT set-3 action is current - 112 ended or was
        // canceled. (During the initial buffer the controller still shows the
        // pre-rewrite move, set 1 - no fire.) The move came out, cost owed.
        if (!restore && curSet == 3 && curId != 112) {
            restore = true;
            why = "left112";
        }
        // Trigger 4: deadline backstop.
        if (!restore &&
            now > g_gang1DeadlineMs.load(std::memory_order_relaxed)) {
            restore = true;
            why = "deadline";
        }
        if (!restore) {
            return;
        }
        if (!IsReadable(player + 0x76B0, sizeof(void*))) {
            return;
        }
        auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
        if (!weapon || !IsWritable(weapon + 0x2368, sizeof(float))) {
            return;
        }
        const float snapshot = g_gang1Snapshot.load(std::memory_order_relaxed);
        const float target = std::clamp(snapshot - kGang1GaugeCost, 0.0f, 1.0f);
        auto* gauge = reinterpret_cast<volatile float*>(weapon + 0x2368);
        const float before = *gauge;
        *gauge = target;
        g_gang1Expected.store(target, std::memory_order_relaxed);
        g_gang1DeadlineMs.store(now + 1000, std::memory_order_relaxed);
        g_gang1Phase.store(2, std::memory_order_release);
        std::ostringstream m;
        m << "GAUGERULE gang1 settle[" << why << "]: gauge " << before << " -> "
          << target << " (snapshot " << snapshot << " - " << kGang1GaugeCost
          << ')';
        Log(m.str());
        return;
    }
    // Phase 2: guard tail - revert direct writes that bypass FUN_142122410.
    auto* player = static_cast<std::uint8_t*>(g_lockedPlayerController) -
                   0x61C8;
    if (!g_lockedPlayerController ||
        !IsReadable(player + 0x76B0, sizeof(void*))) {
        g_gang1Phase.store(0, std::memory_order_release);
        return;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
    if (!weapon || !IsWritable(weapon + 0x2368, sizeof(float))) {
        g_gang1Phase.store(0, std::memory_order_release);
        return;
    }
    const float expected = g_gang1Expected.load(std::memory_order_relaxed);
    auto* gauge = reinterpret_cast<volatile float*>(weapon + 0x2368);
    const float actual = *gauge;
    if (actual < expected - 0.01f || actual > expected + 0.01f) {
        *gauge = expected;
        static std::atomic_uint32_t guardLogs{0};
        if (guardLogs.fetch_add(1, std::memory_order_relaxed) < 10) {
            std::ostringstream m;
            m << "GAUGERULE gang1 guard: reverted direct write " << actual
              << " -> " << expected;
            Log(m.str());
        }
    }
    if (now > g_gang1DeadlineMs.load(std::memory_order_relaxed)) {
        g_gang1Phase.store(0, std::memory_order_release);
    }
}

// GAUGEWATCH (2026-10-04): per-frame direct-write detector for the visible
// spirit bar (weapon+0x2368). FUN_142122410 events are logged by GAUGEEVT;
// everything ELSE that moves the bar (见切 whiff clears, hit refills, the
// stepped decay) is invisible to it - the user's "打着打着空了" drain was
// exactly such a writer (0.9 -> 0.52 -> 0 with zero event lines). Poll each
// game frame, log any |delta| >= 0.02 with the current action. Game thread.
float g_gaugeWatchLast = -1.0f;

void PollGaugeWatch() {
    if (!g_lockedPlayerController || !ModWeaponActive()) {
        return;
    }
    auto* player =
        static_cast<std::uint8_t*>(g_lockedPlayerController) - 0x61C8;
    if (!IsReadable(player + 0x76B0, sizeof(void*))) {
        return;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
    if (!weapon || !IsReadable(weapon + 0x2368, sizeof(float))) {
        return;
    }
    const float cur =
        *reinterpret_cast<const volatile float*>(weapon + 0x2368);
    if (g_gaugeWatchLast < 0.0f) {
        g_gaugeWatchLast = cur;
        return;
    }
    const float delta = cur - g_gaugeWatchLast;
    g_gaugeWatchLast = cur;
    if (delta < 0.02f && delta > -0.02f) {
        return;
    }
    static std::atomic_uint32_t watchLogs{0};
    if (watchLogs.fetch_add(1, std::memory_order_relaxed) >= 200) {
        return;
    }
    std::int32_t actSet = 0;
    std::int32_t actId = 0;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction) {
            actSet = g_lastLongSwordAction->actionSet;
            actId = g_lastLongSwordAction->actionId;
        }
    }
    std::ostringstream m;
    m << "GAUGEWATCH d=" << (delta >= 0.0f ? "+" : "") << delta << " g="
      << (cur - delta) << "->" << cur << " act=" << actSet << ':' << actId;
    Log(m.str());
}

void PollGaugeBeginRules(const void* entity) {
    if (g_gaugeRules.empty()) {
        return;
    }
    void* ctrl = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        ctrl = g_lockedPlayerController;
    }
    if (!ctrl) {
        g_gaugeBegin = {};
        return;
    }
    if (entity != static_cast<const void*>(
                       static_cast<const std::uint8_t*>(ctrl) - 0x61C8)) {
        return;
    }
    auto* bytes = static_cast<std::uint8_t*>(ctrl);
    const std::int32_t set = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xAC);
    const std::int32_t id = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xB0);
    float frame = -1.0f;
    if (set == 3) {
        if (void* inst = ResolveActionInstance(ctrl, set, id)) {
            frame = *reinterpret_cast<volatile float*>(
                static_cast<std::uint8_t*>(inst) + 8);
        }
    }
    bool freshBegin = false;
    if (set != g_gaugeBegin.set || id != g_gaugeBegin.id) {
        freshBegin = set == 3;
    } else if (set == 3 && g_gaugeBegin.frame >= 0.0f &&
               frame + 0.5f < g_gaugeBegin.frame) {
        freshBegin = true;  // same-id re-cast: begin restarted the frame counter
    }
    g_gaugeBegin.set = set;
    g_gaugeBegin.id = id;
    g_gaugeBegin.frame = frame;
    if (freshBegin) {
        ApplyBeginGaugeRules(set, id);
    }
}

// TRAIL rule per-tick (2026-10-21): hold weapon+0x2398 = 1 while a configured
// action is the current one. The flag means "this move's spirit gauge cost was
// paid" — it gates the generic red-glow emitter (FUN_1421224d0) and the
// per-frame weapon trail; native arming only happens in the kijin move begin
// handlers (cost>0 && gauge pays), so custom/imported moves never arm it.
void PollTrailRules(const void* entity) {
    if (g_gaugeRules.empty()) {
        return;
    }
    const GaugeRule* rule = nullptr;
    void* ctrl = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction) {
            rule = FindGaugeRule(GaugeRule::Type::Trail,
                                 g_lastLongSwordAction->actionSet,
                                 g_lastLongSwordAction->actionId);
        }
        ctrl = g_lockedPlayerController;
    }
    if (!rule || !ctrl || !ModWeaponActive()) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(ctrl) - 0x61C8;
    if (entity != player) {
        return;
    }
    void* weapon = *reinterpret_cast<void**>(player + 0x76B0);
    if (!weapon || !IsReadable(weapon, 0x2400)) {
        return;
    }
    auto* wbytes = static_cast<std::uint8_t*>(weapon);
    if (*reinterpret_cast<std::int32_t*>(wbytes + 0x9F8) != 3) {
        return;
    }
    if (wbytes[0x2398] == 0) {
        wbytes[0x2398] = 1;
    }
}

// ACTIONSND (2026-10-22): play a custom WAV (winmm PlaySound) at a configured
// TIML frame of a configured action. Timeline = the motion clock (anim+0x10C,
// TIML 60fps frames, resets to 0 on every motion restart): arming happens
// while the clock is still before the cue point, the clock going backwards
// means the same action re-cast (re-arm), and the sound fires only if the
// action is STILL the current one at the cue frame - a knockback/flinch out
// of the move cancels it (无双斩 can be interrupted mid-swing).
// Config nativePC/plugins/ValorGauge.action-sound.txt, one rule per line:
//   <set> <id> <frame60> <wavPath>
// frame60 = TIML frames at 60fps (50 = 5/6 s). Path relative to the game
// root (the DLL's CWD). Missing file = the built-in default (3:108 @ 50, the
// 气刃无双斩 MHGU swoosh; user pinned fsm id 108 2026-10-22 - the gauge-rules
// comment that calls 107 无双斩 is stale) so deploying the wav alone works.
struct ActionSoundRule {
    std::int32_t set = 0;
    std::int32_t id = 0;
    float frame = 0.0f;      // TIML frames (60fps)
    std::string path;
    bool armed = false;      // armed for the current occurrence of the action
    bool fired = false;
    float armMot = 0.0f;     // motion-clock snapshot taken at arm time
};
std::atomic<std::shared_ptr<std::vector<ActionSoundRule>>> g_actionSounds;

// SND_FILENAME|SND_ASYNC: winmm copies the name internally (only SND_MEMORY
// buffers must outlive playback), but a config reload can swap the rules
// vector while the play call runs - hand it a process-lifetime copy instead.
const char* StableActionSoundPath(const std::string& path) {
    static std::mutex mutex;
    static std::unordered_set<std::string> keep;
    std::scoped_lock lock(mutex);
    return keep.insert(path).first->c_str();
}

void LoadActionSoundConfig() {
    auto rules = std::make_shared<std::vector<ActionSoundRule>>();
    bool filePresent = false;
    std::ifstream input("nativePC/plugins/ValorGauge.action-sound.txt");
    std::string line;
    while (std::getline(input, line)) {
        filePresent = true;
        const auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        std::istringstream ss(line);
        ActionSoundRule rule;
        std::string path;
        if (!(ss >> rule.set >> rule.id >> rule.frame >> path)) {
            continue;
        }
        rule.path = path;
        rules->push_back(std::move(rule));
    }
    if (!filePresent) {
        ActionSoundRule def;
        def.set = 3;
        def.id = 108;
        def.frame = 50.0f;
        def.path = "nativePC/plugins/ValorGauge.sounds/swo_fx.wav";
        rules->push_back(def);
    }
    std::ostringstream msg;
    msg << "Action sounds: " << rules->size() << " rule(s)";
    for (const auto& rule : *rules) {
        msg << " [" << rule.set << ':' << rule.id << " @" << rule.frame
            << "f " << rule.path
            << (std::filesystem::exists(rule.path) ? "" : " MISSING") << "]";
    }
    g_actionSounds.store(rules);
    Log(msg.str());
}

void PollActionSounds(const void* entity) {
    const auto rules = g_actionSounds.load();
    if (!rules || rules->empty()) {
        return;
    }
    void* ctrl = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        ctrl = g_lockedPlayerController;
    }
    if (!ctrl || !ModWeaponActive()) {
        return;
    }
    if (entity != static_cast<const void*>(static_cast<std::uint8_t*>(ctrl) - 0x61C8)) {
        return;
    }
    auto* bytes = static_cast<std::uint8_t*>(ctrl);
    const std::int32_t phase = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xA8);
    const std::int32_t set = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xAC);
    const std::int32_t id = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xB0);
    float mot = -1.0f;
    const auto player = static_cast<std::uint8_t*>(ctrl) - 0x61C8;
    if (IsReadable(player + 0x468, sizeof(void*))) {
        void* anim = *reinterpret_cast<void* const*>(player + 0x468);
        if (anim && IsReadable(static_cast<std::uint8_t*>(anim) + 0x10C, 4)) {
            mot = *reinterpret_cast<volatile float*>(static_cast<std::uint8_t*>(anim) + 0x10C);
        }
    }
    for (auto& rule : *rules) {
        const bool matches =
            set == rule.set && id == rule.id && phase == 2 && mot >= 0.0f;
        if (!matches) {
            if (rule.armed && !rule.fired) {
                std::ostringstream m;
                m << "ACTIONSND cancel act=" << rule.set << ':' << rule.id
                  << " left the action at mot=" << mot << " (cue "
                  << rule.frame << "f not reached)";
                Log(m.str());
            }
            rule.armed = false;
            rule.fired = false;
            continue;
        }
        if (!rule.armed) {
            // arm only while the clock is still before the cue so a poll that
            // first observes the action late cannot fire a stale sound
            if (mot <= rule.frame) {
                rule.armed = true;
                rule.fired = false;
                rule.armMot = mot;
            }
            continue;
        }
        if (mot < rule.armMot - 1.0f) {
            // same action re-cast: the motion clock restarted from 0
            rule.armMot = mot;
            rule.fired = false;
        }
        if (!rule.fired && mot >= rule.frame) {
            rule.fired = true;
            PlaySoundA(StableActionSoundPath(rule.path), nullptr,
                       SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
            std::ostringstream m;
            m << "ACTIONSND fire act=" << set << ':' << id << " mot=" << mot
              << " -> " << rule.path;
            Log(m.str());
        }
    }
}

int __fastcall PropIntHook(void* entity, void* id, std::uint32_t index, int defaultValue) {
    TryQueuedForceWrite(entity);
    TryQueuedFx(entity);
    TryFrameSeek(entity);
#ifndef VALOR_PROBE
    PollMotionBind(entity);
    MotionTraceTick(entity);
    PollGaugeBeginRules(entity);
    PollTrailRules(entity);
    PollActionSounds(entity);
    PollGang1GaugeScope();
    PollGaugeWatch();
#endif
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
// "<set> <id> [invulnSec] [startFrame]" (invulnSec: no-hit window injected at
// the clang; startFrame >= 0: play the forced action from that action-clock
// time instead of 0 - the field counts SECONDS at 30Hz, so TIML frame n
// (60/s) = n/60 seconds. Skipped frames drop their TIML events, e.g. 见切's
// f0-f12 parry window). Shift+F6 reloads this file in-game.
void LoadSheathCounterConfig() {
    g_sheathRules.clear();
    g_sheathMineIds.clear();
    g_sheathBlueIds.clear();
    g_motionBinds.clear();
    g_sheathMinePrimed = false;
    g_sheathBlueActId = 0;
    g_sheathBlueFx = false;
    std::ifstream input("nativePC/plugins/ValorGauge.sheath-counter.txt");
    if (!input) {
        Log("Sheath counter: no ValorGauge.sheath-counter.txt - trigger inert");
        return;
    }
    std::string line;
    int ruleCount = 0;
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
        if (kind == "rule") {
            std::string ids;
            unsigned long bit = 0, toSet = 0, toId = 0;
            if (ss >> ids >> bit >> toSet >> toId) {
                SheathCounterRule rule;
                if (ParseIdList(ids, rule.duringIds)) {
                    rule.eventBit = static_cast<std::uint32_t>(bit);
                    rule.toSet = static_cast<std::uint32_t>(toSet);
                    rule.toId = static_cast<std::uint32_t>(toId);
                    rule.prev = false;
                    g_sheathRules.push_back(rule);
                    ++ruleCount;
                } else {
                    Log("Sheath counter: bad id list in rule line - skipped");
                }
            } else {
                Log("Sheath counter: malformed rule line - skipped");
            }
        } else if (kind == "drule") {
            // Dispatch-reject rule: fired from SheathDispatchHook when a
            // type-6 hit dispatch during a watched action finishes with a
            // non-0xD code and no damage processing (a negated hit).
            std::string ids;
            unsigned long toSet = 0, toId = 0;
            if (ss >> ids >> toSet >> toId) {
                SheathCounterRule rule;
                if (ParseIdList(ids, rule.duringIds)) {
                    rule.dispatchBased = true;
                    rule.toSet = static_cast<std::uint32_t>(toSet);
                    rule.toId = static_cast<std::uint32_t>(toId);
                    g_sheathRules.push_back(rule);
                    ++ruleCount;
                } else {
                    Log("Sheath counter: bad id list in drule line - skipped");
                }
            } else {
                Log("Sheath counter: malformed drule line - skipped");
            }
        } else if (kind == "arule") {
            // Armor rule: fired from DmgCalcHook when a monster hit CONNECTS
            // while the player is inside a watched action (the super-armor
            // tail of the sheathe). Also clamps HP to survive with 1.
            std::string ids;
            unsigned long toSet = 0, toId = 0;
            if (ss >> ids >> toSet >> toId) {
                SheathCounterRule rule;
                if (ParseIdList(ids, rule.duringIds)) {
                    rule.armorBased = true;
                    rule.toSet = static_cast<std::uint32_t>(toSet);
                    rule.toId = static_cast<std::uint32_t>(toId);
                    g_sheathRules.push_back(rule);
                    ++ruleCount;
                } else {
                    Log("Sheath counter: bad id list in arule line - skipped");
                }
            } else {
                Log("Sheath counter: malformed arule line - skipped");
            }
        } else if (kind == "krel") {
            // Key-release rule: while inside a watched action, releasing the
            // watched key (down->up edge) forces the switch. Fires the plain
            // armed-force path (no counter window / no no-hit injection -
            // the sheathing follow-up needs no protection), no latch.
            std::string ids;
            unsigned long vk = 0, toSet = 0, toId = 0;
            if (ss >> ids >> vk >> toSet >> toId) {
                SheathCounterRule rule;
                if (ParseIdList(ids, rule.duringIds)) {
                    rule.keyBased = true;
                    rule.vk = static_cast<std::uint32_t>(vk);
                    rule.toSet = static_cast<std::uint32_t>(toSet);
                    rule.toId = static_cast<std::uint32_t>(toId);
                    g_sheathRules.push_back(rule);
                    ++ruleCount;
                } else {
                    Log("Sheath counter: bad id list in krel line - skipped");
                }
            } else {
                Log("Sheath counter: malformed krel line - skipped");
            }
        } else if (kind == "bluesim") {
            std::string ids;
            if (ss >> ids) {
                ParseIdList(ids, g_sheathBlueIds);
            }
        } else if (kind == "mine") {
            std::string ids;
            if (ss >> ids) {
                ParseIdList(ids, g_sheathMineIds);
            }
        }
    }
    std::ostringstream m;
    m << "Sheath counter: " << ruleCount << " rule(s) loaded, mine ids=";
    if (g_sheathMineIds.empty()) {
        m << "(none)";
    } else {
        for (std::size_t i = 0; i < g_sheathMineIds.size(); ++i) {
            if (i) m << ',';
            m << g_sheathMineIds[i];
        }
    }
    m << ", bluesim ids=";
    if (g_sheathBlueIds.empty()) {
        m << "(none)";
    } else {
        for (std::size_t i = 0; i < g_sheathBlueIds.size(); ++i) {
            if (i) m << ',';
            m << g_sheathBlueIds[i];
        }
    }
    Log(m.str());
}

// ==========================================================================
// GUARD: native-guard borrow for the migrated counter moves (刚见切 112 /
// 特殊纳刀 99 + 架势持续 98 by default; 2026-10-11 the counter moved
// 86 -> 112 - the i-frame recipe became TIML guard frames on LMT 345).
// Replaces the all-angle no-hit windows with the engine's own guard
// arbitration, direction-limited:
//   1. While a watched action runs, write weapon+0x20F8 = guard level and the
//      weapon param block (*(player+0x7810)) +0x940/+0x944 = guard arc in
//      degrees. The engine's hit routing then treats in-arc hits as guarded:
//      angle check -> chip damage -> stamina drain -> guard-reaction action
//      {3,35..38} (per-weapon command tables; all 14 weapons carry
//      DAMAGE_GUARD_S/M/L at 36/37/38). Saved values restored on close.
//   2. "Blocked" confirmation = the reaction action itself (GUARDREACT). A
//      direction-angle self-check in DmgCalcHook arms a fallback latch
//      (GUARDARM) in case the engine route stays dead on LS.
//   3. A blocked hit's HP write is negated entirely: FUN_141F518E0 is the
//      single red-bar entry (damage deducts the red bar IN FULL; the yellow
//      "recoverable" portion is only a pending-refill quota in the
//      player+0x1D78 / +0xEBA8 slot arrays) - skipping the call AND clearing
//      the quota slots = no damage, no phantom refill. Stamina drain
//      (FUN_141C49F10) is refunded on the react latch.
//   4. Turn clamp: FUN_141F755C0 detour clamps every action yaw request to
//      startYaw +/- maxTurnDeg (mode 2/0 absolute, mode 1 relative), so the
//      player cannot spin freely during the move.
// Config nativePC/plugins/ValorGauge.guard.txt; file missing = feature OFF.
// (GuardConfig / g_guardCfg / g_guardActive are declared above DoActionHook.)
std::atomic<std::int64_t> g_guardDeadlineMs{0};
std::atomic<float> g_guardStartYaw{0.0f};
std::atomic<bool> g_guardSavesValid{false};
int g_guardSavedLevel = 0;
float g_guardSavedArcMin = 0.0f;
float g_guardSavedArcMax = 0.0f;
std::atomic<std::int64_t> g_guardReactUntilMs{0};
std::atomic<std::int64_t> g_guardArmUntilMs{0};
std::atomic<std::uint32_t> g_guardReactId{0};
// Chip passthrough restore: the engine deducts guard chip damage BEFORE
// queueing the reaction; GUARDREACT then rolls the HP back to this snapshot.
std::atomic<float> g_guardRestoreHp{0.0f};
std::atomic<std::int64_t> g_guardRestoreUntilMs{0};
std::atomic<std::uint32_t> g_guardLogCount{0};
std::atomic<std::uint32_t> g_guardTurnLogCount{0};
std::atomic<std::uint32_t> g_guardPassLogCount{0};

// All three originals return void (epilogues touch no rax: verified on the
// dump - turnSet `add rsp,0x90; pop rbx; ret`, hpDelta `add rsp,0x40; pop
// rbx; ret`, stamDelta `add rsp,0x150; pop rdi; ret`).
using TurnSetFn = void(__fastcall*)(void*, float, float, int, int);
TurnSetFn g_originalTurnSet = nullptr;
using HpDeltaFn = void(__fastcall*)(void*, float);
HpDeltaFn g_originalHpDelta = nullptr;
HpDeltaFn g_originalStamDelta = nullptr;
GuardVitSetFn g_originalVitSet = nullptr;
GuardDeltaFn g_originalVitAdd = nullptr;

// Block-event windows. USER-VALIDATED 2026-10-06: the guard chip damage
// write and the guard-reaction action land within ~1 frame of each other
// ("时间间隔不久,绝对达不到1.2s") - the old 1200/600ms margins only caused
// false immunity for follow-up hits. Both windows = one configurable value.
std::atomic<std::int64_t> g_blockWindowMs{150};
constexpr float kGuardRad2Deg = 57.29577951308232f;

std::int64_t GuardNowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

float GuardWrap180(float deg) {
    while (deg > 180.0f) deg -= 360.0f;
    while (deg < -180.0f) deg += 360.0f;
    return deg;
}

// em+0x170..0x17C quaternion (x,y,z,w), Y-up Z-forward:
// fwd = (2(xz+wy), 2(yz-wx), 1-2(x^2+y^2)); yaw = atan2(fwd.x, fwd.z).
float GuardYawDeg(const void* entity) {
    const auto* em = static_cast<const std::uint8_t*>(entity);
    if (!IsReadable(em + 0x170, 16)) {
        return 0.0f;
    }
    const float x = *reinterpret_cast<const float*>(em + 0x170);
    const float y = *reinterpret_cast<const float*>(em + 0x174);
    const float z = *reinterpret_cast<const float*>(em + 0x178);
    const float w = *reinterpret_cast<const float*>(em + 0x17C);
    const float fwdX = 2.0f * (x * z + w * y);
    const float fwdZ = 1.0f - 2.0f * (x * x + y * y);
    return std::atan2(fwdX, fwdZ) * kGuardRad2Deg;
}

std::uint8_t* GuardWeaponOf(std::uint8_t* player) {
    if (!IsReadable(player + 0x76B0, sizeof(void*))) {
        return nullptr;
    }
    return *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
}

void GuardCloseWindow(const char* reason) {
    if (!g_guardActive.exchange(false)) {
        return;
    }
    g_guardReactUntilMs = 0;
    g_guardArmUntilMs = 0;
    std::ostringstream m;
    m << "GUARDCLOSE (" << reason << ")";
    // Nothing to restore: the window no longer writes any game memory
    // (guard-level/arc writes retired - see GuardOpenWindow note).
    (void)g_guardSavesValid.exchange(false);
    Log(m.str());
}

void GuardOpenWindow(std::uint8_t* player, std::uint32_t actionId) {
    g_guardStartYaw = GuardYawDeg(player);
    g_guardSavesValid = false;
    std::ostringstream m;
    m << "GUARDOPEN act=3:" << actionId
      << " startYaw=" << g_guardStartYaw.load()
      << " arc=+/-" << g_guardCfg.arcHalfDeg
      << " maxTurn=" << g_guardCfg.maxTurnDeg;
    // NOTE: no memory writes beyond our own bookkeeping. The old weapon
    // +0x20F8 "guard level" write turned out to be SHARPNESS (HunterPie:
    // MHWSharpnessStructure), and the param-block +0x940/+0x944 "guard arc"
    // restore logged vanilla values [1200,0] - not degrees either. Native
    // guard routing (3:98 reactions) works without any of these writes;
    // negation happens in GuardVitSetHook off the react/arm latches.
    g_guardDeadlineMs = GuardNowMs() +
        static_cast<std::int64_t>(g_guardCfg.maxWindowSec * 1000.0f);
    g_guardActive = true;
    Log(m.str());
}

void GuardOnActionAccepted(void* controller, std::uint32_t actionId) {
    if (g_guardCfg.ids.empty()) {
        return;
    }
    const bool watched = std::find(g_guardCfg.ids.begin(), g_guardCfg.ids.end(),
                                   actionId) != g_guardCfg.ids.end();
    if (watched) {
        if (g_guardActive.load(std::memory_order_acquire)) {
            // Chained watched move: keep the yaw latch, refresh the deadline.
            g_guardDeadlineMs = GuardNowMs() +
                static_cast<std::int64_t>(g_guardCfg.maxWindowSec * 1000.0f);
            return;
        }
        auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
        GuardOpenWindow(player, actionId);
        return;
    }
    if (g_guardActive.load(std::memory_order_relaxed)) {
        GuardCloseWindow("action change");
    }
}

void GuardOnReactionAction(std::uint32_t actionSet, std::uint32_t actionId) {
    if (actionSet != 3) {
        return;
    }
    if (actionId < 35 || actionId > 38) {
        return;
    }
    g_guardReactId = actionId;
    g_guardReactUntilMs = GuardNowMs() + g_blockWindowMs.load(std::memory_order_relaxed);
    // Roll back the chip damage that the engine applied before routing this
    // reaction (restore snapshot armed by GuardVitSetHook ~same frame).
    const std::int64_t now = GuardNowMs();
    const float restoreHp = g_guardRestoreHp.load(std::memory_order_relaxed);
    if (now < g_guardRestoreUntilMs.load(std::memory_order_relaxed) &&
        restoreHp > 0.0f) {
        g_guardRestoreUntilMs = 0;
        void* controller = g_lockedPlayerController;
        if (controller) {
            auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
            if (auto* box = GuardVitalsBox(player,
                                            g_vitalsHpSlot.load())) {
                float& cur =
                    *reinterpret_cast<float*>(box + 0x64);
                const float before = cur;
                if (cur < restoreHp - 0.01f) {
                    cur = restoreHp;
                    GuardApplyBlockBenefits(player, box);
                    const auto n =
                        g_guardLogCount.fetch_add(1, std::memory_order_relaxed);
                    if (n < 48) {
                        std::ostringstream m;
                        m << "GUARDBLOCK restore " << before << " -> "
                          << restoreHp << " (chip rolled back on reaction)";
                        Log(m.str());
                    }
                }
            }
        }
    }
    const auto n = g_guardLogCount.fetch_add(1, std::memory_order_relaxed);
    if (n < 24) {
        std::ostringstream m;
        m << "GUARDREACT engine guard-reaction 3:" << actionId << " latched";
        Log(m.str());
    }
}

// DmgCalcHook side "frontal?" self-check. a1 = attacker em: position at
// em+0x160..0x168, same layout as the player. Arm the fallback latch when
// the attack came from inside our arc (position-based approximation of the
// engine's facing check; GUARDARM-out lines are the tuning data).
void NotifyGuardDamageHit(long long attacker, long long defenderCtx) {
    (void)defenderCtx;
    if (!g_guardActive.load(std::memory_order_acquire)) {
        return;
    }
    void* controller = g_lockedPlayerController;
    if (!controller) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (attacker == reinterpret_cast<long long>(player)) {
        return;  // player-dealt hit
    }
    auto* attackerEm = reinterpret_cast<const std::uint8_t*>(attacker);
    if (!IsReadable(attackerEm + 0x160, 12) || !IsReadable(player + 0x160, 12)) {
        return;
    }
    const float dx = *reinterpret_cast<const float*>(attackerEm + 0x160) -
                     *reinterpret_cast<const float*>(player + 0x160);
    const float dz = *reinterpret_cast<const float*>(attackerEm + 0x168) -
                     *reinterpret_cast<const float*>(player + 0x168);
    float angDiff = 0.0f;
    if (dx * dx + dz * dz >= 1.0f) {
        const float angleTo = std::atan2(dx, dz) * kGuardRad2Deg;
        angDiff = GuardWrap180(angleTo - GuardYawDeg(player));
    }
    const bool frontal = std::fabs(angDiff) <= g_guardCfg.arcHalfDeg;
    if (frontal && GuardCurrentActionInArmIds()) {
        g_guardArmUntilMs = GuardNowMs() + g_blockWindowMs.load(std::memory_order_relaxed);
    }
    const auto n = g_guardLogCount.fetch_add(1, std::memory_order_relaxed);
    if (n < 40) {
        std::ostringstream m;
        m << "GUARDARM dmg-calc hit " << (frontal ? "FRONTAL" : "outside")
          << " angDiff=" << angDiff << " arcHalf=" << g_guardCfg.arcHalfDeg;
        Log(m.str());
    }
}

void __fastcall GuardHpDeltaHook(void* player, float delta) {
    void* controller = g_lockedPlayerController;
    if (delta < 0.0f && g_guardActive.load(std::memory_order_acquire) &&
        controller &&
        player == static_cast<std::uint8_t*>(controller) - 0x61C8) {
        const std::int64_t now = GuardNowMs();
        const bool arm = g_guardCfg.armFallback && GuardCurrentActionInArmIds() &&
                         now < g_guardArmUntilMs.load(std::memory_order_relaxed);
        const bool lethal =
            delta < 1.0f && GuardCurrentActionInArmIds();
        if (arm || lethal) {
            // Secondary negation point (primary = GuardVitSetHook, where the
            // observed damage actually converges). Skip the call and clear
            // the pending-heal quota array (box+0x30 -> +0xEBB0).
            auto* p = static_cast<std::uint8_t*>(player);
            float hpBefore = -1.0f;
            if (auto* box = GuardVitalsBox(p, g_vitalsHpSlot.load())) {
                hpBefore = *reinterpret_cast<const float*>(box + 0x64);
                GuardClearHealingQuota(box);
            }
            if (g_guardCfg.gaugeRefill) {
                if (auto* weapon = GuardWeaponOf(p)) {
                    if (IsWritable(weapon + 0x2399, 1)) {
                        weapon[0x2399] = 1;  // vanilla 见切-success latch
                    }
                }
            }
            if (g_guardCfg.counterOnBlock) {
                OpenCounterWindow("guard block");
                if (g_counterAutoForce && !g_forceArmed.exchange(true)) {
                    Log("GUARDBLOCK: counter window + force armed");
                }
            }
            const auto n = g_guardLogCount.fetch_add(1, std::memory_order_relaxed);
            if (n < 48) {
                std::ostringstream m;
                m << "GUARDBLOCK negated " << delta << " hp=" << hpBefore
                  << " path=" << (arm ? "arm" : "lethal");
                Log(m.str());
            }
            return;
        }
        const auto n = g_guardPassLogCount.fetch_add(1, std::memory_order_relaxed);
        if (n < 16) {
            std::ostringstream m;
            m << "GUARDPASS window hit passed through (no react/arm latch) delta="
              << delta;
            Log(m.str());
        }
    }
    g_originalHpDelta(player, delta);
}

void __fastcall GuardStamDeltaHook(void* player, float delta) {
    void* controller = g_lockedPlayerController;
    if (delta < 0.0f && g_guardCfg.staminaRefund &&
        g_guardActive.load(std::memory_order_acquire) && controller &&
        player == static_cast<std::uint8_t*>(controller) - 0x61C8 &&
        GuardNowMs() < g_guardReactUntilMs.load(std::memory_order_relaxed)) {
        const auto n = g_guardLogCount.fetch_add(1, std::memory_order_relaxed);
        if (n < 48) {
            std::ostringstream m;
            m << "GUARDSTAM refunded " << delta;
            Log(m.str());
        }
        return;
    }
    g_originalStamDelta(player, delta);
}

void __fastcall GuardTurnSetHook(void* em, float yawDeg, float duration,
                                 int mode, int turnKind) {
    void* controller = g_lockedPlayerController;
    if (g_guardActive.load(std::memory_order_acquire) && controller &&
        em == static_cast<std::uint8_t*>(controller) - 0x61C8) {
        // Scope gate: only clamp while the CURRENT action is a turnIds member.
        // Live controller+0xAC/+0xB0 (player+0x6274/+0x6278), not the tracker
        // snapshot - held stances (98/99) steer per-frame and must stay free.
        const auto* c = static_cast<const std::uint8_t*>(controller);
        if (IsReadable(c + 0xAC, 8)) {
            const std::uint32_t curSet =
                *reinterpret_cast<const std::uint32_t*>(c + 0xAC);
            const std::uint32_t curId =
                *reinterpret_cast<const std::uint32_t*>(c + 0xB0);
            if (curSet != 3 ||
                std::find(g_guardCfg.turnIds.begin(), g_guardCfg.turnIds.end(),
                          curId) == g_guardCfg.turnIds.end()) {
                g_originalTurnSet(em, yawDeg, duration, mode, turnKind);
                return;
            }
        }
        const float startYaw = g_guardStartYaw.load(std::memory_order_relaxed);
        const float maxTurn = g_guardCfg.maxTurnDeg;
        float clamped = yawDeg;
        if (mode == 1) {
            // Relative: the original computes em+0xB220 += yaw - em+0xB224;
            // clamp the resulting absolute target, then back-solve the input.
            const auto* e = static_cast<const std::uint8_t*>(em);
            if (IsReadable(e + 0xB220, 8)) {
                const float target =
                    *reinterpret_cast<const float*>(e + 0xB220);
                const float applied =
                    *reinterpret_cast<const float*>(e + 0xB224);
                const float targetAfter = target + yawDeg - applied;
                float d = GuardWrap180(targetAfter - startYaw);
                d = std::max(-maxTurn, std::min(maxTurn, d));
                clamped = startYaw + d - target + applied;
            }
        } else {
            float d = GuardWrap180(yawDeg - startYaw);
            d = std::max(-maxTurn, std::min(maxTurn, d));
            clamped = startYaw + d;
        }
        if (std::fabs(GuardWrap180(clamped - yawDeg)) > 0.01f) {
            const auto n =
                g_guardTurnLogCount.fetch_add(1, std::memory_order_relaxed);
            if (n < 24) {
                std::ostringstream m;
                m << "TURNCLAMP yaw " << yawDeg << " -> " << clamped
                  << " (mode " << mode << ", start " << startYaw
                  << ", maxTurn " << maxTurn << ")";
                Log(m.str());
            }
        }
        yawDeg = clamped;
    }
    g_originalTurnSet(em, yawDeg, duration, mode, turnKind);
}

// --- Vitals slots (CONFIGURABLE - user-verified 2026-10-05: the +0x7630 box
// is WEAPON SHARPNESS, not HP; the whole "vitals box" cluster derived from
// FUN_141f62320/141f628c0/141f21ea10 belongs to the sharpness system. The
// real HP box slot is unknown until found via VIT trace / x64dbg. Configure
// nativePC/plugins/ValorGauge.vitals.txt = "<hpSlotHex> <stamSlotHex>"
// (defaults 7630/7670). Slot-dependent WRITE paths (chip-quota clearing,
// no-death clamp) only activate once the slot is explicitly overridden. ----
std::atomic<std::size_t> g_vitalsHpSlot{0x7630};
std::atomic<std::size_t> g_vitalsStamSlot{0x7670};
std::atomic<bool> g_vitalsHpSlotCustom{false};

std::uint8_t* GuardVitalsBox(std::uint8_t* player, std::size_t slot) {
    if (!IsReadable(player + slot, sizeof(void*))) {
        return nullptr;
    }
    auto* box = *reinterpret_cast<std::uint8_t**>(player + slot);
    if (box && IsReadable(box + 0x60, 8)) {
        return box;
    }
    return nullptr;
}

void LoadVitalsConfig() {
    std::ifstream input("nativePC/plugins/ValorGauge.vitals.txt");
    std::string hpTok, stamTok;
    std::string line;
    bool any = false;
    while (std::getline(input, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        std::istringstream ss(line);
        std::string key;
        if (!(ss >> key)) {
            continue;
        }
        std::string val;
        if (!(ss >> val)) {
            continue;
        }
        const auto parsed = static_cast<std::size_t>(
            std::strtoull(val.c_str(), nullptr, 0));
        if (key == "hp") {
            g_vitalsHpSlot = parsed;
            any = true;
        } else if (key == "stam") {
            g_vitalsStamSlot = parsed;
            any = true;
        }
    }
    // Slot-dependent writes only trust an explicitly configured slot.
    g_vitalsHpSlotCustom = any;
    std::ostringstream m;
    m << "Vitals slots: hp=+0x" << std::hex << g_vitalsHpSlot.load()
      << " stam=+0x" << g_vitalsStamSlot.load() << std::dec
      << (any ? " (custom - HP writes active)"
              : " (DEFAULTS = sharpness box! HP writes disabled until "
                "ValorGauge.vitals.txt sets the real slot)");
    Log(m.str());
}

// --- Vitals write tracers (VITSET/VITADD) -----------------------------------
// The two low-level box setters every gauge change funnels through:
// FUN_141216940 (set cur, min(v, max)) and FUN_141216880 (cur += delta).
// Shared by ALL entities - observe the local player's configured boxes and,
// for anything else, DISCOVER which player slot (if any) holds the box so
// the real HP slot identifies itself in one session.
std::atomic<std::uint32_t> g_vitHpLogCount{0};
std::atomic<std::uint32_t> g_vitStamLogCount{0};
std::atomic<std::uint32_t> g_vitOtherLogCount{0};

// Classify a vitals box: configured HP/stam slots first, then scan the
// player pointer-slot window (+0x7400..+0x77F0) for a 1-deref match.
// Allocation-free (hot path); fills cap/capLimit for rate limiting.
const char* GuardVitClassify(void* box,
                             std::atomic<std::uint32_t>** capOut,
                             std::uint32_t* capLimitOut,
                             std::size_t* slotOut) {
    *capOut = &g_vitOtherLogCount;
    *capLimitOut = 64;
    *slotOut = 0;
    const char* tag = "ent";
    void* controller = g_lockedPlayerController;
    if (!controller) {
        return tag;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (IsReadable(player + g_vitalsHpSlot.load(std::memory_order_relaxed),
                   sizeof(void*)) &&
        box == *reinterpret_cast<void* const*>(
                   player + g_vitalsHpSlot.load(std::memory_order_relaxed))) {
        *capOut = &g_vitHpLogCount;
        *capLimitOut = 100;
        *slotOut = g_vitalsHpSlot.load(std::memory_order_relaxed);
        return "HP";
    }
    if (IsReadable(player + g_vitalsStamSlot.load(std::memory_order_relaxed),
                   sizeof(void*)) &&
        box == *reinterpret_cast<void* const*>(
                   player + g_vitalsStamSlot.load(std::memory_order_relaxed))) {
        *capOut = &g_vitStamLogCount;
        *capLimitOut = 40;
        *slotOut = g_vitalsStamSlot.load(std::memory_order_relaxed);
        return "STAM";
    }
    for (std::size_t off = 0x7400; off <= 0x77F0; off += 8) {
        if (IsReadable(player + off, sizeof(void*)) &&
            box == *reinterpret_cast<void* const*>(player + off)) {
            *slotOut = off;
            break;
        }
    }
    return tag;
}

// USER-VERIFIED 2026-10-05 (HPWATCH + VIT trace): the local player's HP box
// IS *(player+0x7630): +0x60=max, +0x64=cur; RecoverableHealth at box+0x2DD4;
// pending-heal array at *(box+0x30)+0xEBB0 (4 entries x0x40: Heal@0x10,
// MaxHeal@0x18, Stage@0x2C); Stamina cur/max at box+0x12C/0x130. Damage
// writes route through the low-level setter (FUN_141216940) from unknown
// callers (seen: RVA +0x1218E18), NOT through FUN_141f518e0.
void GuardClearHealingQuota(std::uint8_t* box) {
    if (!IsReadable(box + 0x30, sizeof(void*))) {
        return;
    }
    auto* arr = *reinterpret_cast<std::uint8_t**>(box + 0x30);
    if (!arr || !IsWritable(arr + 0xEBB0, 4 * 0x40)) {
        return;
    }
    for (int i = 0; i < 4; ++i) {
        auto* e = arr + 0xEBB0 + i * 0x40;
        *reinterpret_cast<float*>(e + 0x10) = 0.0f;       // Heal
        *reinterpret_cast<std::int32_t*>(e + 0x2C) = 0;   // Stage
    }
}

// Shared "blocked" benefits: clear the pending-heal quota, top stamina back
// up, optionally refill the spirit gauge (vanilla foresight-success latch).
void GuardApplyBlockBenefits(std::uint8_t* player, std::uint8_t* box) {
    GuardClearHealingQuota(box);
    // Stamina top-up (hud stamina lives at hpBox+0x12C/0x130; the +0x7670
    // box turned out to be [0,0], not stamina).
    if (IsWritable(box + 0x130, 4)) {
        *reinterpret_cast<float*>(box + 0x12C) =
            *reinterpret_cast<const float*>(box + 0x130);
    }
    if (g_guardCfg.gaugeRefill) {
        if (auto* weapon = GuardWeaponOf(player)) {
            if (IsWritable(weapon + 0x2399, 1)) {
                weapon[0x2399] = 1;  // vanilla 见切-success latch
            }
        }
    }
    if (g_guardCfg.counterOnBlock) {
        OpenCounterWindow("guard block");
        if (g_counterAutoForce && !g_forceArmed.exchange(true)) {
            Log("GUARDBLOCK: counter window + force armed");
        }
    }
}

// True when the tracked current action is one of cfg.armIds (empty = all).
bool GuardCurrentActionInArmIds() {
    if (g_guardCfg.armIds.empty()) {
        return true;
    }
    ActionInfo cur;
    {
        std::scoped_lock lock(g_actionMutex);
        if (!g_lastLongSwordAction) {
            return false;
        }
        cur = *g_lastLongSwordAction;
    }
    if (cur.actionSet != 3) {
        return false;
    }
    return std::find(g_guardCfg.armIds.begin(), g_guardCfg.armIds.end(),
                     static_cast<std::uint32_t>(cur.actionId)) !=
           g_guardCfg.armIds.end();
}

void __fastcall GuardVitSetHook(void* box, float value, std::uint8_t floorFlag) {
    auto* b = static_cast<const std::uint8_t*>(box);
    const std::uintptr_t caller =
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()) -
        g_moduleBase.load(std::memory_order_relaxed);
    float prev = -1.0f;
    if (IsReadable(b + 0x64, sizeof(float))) {
        prev = *reinterpret_cast<const float*>(b + 0x64);
    }
    // GUARD negation point: every HP write converges on this setter. The
    // engine applies guard chip damage BEFORE queueing the reaction (live
    // trace), so the react latch can never legitimately cover its own hit -
    // pre-negating on it only granted blanket immunity to FOLLOW-UP hits for
    // 1.2s (the "knocked back outside the move but HP preserved" bug).
    // Negation now requires the current action to be arm-armed (armIds
    // scoped; currently none - 112/98/99 all carry native guard points and
    // rely on the reaction rollback):
    //   - arm latch (directional, from DmgCalcHook) -> drop the write;
    //   - lethal override (would leave <1 HP) -> drop unconditionally.
    //   - everything else passes; GuardOnReactionAction rolls it back when
    //     the engine confirms the block with a guard reaction.
    void* controller = g_lockedPlayerController;
    if (value < prev && prev > 0.0f && g_guardActive.load(std::memory_order_acquire) &&
        controller &&
        box == *reinterpret_cast<void* const*>(
                   static_cast<std::uint8_t*>(controller) - 0x61C8 +
                   g_vitalsHpSlot.load(std::memory_order_relaxed))) {
        const std::int64_t now = GuardNowMs();
        const bool arm = g_guardCfg.armFallback && GuardCurrentActionInArmIds() &&
                         now < g_guardArmUntilMs.load(std::memory_order_relaxed);
        const bool lethal =
            value < 1.0f && GuardCurrentActionInArmIds();
        auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
        if (arm || lethal) {
            GuardApplyBlockBenefits(player, static_cast<std::uint8_t*>(box));
            const auto n = g_guardLogCount.fetch_add(1, std::memory_order_relaxed);
            if (n < 48) {
                std::ostringstream m;
                m << "GUARDBLOCK vitset negated " << prev << " -> " << value
                  << " path=" << (arm ? "arm" : "lethal")
                  << " caller=+0x" << std::hex << caller;
                Log(m.str());
            }
            return;
        }
        // Chip passed through: stash the pre-hit HP for the reaction restore.
        g_guardRestoreHp = prev;
        g_guardRestoreUntilMs = now + g_blockWindowMs.load(std::memory_order_relaxed);
    }
    g_originalVitSet(box, value, floorFlag);
    std::atomic<std::uint32_t>* cap = nullptr;
    std::uint32_t capLimit = 0;
    std::size_t slot = 0;
    const char* tag = GuardVitClassify(box, &cap, &capLimit, &slot);
    if (cap->load(std::memory_order_relaxed) < capLimit) {
        cap->fetch_add(1, std::memory_order_relaxed);
        std::ostringstream m;
        m << "VITSET " << tag << " " << prev << " -> " << value;
        if (slot) {
            m << " slot=+0x" << std::hex << slot;
        } else {
            m << " box=0x" << std::hex
              << reinterpret_cast<std::uintptr_t>(box);
        }
        m << " caller=+0x" << caller
          << " GW=" << (g_guardActive.load(std::memory_order_relaxed) ? 1 : 0);
        Log(m.str());
    }
}

void __fastcall GuardVitAddHook(void* box, float delta) {
    auto* b = static_cast<const std::uint8_t*>(box);
    const std::uintptr_t caller =
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()) -
        g_moduleBase.load(std::memory_order_relaxed);
    float prev = -1.0f;
    if (IsReadable(b + 0x64, sizeof(float))) {
        prev = *reinterpret_cast<const float*>(b + 0x64);
    }
    g_originalVitAdd(box, delta);
    std::atomic<std::uint32_t>* cap = nullptr;
    std::uint32_t capLimit = 0;
    std::size_t slot = 0;
    const char* tag = GuardVitClassify(box, &cap, &capLimit, &slot);
    if (cap->load(std::memory_order_relaxed) < capLimit) {
        cap->fetch_add(1, std::memory_order_relaxed);
        float after = prev;
        if (IsReadable(b + 0x64, sizeof(float))) {
            after = *reinterpret_cast<const float*>(b + 0x64);
        }
        std::ostringstream m;
        m << "VITADD " << tag << " " << delta << " (" << prev << " -> " << after
          << ")";
        if (slot) {
            m << " slot=+0x" << std::hex << slot;
        } else {
            m << " box=0x" << std::hex
              << reinterpret_cast<std::uintptr_t>(box);
        }
        m << " caller=+0x" << caller
          << " GW=" << (g_guardActive.load(std::memory_order_relaxed) ? 1 : 0);
        Log(m.str());
    }
}

// --- HP lock (address verification tool) -------------------------------------
// ValorGauge.hp-lock.txt = "<value>"; >0 forces BOTH box fields (max+cur) to
// value every frame. Set 200: if the HUD shows/keeps 200, the vitals box
// address is right and any damage write that fights the lock is visible in
// the VITSET/VITADD trace. 0 or missing file = off.
std::atomic<float> g_hpLockValue{0.0f};

void LoadHpLockConfig() {
    float v = 0.0f;
    std::ifstream input("nativePC/plugins/ValorGauge.hp-lock.txt");
    if (input >> v) {
        g_hpLockValue = (v > 0.0f && v <= 500.0f) ? v : 0.0f;
    } else {
        g_hpLockValue = 0.0f;
    }
    std::ostringstream m;
    m << "HP lock: " << g_hpLockValue.load()
      << (g_hpLockValue.load() > 0.0f ? " (forced every frame - TEST TOOL)"
                                      : " (off)");
    Log(m.str());
}

void PollHpLock() {
    const float v = g_hpLockValue.load(std::memory_order_relaxed);
    if (v <= 0.0f) {
        return;
    }
    void* controller = g_lockedPlayerController;
    if (!controller) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (auto* box = GuardVitalsBox(player, g_vitalsHpSlot.load())) {
        *reinterpret_cast<float*>(box + 0x60) = v;  // max
        *reinterpret_cast<float*>(box + 0x64) = v;  // cur
    }
}

// --- HPWATCH: one-second candidate dump for the real HP field ---------------
// Conflicting static reads (HunterPie says the *(player+0x7630) container IS
// the HUD structure: MaxHealth@0x60/Health@0x64/Stamina@0x12C/Recoverable@
// 0x2DD4/healing array *(c+0x30)+0xEBB0; the user's hp-lock test on the same
// box moved SHARPNESS). Ground truth = which logged number tracks the HUD
// when a hit lands. Cap-limited so one session answers it.
std::atomic<std::uint32_t> g_hpWatchCount{0};
std::uint32_t g_hpWatchFrame = 0;

void PollHpWatch() {
    if (++g_hpWatchFrame % 90 != 0) {
        return;
    }
    if (g_hpWatchCount.load(std::memory_order_relaxed) >= 400) {
        return;
    }
    void* controller = g_lockedPlayerController;
    if (!controller) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    std::ostringstream m;
    m << "HPWATCH";
    // HunterPie HUD structure = the *(player+0x7630) container itself.
    if (IsReadable(player + 0x7630, sizeof(void*))) {
        auto* c = *reinterpret_cast<std::uint8_t**>(player + 0x7630);
        if (c) {
            if (IsReadable(c + 0x64, sizeof(float))) {
                m << " hpBox=["
                  << *reinterpret_cast<const float*>(c + 0x60) << ","
                  << *reinterpret_cast<const float*>(c + 0x64) << "]";
            }
            if (IsReadable(c + 0x130, sizeof(float))) {
                m << " hudStam=["
                  << *reinterpret_cast<const float*>(c + 0x12C) << ","
                  << *reinterpret_cast<const float*>(c + 0x130) << "]";
            }
            if (IsReadable(c + 0x2DD4, sizeof(float))) {
                m << " rec="
                  << *reinterpret_cast<const float*>(c + 0x2DD4);
            }
            if (IsReadable(c + 0x30, sizeof(void*))) {
                auto* arr = *reinterpret_cast<std::uint8_t**>(c + 0x30);
                if (arr && IsReadable(arr + 0xEBB0, 4 * 0x40)) {
                    m << " heal=";
                    for (int i = 0; i < 4; ++i) {
                        auto* e = arr + 0xEBB0 + i * 0x40;
                        m << *reinterpret_cast<const float*>(e + 0x10) << "/"
                          << *reinterpret_cast<const float*>(e + 0x18) << "s"
                          << *reinterpret_cast<const std::int32_t*>(e + 0x2C)
                          << (i < 3 ? "+" : "");
                    }
                }
            }
        } else {
            m << " hpBox=<null>";
        }
    }
    // Legacy candidates for comparison.
    if (auto* stamBox = GuardVitalsBox(player, 0x7670)) {
        m << " box7670=[" << *reinterpret_cast<const float*>(stamBox + 0x60)
          << "," << *reinterpret_cast<const float*>(stamBox + 0x64) << "]";
    }
    if (auto* weapon = GuardWeaponOf(player)) {
        if (IsReadable(weapon + 0x2100, 4)) {
            m << " sharp=" << *reinterpret_cast<const int*>(weapon + 0x20F8)
              << "/" << *reinterpret_cast<const int*>(weapon + 0x20FC);
        }
    }
    if (auto* box = GuardVitalsBox(player, g_vitalsHpSlot.load())) {
        m << " cfgHp=[" << *reinterpret_cast<const float*>(box + 0x60)
          << "," << *reinterpret_cast<const float*>(box + 0x64) << "]";
    }
    g_hpWatchCount.fetch_add(1, std::memory_order_relaxed);
    Log(m.str());
}

// --- Stamina control (drain / cost / emptyExit) ------------------------------
// Real stamina = hpBox+0x12C (cur) / +0x130 (max) - HPWATCH-proven to track
// the HUD bar (drains on sprint, refills on idle). The engine's per-frame
// updater (FUN_141C4A260) reaches it through vtable+0xC68/+0xC70 virtuals, so
// we do not hook the writer; we read/write the field directly (gauge-lock
// pattern). Fatigue anchors for later: player+0x208EC (threshold float),
// +0x20914 (exhaustion flag byte).
// Config nativePC/plugins/ValorGauge.stamina.txt:
//   drain <ids> <perSec>            continuous drain while the action runs
//   cost <ids> <amount>             one-time cost on action accept (见切)
//   emptyExit <ids> <toSet> <toId>  force a buffered {set,id} switch once the
//                                   bar empties during those actions
std::vector<std::uint32_t> g_stamDrainIds;
float g_stamDrainPerSec = 0.0f;
std::vector<std::uint32_t> g_stamCostIds;
float g_stamCostAmount = 0.0f;
std::vector<std::uint32_t> g_stamExitIds;
std::uint32_t g_stamExitSet = 0;
std::uint32_t g_stamExitId = 0;
// Two-phase exit: phase 1 = sheathe switch, phase 2 = the panting state
// (user-provided 1:324). Stage machine survives action accepts until done
// or a 3s timeout fires.
std::uint32_t g_stamExitThenSet = 0;
std::uint32_t g_stamExitThenId = 0;
int g_stamExitStage = 0;
std::chrono::steady_clock::time_point g_stamExitStageTime{};
bool g_stamExitFired = false;
std::uint32_t g_stamDrainActId = 0;
bool g_stamDrainArmed = false;
float g_stamDrainBase = 0.0f;
std::chrono::steady_clock::time_point g_stamDrainStart{};
std::atomic<std::uint32_t> g_stamLogCount{0};

float* StaminaCurPtr(std::uint8_t* player) {
    auto* box = GuardVitalsBox(player, g_vitalsHpSlot.load());
    if (!box || !IsWritable(box + 0x12C, 4) || !IsReadable(box + 0x130, 4)) {
        return nullptr;
    }
    return reinterpret_cast<float*>(box + 0x12C);
}

void StaminaOnActionAccepted(std::uint32_t actionId) {
    g_stamExitFired = false;
    // Any accepted action change disarms the absolute-drain snapshot so
    // re-entering the move re-snapshots from the CURRENT bar (stale
    // snapshots emptied the bar instantly on re-entry).
    g_stamDrainArmed = false;
    if (g_stamCostIds.empty() || g_stamCostAmount <= 0.0f) {
        return;
    }
    if (std::find(g_stamCostIds.begin(), g_stamCostIds.end(), actionId) ==
        g_stamCostIds.end()) {
        return;
    }
    void* controller = g_lockedPlayerController;
    if (!controller) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (float* cur = StaminaCurPtr(player)) {
        const float before = *cur;
        *cur = std::max(0.0f, before - g_stamCostAmount);
        const auto n = g_stamLogCount.fetch_add(1, std::memory_order_relaxed);
        if (n < 60) {
            std::ostringstream m;
            m << "STAMCOST act=3:" << actionId << " " << before << " -> "
              << *cur << " (-" << g_stamCostAmount << ")";
            Log(m.str());
        }
    }
}

void PollStaminaControl() {
    if ((g_stamDrainIds.empty() && g_stamExitIds.empty())) {
        return;
    }
    void* controller = g_lockedPlayerController;
    if (!controller || !ModWeaponActive()) {
        return;
    }
    ActionInfo cur;
    {
        std::scoped_lock lock(g_actionMutex);
        if (!g_lastLongSwordAction) {
            return;
        }
        cur = *g_lastLongSwordAction;
    }
    const std::uint32_t curSet = static_cast<std::uint32_t>(cur.actionSet);
    const std::uint32_t curId = static_cast<std::uint32_t>(cur.actionId);
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    float* curPtr = StaminaCurPtr(player);
    if (!curPtr) {
        return;
    }
    const auto nowClock = std::chrono::steady_clock::now();

    // --- drain (set-3 watched actions): absolute-set suppression ----------
    if (curSet == 3) {
        const bool draining =
            !g_stamDrainIds.empty() &&
            std::find(g_stamDrainIds.begin(), g_stamDrainIds.end(),
                      curId) != g_stamDrainIds.end();
        if (draining) {
            // Snapshot on entry, then pull the bar DOWN to
            // (snapshot - rate*elapsed) each poll - never raises, wins
            // regardless of engine write order (only sawtooths within a
            // frame). The snapshot disarms on every accepted action change
            // (StaminaOnActionAccepted), so re-entering the move always
            // starts from the CURRENT bar - the old version kept the stale
            // snapshot and instantly emptied the bar on re-entry.
            if (!g_stamDrainArmed || g_stamDrainActId != curId) {
                g_stamDrainArmed = true;
                g_stamDrainActId = curId;
                g_stamDrainBase = *curPtr;
                g_stamDrainStart = nowClock;
            }
            if (g_stamDrainPerSec > 0.0f && *curPtr > 0.0f) {
                const float before = *curPtr;
                const float elapsed = std::chrono::duration<float>(
                                          nowClock - g_stamDrainStart).count();
                const float target = std::max(
                    0.0f, g_stamDrainBase - g_stamDrainPerSec * elapsed);
                if (*curPtr > target) {
                    *curPtr = target;
                }
                static std::int32_t lastLoggedFloor = -999;
                const std::int32_t floored =
                    static_cast<std::int32_t>(*curPtr);
                if (floored != lastLoggedFloor &&
                    g_stamLogCount.load(std::memory_order_relaxed) < 60) {
                    lastLoggedFloor = floored;
                    g_stamLogCount.fetch_add(1, std::memory_order_relaxed);
                    std::ostringstream m;
                    m << "STAMDRAIN act=3:" << curId << " " << before
                      << " -> " << *curPtr << " (" << g_stamDrainPerSec
                      << "/s, abs)";
                    Log(m.str());
                }
            }
        } else {
            g_stamDrainArmed = false;
        }
    } else {
        g_stamDrainArmed = false;
    }

    // --- empty exit, two-phase: sheathe first, then the panting state -----
    // User design: 1:324 = the panting (fatigue) idle; emptying during the
    // stance plays the sheathe, then immediately drops into 1:324.
    const bool exitWatched =
        !g_stamExitIds.empty() &&
        std::find(g_stamExitIds.begin(), g_stamExitIds.end(), curId) !=
            g_stamExitIds.end() &&
        curSet == 3;
    const auto matches = [&](std::uint32_t set, std::uint32_t id) {
        return curSet == set && curId == id;
    };
    auto queueSwitch = [&](std::uint32_t set, std::uint32_t id) {
        auto* ctrl = static_cast<std::uint8_t*>(controller);
        if (IsWritable(ctrl + 0xC0, 4) && IsWritable(ctrl + 0xBC, 4)) {
            *reinterpret_cast<std::int32_t*>(ctrl + 0xBC) =
                static_cast<std::int32_t>(set);
            *reinterpret_cast<std::int32_t*>(ctrl + 0xC0) =
                static_cast<std::int32_t>(id);
            return true;
        }
        return false;
    };
    const bool phase2Configured = g_stamExitThenSet != 0 || g_stamExitThenId != 0;
    // Stage timeouts guard against a queued slot that never consumes.
    if (g_stamExitStage > 0 &&
        std::chrono::duration<float>(nowClock - g_stamExitStageTime).count() >
            3.0f) {
        Log("STAMEXIT stage timeout - reset");
        g_stamExitStage = 0;
        g_stamExitFired = false;
    }
    switch (g_stamExitStage) {
        case 1:
            if (matches(g_stamExitSet, g_stamExitId)) {
                if (phase2Configured) {
                    if (queueSwitch(g_stamExitThenSet, g_stamExitThenId)) {
                        Log("STAMEXIT phase2 queued: panting state");
                        g_stamExitStage = 2;
                        g_stamExitStageTime = nowClock;
                    }
                } else {
                    g_stamExitStage = 0;
                    g_stamExitFired = false;
                }
            } else if (!exitWatched && *curPtr > 0.0f) {
                // Player left the watched action some other way.
                g_stamExitStage = 0;
                g_stamExitFired = false;
            }
            break;
        case 2:
            if (matches(g_stamExitThenSet, g_stamExitThenId)) {
                Log("STAMEXIT panting state reached");
                g_stamExitStage = 0;
                g_stamExitFired = false;
            }
            break;
        default:
            if (*curPtr <= 0.0f && exitWatched && !g_stamExitFired) {
                g_stamExitFired = true;
                if (queueSwitch(g_stamExitSet, g_stamExitId)) {
                    Log("STAMEXIT stamina empty - queued sheathe switch");
                    g_stamExitStage = 1;
                    g_stamExitStageTime = nowClock;
                }
            }
            break;
    }
}

void LoadStaminaConfig() {
    g_stamDrainIds.clear();
    g_stamDrainPerSec = 0.0f;
    g_stamCostIds.clear();
    g_stamCostAmount = 0.0f;
    g_stamExitIds.clear();
    g_stamExitSet = 0;
    g_stamExitId = 0;
    g_stamExitThenSet = 0;
    g_stamExitThenId = 0;
    g_stamExitStage = 0;
    g_stamExitFired = false;
    std::ifstream input("nativePC/plugins/ValorGauge.stamina.txt");
    if (!input) {
        Log("Stamina: no ValorGauge.stamina.txt - stamina control OFF");
        return;
    }
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
        if (kind == "drain") {
            std::string ids;
            float perSec = 0.0f;
            if (ss >> ids >> perSec && ParseIdList(ids, g_stamDrainIds)) {
                g_stamDrainPerSec = std::max(0.0f, std::min(200.0f, perSec));
            } else {
                Log("Stamina: malformed drain line - skipped");
            }
        } else if (kind == "cost") {
            std::string ids;
            float amount = 0.0f;
            if (ss >> ids >> amount && ParseIdList(ids, g_stamCostIds)) {
                g_stamCostAmount = std::max(0.0f, std::min(500.0f, amount));
            } else {
                Log("Stamina: malformed cost line - skipped");
            }
        } else if (kind == "emptyExit") {
            std::string ids;
            unsigned long toSet = 0;
            unsigned long toId = 0;
            unsigned long thenSet = 0;
            unsigned long thenId = 0;
            if (ss >> ids >> toSet >> toId && ParseIdList(ids, g_stamExitIds)) {
                g_stamExitSet = static_cast<std::uint32_t>(toSet);
                g_stamExitId = static_cast<std::uint32_t>(toId);
                ss >> thenSet >> thenId;  // optional phase 2
                g_stamExitThenSet = static_cast<std::uint32_t>(thenSet);
                g_stamExitThenId = static_cast<std::uint32_t>(thenId);
            } else {
                Log("Stamina: malformed emptyExit line - skipped");
            }
        }
    }
    std::ostringstream m;
    m << "Stamina control: drain=";
    if (g_stamDrainIds.empty()) {
        m << "(none)";
    } else {
        for (std::size_t i = 0; i < g_stamDrainIds.size(); ++i) {
            if (i) m << ',';
            m << g_stamDrainIds[i];
        }
        m << '@' << g_stamDrainPerSec << "/s";
    }
    m << " cost=";
    if (g_stamCostIds.empty()) {
        m << "(none)";
    } else {
        for (std::size_t i = 0; i < g_stamCostIds.size(); ++i) {
            if (i) m << ',';
            m << g_stamCostIds[i];
        }
        m << '-' << g_stamCostAmount;
    }
    m << " emptyExit=";
    if (g_stamExitIds.empty()) {
        m << "(none)";
    } else {
        for (std::size_t i = 0; i < g_stamExitIds.size(); ++i) {
            if (i) m << ',';
            m << g_stamExitIds[i];
        }
        m << "->" << g_stamExitSet << ':' << g_stamExitId;
    }
    Log(m.str());
}

void PollGuardWindow() {
    if (!g_guardActive.load(std::memory_order_relaxed)) {
        return;
    }
    if (GuardNowMs() >= g_guardDeadlineMs.load(std::memory_order_relaxed)) {
        GuardCloseWindow("deadline");
        return;
    }
    if (!g_lockedPlayerController || !ModWeaponActive()) {
        GuardCloseWindow("weapon/controller lost");
    }
}

void LoadGuardConfig() {
    if (g_guardActive.load(std::memory_order_relaxed)) {
        GuardCloseWindow("config reload");
    }
    g_guardCfg = GuardConfig{};
    std::ifstream input("nativePC/plugins/ValorGauge.guard.txt");
    if (!input) {
        Log("Guard: no ValorGauge.guard.txt - native-guard borrow OFF");
        return;
    }
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
        if (kind == "guard") {
            std::string ids;
            float arc = 0.0f;
            float turn = 0.0f;
            int level = 0;
            int stam = 0;
            int gauge = 0;
            if (ss >> ids >> arc >> turn >> level >> stam >> gauge) {
                if (ParseIdList(ids, g_guardCfg.ids)) {
                    g_guardCfg.arcHalfDeg =
                        std::max(1.0f, std::min(180.0f, arc));
                    g_guardCfg.maxTurnDeg =
                        std::max(0.0f, std::min(180.0f, turn));
                    g_guardCfg.guardLevel = level;
                    g_guardCfg.staminaRefund = stam != 0;
                    g_guardCfg.gaugeRefill = gauge != 0;
                } else {
                    Log("Guard: bad id list - guard line skipped");
                }
            } else {
                Log("Guard: malformed guard line - skipped");
            }
        } else if (kind == "armIds") {
            std::string ids;
            if (ss >> ids) {
                ParseIdList(ids, g_guardCfg.armIds);
            }
        } else if (kind == "turnIds") {
            // Failed parse (e.g. "turnIds off") clears the list = clamp off.
            std::string ids;
            if (ss >> ids) {
                (void)ParseIdList(ids, g_guardCfg.turnIds);
            }
        } else if (kind == "option") {
            std::string key;
            float val = 0.0f;
            if (ss >> key >> val) {
                if (key == "maxWindowSec") {
                    g_guardCfg.maxWindowSec =
                        std::max(0.5f, std::min(30.0f, val));
                } else if (key == "armFallback") {
                    g_guardCfg.armFallback = val != 0;
                } else if (key == "swallowReaction") {
                    g_guardCfg.swallowReaction = val != 0;
                } else if (key == "counterOnBlock") {
                    g_guardCfg.counterOnBlock = val != 0;
                } else if (key == "blockWindowMs") {
                    g_blockWindowMs = static_cast<std::int64_t>(
                        std::max(30.0f, std::min(1000.0f, val)));
                }
            }
        }
    }
    if (g_guardCfg.ids.empty()) {
        Log("Guard: no valid guard line - native-guard borrow OFF");
        return;
    }
    std::ostringstream m;
    m << "Guard: ON ids=";
    for (std::size_t i = 0; i < g_guardCfg.ids.size(); ++i) {
        if (i) m << ',';
        m << g_guardCfg.ids[i];
    }
    m << " arc=+/-" << g_guardCfg.arcHalfDeg
      << "deg maxTurn=" << g_guardCfg.maxTurnDeg
      << "deg level=" << g_guardCfg.guardLevel
      << " stamRefund=" << g_guardCfg.staminaRefund
      << " gaugeRefill=" << g_guardCfg.gaugeRefill
      << " armFallback=" << g_guardCfg.armFallback
      << " swallow=" << g_guardCfg.swallowReaction
      << " counterOnBlock=" << g_guardCfg.counterOnBlock
      << " maxWindow=" << g_guardCfg.maxWindowSec << "s";
    m << " armIds=";
    if (g_guardCfg.armIds.empty()) {
        m << "(all)";
    } else {
        for (std::size_t i = 0; i < g_guardCfg.armIds.size(); ++i) {
            if (i) m << ',';
            m << g_guardCfg.armIds[i];
        }
    }
    m << " turnIds=";
    if (g_guardCfg.turnIds.empty()) {
        m << "(off)";
    } else {
        for (std::size_t i = 0; i < g_guardCfg.turnIds.size(); ++i) {
            if (i) m << ',';
            m << g_guardCfg.turnIds[i];
        }
    }
    Log(m.str());
}

void LoadForceActionConfig() {
    std::ifstream input("nativePC\\plugins\\ValorGauge.force-action.txt");
    std::int32_t set = 0;
    std::int32_t id = 0;
    float invulnSec = 0.0f;
    if (input >> set >> id && set >= 0 && id >= 0) {
        g_forceSet = static_cast<std::uint32_t>(set);
        g_forceId = static_cast<std::uint32_t>(id);
        g_forceCfgSet.store(g_forceSet, std::memory_order_relaxed);
        g_forceCfgId.store(g_forceId, std::memory_order_relaxed);
    }
    if (input >> invulnSec && invulnSec >= 0.0f && invulnSec <= 3.0f) {
        g_forceInvulnSec = invulnSec;
    }
    float startFrame = -1.0f;
    if (input >> startFrame && startFrame >= 0.0f && startFrame <= 1000.0f) {
        g_seekFrame = startFrame;
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
        << (g_counterAutoForce ? "on" : "off (Ctrl inside window)")
        << ", startFrame=" << (g_seekFrame >= 0.0f ? "on" : "off");
    if (g_seekFrame >= 0.0f) {
        msg << " (" << g_seekFrame << "s)";
    }
    msg << ", Shift+F6 reloads";
    Log(msg.str());
    LoadModWeaponTypeConfig();
    LoadHitChargeConfig();
    LoadGaugeLockConfig();
    LoadGaugeLogConfig();
    LoadGaugeRulesConfig();
    LoadSheathRewriteConfig();
    LoadActionSoundConfig();
    LoadSheathCounterConfig();
    LoadVitalsConfig();
    LoadGuardConfig();
    LoadHpLockConfig();
    LoadStaminaConfig();
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
    m << "COUNTER WINDOW OPEN (" << reason << ") nohit=" << g_forceInvulnSec
      << "s - press the spirit key to counter";
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
            if (negBit && !g_negBitPrev && !g_clangLatch) {
                g_clangLatch = true;
                OpenCounterWindow("negation bit 250 set (player+0x8954)");
                g_forceSet = g_forceCfgSet.load(std::memory_order_relaxed);
                g_forceId = g_forceCfgId.load(std::memory_order_relaxed);
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
                if (flash > 0.5f && g_flashPrev <= 0.5f && !g_clangLatch) {
                    g_clangLatch = true;
                    OpenCounterWindow("deflect flash (weapon+0x23A0)");
                    g_forceSet = g_forceCfgSet.load(std::memory_order_relaxed);
                    g_forceId = g_forceCfgId.load(std::memory_order_relaxed);
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
        if (in86 && bit15 && !g_fsuccBitPrev && !g_clangLatch) {
            g_clangLatch = true;
            OpenCounterWindow("foresight success bit15 (player+0x8938)");
            g_forceSet = g_forceCfgSet.load(std::memory_order_relaxed);
            g_forceId = g_forceCfgId.load(std::memory_order_relaxed);
            if (g_counterAutoForce && !g_forceArmed.exchange(true)) {
                Log("CLANG: vanilla foresight success latch - auto-force armed");
            }
        }
        g_fsuccBitPrev = bit15;
    }
}

// Per-frame sheathe-family watcher: config rules (event-bit rising edge while
// inside a watched action -> counter window + force switch) plus the mining
// logger that finds WHICH bit (if any) the negation of the TIML i-frame
// windows produces. See the SheathCounterRule block for the config format.
void PollSheatheCounter() {
    if (g_sheathRules.empty() && g_sheathMineIds.empty()) {
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
        for (auto& rule : g_sheathRules) {
            rule.prev = false;
        }
        g_sheathMinePrimed = false;
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    ActionInfo cur;
    bool haveCur = false;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction) {
            cur = *g_lastLongSwordAction;
            haveCur = true;
        }
    }
    const bool set3 = haveCur && cur.actionSet == 3;
    const std::uint32_t id = haveCur ? cur.actionId : 0;

    std::uint32_t ev[8] = {};
    const bool evReadable = IsReadable(player + 0x8938, sizeof(ev));
    if (evReadable) {
        std::memcpy(ev, player + 0x8938, sizeof(ev));
    }

    // 1) Config rules: rising edge on the rule's event bit.
    for (auto& rule : g_sheathRules) {
        if (rule.keyBased) {
            // krel: key-release edge while inside the watched action.
            // The armed force is consumed by TryQueuedForceWrite on the
            // game thread; deliberately no counter window (the sheathing
            // follow-up needs no no-hit) and no latch (the edge IS the
            // one-shot; a rejected force retries on the next press).
            if (!set3 || !SheathIdsContain(rule.duringIds, id)) {
                rule.prev = false;
                continue;
            }
            const bool down = rule.vk != 0 &&
                (GetAsyncKeyState(static_cast<int>(rule.vk)) & 0x8000) != 0;
            // Diagnosis: the sheathe key vk is a guess (side-up = XBUTTON2?).
            // While in-hold, scan ALL vks and log whichever are down - the
            // real key appears by name even if the driver remaps it.
            static std::atomic_uint32_t krelDbg{0};
            if (krelDbg.load() < 8 && g_sheathMineLogCount.load() < 400) {
                krelDbg.fetch_add(1);
                std::ostringstream m;
                m << "SHEATHKRELDBG act=3:" << id << " down=[";
                bool first = true;
                for (unsigned vkScan = 2; vkScan < 255; ++vkScan) {
                    if ((GetAsyncKeyState(static_cast<int>(vkScan)) & 0x8000)
                            != 0) {
                        if (!first) m << ',';
                        m << vkScan;
                        first = false;
                    }
                }
                m << "] rule.vk=" << rule.vk << " down=" << down
                  << " prev=" << rule.prev;
                Log(m.str());
            }
            const bool release = rule.prev && !down;
            rule.prev = down;
            if (release && !g_forceArmed.exchange(true)) {
                g_forceSet = rule.toSet;
                g_forceId = rule.toId;
                std::ostringstream m;
                m << "SHEATHKREL: key " << rule.vk << " released during 3:"
                  << id << " - force " << rule.toSet << ':' << rule.toId;
                Log(m.str());
            }
            continue;
        }
        if (rule.dispatchBased || rule.armorBased) {
            // drule/arule fire in their own hooks; re-arm here on leaving
            // the watched set (sentinel lastActId), so same-id re-entry
            // (98 -> 100 -> 98) counts as a new instance and fires again.
            if (!set3 || !SheathIdsContain(rule.duringIds, id)) {
                rule.lastActId = 0xFFFFFFFFu;
            } else if (id != rule.lastActId) {
                rule.fired = false;
                rule.lastActId = id;
            }
            continue; // not bit rules (drule fires in the dispatch hook,
                      // arule in the damage hook); their eventBit is garbage
        }
        if (!set3 || !SheathIdsContain(rule.duringIds, id)) {
            rule.prev = false;
            rule.fired = false;
            rule.lastActId = 0;
            continue;
        }
        if (id != rule.lastActId) {
            rule.lastActId = id;
            rule.fired = false; // new action instance: re-arm the one-shot
        }
        if (!evReadable || rule.eventBit >= 256) {
            continue;
        }
        const bool state =
            (ev[rule.eventBit >> 5] >> (rule.eventBit & 31)) & 1u;
        if (state && g_sheathMineLogCount.load() < 480) {
            ++g_sheathMineLogCount;
            std::ostringstream m;
            m << "SHEATHRULEDBG bit=" << rule.eventBit << " state=" << state
              << " prev=" << rule.prev << " fired=" << rule.fired
              << " act=3:" << id;
            Log(m.str());
        }
        if (state && !rule.prev && !rule.fired) {
            rule.fired = true;
            g_forceSet = rule.toSet;
            g_forceId = rule.toId;
            g_sheathReassertSet.store(
                static_cast<std::int32_t>(rule.toSet), std::memory_order_relaxed);
            g_sheathReassertId.store(
                static_cast<std::int32_t>(rule.toId), std::memory_order_relaxed);
            g_sheathReassertUntilMs.store(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count() +
                    1200,
                std::memory_order_relaxed);
            std::ostringstream m;
            m << "SHEATHCOUNTER: event bit " << rule.eventBit
              << " rose during 3:" << id << " - force " << rule.toSet << ':'
              << rule.toId;
            Log(m.str());
            OpenCounterWindow("sheath-counter rule");
            if (g_counterAutoForce && !g_forceArmed.exchange(true)) {
                Log("SHEATHCOUNTER: auto-force armed");
            }
        }
        rule.prev = state;
    }

    // 2) Mining: log every event-dword change while inside a watched action
    //    (edges only; F5 drops a manual "got hit" marker into the same log).
    const bool mining = set3 && !g_sheathMineIds.empty() &&
                        SheathIdsContain(g_sheathMineIds, id);
    if (mining && evReadable) {
        if (!g_sheathMinePrimed) {
            std::memcpy(g_sheathMinePrevEv, ev, sizeof(ev));
            g_sheathMinePrimed = true;
            std::ostringstream m;
            m << "IFRMINE armed in 3:" << id << std::hex << std::uppercase;
            for (int d = 0; d < 8; ++d) {
                m << " ev" << d << "=0x" << ev[d];
            }
            Log(m.str());
        } else {
            for (int d = 0; d < 8; ++d) {
                if (ev[d] == g_sheathMinePrevEv[d]) {
                    continue;
                }
                const std::uint32_t rose = ev[d] & ~g_sheathMinePrevEv[d];
                if (g_sheathMineLogCount.load() < 400) {
                    ++g_sheathMineLogCount;
                    std::ostringstream m;
                    m << "IFRMINE ev" << d << " 0x" << std::hex
                      << std::uppercase << g_sheathMinePrevEv[d] << " -> 0x"
                      << ev[d] << std::dec;
                    if (rose) {
                        m << " rose=[";
                        for (int b = 0; b < 32; ++b) {
                            if (rose & (1u << b)) {
                                m << d * 32 + b << ',';
                            }
                        }
                        m << ']';
                    }
                    m << " act=3:" << id;
                    Log(m.str());
                }
            }
            std::memcpy(g_sheathMinePrevEv, ev, sizeof(ev));
        }
    } else {
        g_sheathMinePrimed = false;
    }
}

// Bluesim: per-frame mimic of the 见切/居合 per-move handlers while inside a
// watched action (the vanilla handlers are dispatched by action id through
// function-pointer tables, so custom actions never run them). Writes the
// same outputs: once per action the +0x2399 / +0x2CED latches and the FX
// 0x7D7 (foresight blue flash) + 0x7D4 (iai sound); every frame
// weapon+0x23A0 / +0x23A4 <- playerParam+0x8A8 / +0x8B8 (the success-armor
// timers that also drive event bit 151 while positive).
void PollSheatheBlueSim() {
    if (g_sheathBlueIds.empty()) {
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
        g_sheathBlueActId = 0;
        g_sheathBlueFx = false;
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    ActionInfo cur;
    bool haveCur = false;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction) {
            cur = *g_lastLongSwordAction;
            haveCur = true;
        }
    }
    if (!haveCur || cur.actionSet != 3 ||
        !SheathIdsContain(g_sheathBlueIds, cur.actionId)) {
        g_sheathBlueActId = 0;
        g_sheathBlueFx = false;
        return;
    }
    if (!IsReadable(player + 0x76B0, sizeof(void*))) {
        return;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
    if (!weapon || !IsReadable(weapon + 0x8808, sizeof(void*))) {
        return;
    }
    if (cur.actionId != g_sheathBlueActId) {
        g_sheathBlueActId = cur.actionId;
        g_sheathBlueFx = false;
    }
    if (!g_sheathBlueFx) {
        g_sheathBlueFx = true;
        if (IsReadable(weapon + 0x2399, 1)) {
            weapon[0x2399] = 1;
        }
        if (IsReadable(weapon + 0x2CED, 1)) {
            weapon[0x2CED] = 1;
        }
        auto* emitter = *reinterpret_cast<void**>(weapon + 0x8808);
        if (emitter && g_fxNotifyA) {
            g_fxNotifyA(emitter, 0x7D7, 0); // foresight blue flash + sound
            g_fxNotifyA(emitter, 0x7D4, 0); // iai deflect sound
        }
        std::ostringstream m;
        m << "BLUESIM armed in 3:" << cur.actionId;
        Log(m.str());
    }
    if (IsReadable(weapon + 0x7810, sizeof(void*))) {
        auto* playerParam = *reinterpret_cast<std::uint8_t**>(weapon + 0x7810);
        if (playerParam && IsReadable(playerParam + 0x8B8, sizeof(float) * 2)) {
            if (IsWritable(weapon + 0x23A0, sizeof(float))) {
                *reinterpret_cast<float*>(weapon + 0x23A0) =
                    *reinterpret_cast<const float*>(playerParam + 0x8A8);
            }
            if (IsWritable(weapon + 0x23A4, sizeof(float))) {
                *reinterpret_cast<float*>(weapon + 0x23A4) =
                    *reinterpret_cast<const float*>(playerParam + 0x8B8);
            }
        }
    }
}
#endif // !VALOR_PROBE

#ifdef VALOR_PROBE
// POOLDIFF: per-frame reaction-pool snapshot diff. Hypothesis: the deflect
// ("blue i-frame") windows of 见切/居合 are reaction-pool timers (same family
// as the no-hit timer pool+0xE4 = query type 8); the arbitration picks
// guard/deflect/damage by which timer is running. Log every pool field that
// RISES above 0.5 (a defense window opening) - the offset that spikes on a
// 见切 deflect is the deflect timer.
std::uint8_t g_poolDiffPrev[0x180] = {};
std::uint8_t g_poolDiffPrevMgr[0x30] = {}; // mgr +0x760..0x790 (reaction slots)
bool g_poolDiffPrimed = false;
std::atomic_uint32_t g_poolDiffLogs{0};

void PollPoolDiff() {
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (!IsReadable(player + 0x7D20, sizeof(void*))) {
        return;
    }
    auto* mgr = *reinterpret_cast<std::uint8_t**>(player + 0x7D20);
    if (!mgr || !IsReadable(mgr + 0x28, sizeof(void*))) {
        return;
    }
    auto* pool = *reinterpret_cast<std::uint8_t**>(mgr + 0x28);
    if (!pool || !IsReadable(pool, sizeof(g_poolDiffPrev))) {
        return;
    }
    if (!g_poolDiffPrimed) {
        std::memcpy(g_poolDiffPrev, pool, sizeof(g_poolDiffPrev));
        if (IsReadable(mgr + 0x760, sizeof(g_poolDiffPrevMgr))) {
            std::memcpy(g_poolDiffPrevMgr, mgr + 0x760, sizeof(g_poolDiffPrevMgr));
        }
        g_poolDiffPrimed = true;
        return;
    }
    auto logRise = [&](const char* tag, std::size_t off, float oldVal, float newVal) {
        if (g_poolDiffLogs.load() >= 600) {
            return;
        }
        ++g_poolDiffLogs;
        std::ostringstream m;
        m << "POOLDIFF " << tag << " +0x" << std::hex << off << std::dec
          << ": " << oldVal << " -> " << newVal;
        std::uint32_t newRaw;
        std::memcpy(&newRaw, pool + off, 4);
        m << " (raw 0x" << std::hex << newRaw << ")" << std::dec;
        {
            std::scoped_lock lock(g_actionMutex);
            if (g_lastLongSwordAction) {
                m << " act=" << g_lastLongSwordAction->actionSet << ':'
                  << g_lastLongSwordAction->actionId;
            }
        }
        Log(m.str());
    };
    for (std::size_t off = 0; off + 4 <= sizeof(g_poolDiffPrev); off += 4) {
        float newVal;
        std::memcpy(&newVal, pool + off, 4);
        float oldVal;
        std::memcpy(&oldVal, g_poolDiffPrev + off, 4);
        // Defense windows on LS are short (0.2-0.5 s) - a 0.5 threshold
        // missed them entirely in the first mining round. Any rise past
        // 0.05 from at-or-below it, or a jump > 0.1, is a window event.
        if (newVal > 0.05f && oldVal <= 0.05f) {
            logRise("pool", off, oldVal, newVal);
        } else if (newVal - oldVal > 0.1f && newVal > 0.1f) {
            logRise("pooljump", off, oldVal, newVal);
        }
    }
    std::memcpy(g_poolDiffPrev, pool, sizeof(g_poolDiffPrev));
    // MGR classification region (+0x760-0x790): the guard/deflect reaction
    // slots (+0x769 u16 = 0x100/0x101 lives here) - raw-change detection.
    if (IsReadable(mgr + 0x760, sizeof(g_poolDiffPrevMgr))) {
        for (std::size_t off = 0; off + 4 <= sizeof(g_poolDiffPrevMgr); off += 4) {
            std::uint32_t newRaw, oldRaw;
            std::memcpy(&newRaw, mgr + 0x760 + off, 4);
            std::memcpy(&oldRaw, g_poolDiffPrevMgr + off, 4);
            if (newRaw != oldRaw && g_poolDiffLogs.load() < 600) {
                ++g_poolDiffLogs;
                std::ostringstream m;
                m << "POOLDIFF mgr +0x" << std::hex << (0x760 + off) << ": 0x"
                  << oldRaw << " -> 0x" << newRaw << std::dec;
                {
                    std::scoped_lock lock(g_actionMutex);
                    if (g_lastLongSwordAction) {
                        m << " act=" << g_lastLongSwordAction->actionSet << ':'
                          << g_lastLongSwordAction->actionId;
                    }
                }
                Log(m.str());
            }
        }
        std::memcpy(g_poolDiffPrevMgr, mgr + 0x760, sizeof(g_poolDiffPrevMgr));
    }
}
#endif // VALOR_PROBE (POOLDIFF)

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

// --- F12 ATK-row scan + guard-page access trace ------------------------------
// Part-break (破坏值) question 2026-10-18: which cAttackParamPl field feeds the
// per-hit part-break accumulation and whether anything reads it at all on the
// custom row. cAttackParamPl = col ATK type-1 row object (0x270 bytes; factory
// jump table 0x143F949E0[type], class name string at 0x142F12DD0); runtime
// layout from Load FUN_14027d880 / Save FUN_14027dd20 (row 73 live-confirmed
// 2026-10-19 via ATKHEX: +0x1F0/+0x210 mirrored the deployed HitLag edit):
//   +0x00 vtable(0x142F12DE0) +0x08 Index u32 +0x0c Index2 u32
//   +0x18 Attack_Type u32 +0x20 Motion_Value +0x24 Fixed_Damage
//   +0x28 Part_Modifier +0x68 Stun +0x6c Exhaust +0x1c8 Soften
//   +0x1d0 Elem_Mod +0x1d4 Status_Mod +0x1d8 Mount +0x240 Flinch u32
//   +0x1ec HitLagSpd_a +0x1f0 HitLag_a +0x20c HitLagSpd_b +0x210 HitLag_b
// F12 arms one session: background scan of private RW memory for the vtable
// pointer, per-row dump, hex dump of the configured row, then a PAGE_GUARD
// trace over that row for 30 s (VEH logs rw/offset/rip/stack per access;
// neighbour-row faults on the same page restore the guard instead of
// disarming it - they are counted and reported in the session-over line).
// Config nativePC/plugins/ValorProbe.atktrace.txt = "<rowIndex>" (default 22).
static std::atomic<std::uintptr_t> g_atkRowTarget{0};
static std::atomic<long long> g_atkTraceDeadline{0};
static std::atomic<unsigned> g_atkTraceLines{0};
static std::atomic<unsigned> g_atkNeighborFaults{0};
static std::atomic<unsigned> g_atkNeiLines{0};
static std::atomic<bool> g_atkReprotectPending{false};
static std::atomic<bool> g_atkVehInstalled{false};
static std::atomic<bool> g_atkScanRunning{false};
static std::atomic<std::uintptr_t> g_atkGuardedRows[8] = {};
static std::atomic<unsigned> g_atkGuardedRowCount{0};
static int g_atkRowCfg = 22;
static bool g_atkKeyWasDown = false;

struct AtkGuardedPage {
    void* base;
    SIZE_T size;
    DWORD oldProtect;
};
static std::vector<AtkGuardedPage> g_atkGuardedPages;
static std::mutex g_atkGuardMutex;

// (Re)arm PAGE_GUARD on every page overlapping one row. Safe to call again
// after a fault consumed the guard bit - that is exactly how the single-step
// handler restores it. A previous build skipped re-arming whenever the page
// list was non-empty, which silently turned the whole trace one-shot.
static void AtkGuardRowPages(std::uintptr_t target) {
    if (!target) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_atkGuardMutex);
    const auto pageStart = target & ~static_cast<std::uintptr_t>(0xFFF);
    const auto pageEnd = (target + 0x270 + 0xFFF) & ~static_cast<std::uintptr_t>(0xFFF);
    for (auto page = pageStart; page < pageEnd; page += 0x1000) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(page), &info, sizeof(info)) ||
            info.State != MEM_COMMIT) {
            continue;
        }
        DWORD oldProtect = 0;
        if (VirtualProtect(reinterpret_cast<void*>(page), 0x1000,
                           (info.Protect & ~PAGE_GUARD) | PAGE_GUARD, &oldProtect)) {
            bool known = false;
            for (const auto& pg : g_atkGuardedPages) {
                known |= (pg.base == reinterpret_cast<void*>(page));
            }
            if (!known) {
                g_atkGuardedPages.push_back(
                    {reinterpret_cast<void*>(page), 0x1000, oldProtect});
            }
        } else {
            static std::atomic<unsigned> guardFails{0};
            if (guardFails.fetch_add(1, std::memory_order_relaxed) < 3) {
                std::ostringstream m;
                m << "ATKTRACE: VirtualProtect(guard) FAILED page=0x" << std::hex
                  << page << " err=" << std::dec << GetLastError();
                Log(m.str());
            }
        }
    }
}

static void AtkReprotectAll() {
    std::lock_guard<std::mutex> lock(g_atkGuardMutex);
    for (auto& pg : g_atkGuardedPages) {
        DWORD dontCare = 0;
        VirtualProtect(pg.base, pg.size,
                       (pg.oldProtect & ~PAGE_GUARD) | PAGE_GUARD, &dontCare);
    }
}

static void AtkRemoveGuard() {
    std::lock_guard<std::mutex> lock(g_atkGuardMutex);
    for (const auto& page : g_atkGuardedPages) {
        DWORD dontCare = 0;
        VirtualProtect(page.base, page.size, page.oldProtect, &dontCare);
    }
    g_atkGuardedPages.clear();
    g_atkGuardedRowCount.store(0, std::memory_order_relaxed);
    for (auto& r : g_atkGuardedRows) {
        r.store(0, std::memory_order_relaxed);
    }
}

static LONG NTAPI AtkGuardVectored(PEXCEPTION_POINTERS info) {
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code != STATUS_GUARD_PAGE_VIOLATION) {
        if (code == STATUS_SINGLE_STEP &&
            g_atkReprotectPending.exchange(false, std::memory_order_relaxed)) {
            // The faulting instruction finished; put the guards back.
            info->ContextRecord->EFlags &= ~0x100;
            AtkReprotectAll();
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const int rowCount = static_cast<int>(
        g_atkGuardedRowCount.load(std::memory_order_relaxed));
    if (rowCount <= 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const auto addr = static_cast<std::uintptr_t>(
        info->ExceptionRecord->ExceptionInformation[1]);
    const auto base = g_moduleBase.load(std::memory_order_relaxed);
    for (int i = 0; i < rowCount; ++i) {
        const auto target = g_atkGuardedRows[i].load(std::memory_order_relaxed);
        if (!target) {
            continue;
        }
        const bool inRow = addr >= target && addr < target + 0x270;
        const auto page = addr & ~static_cast<std::uintptr_t>(0xFFF);
        const bool onPage = page >= (target & ~static_cast<std::uintptr_t>(0xFFF)) &&
                            page <= ((target + 0x270 - 1) & ~static_cast<std::uintptr_t>(0xFFF));
        if (!inRow && !onPage) {
            continue;
        }
        // Re-run the faulting instruction, then restore the guard on the
        // single step. Without this the OS-cleared PAGE_GUARD bit never comes
        // back and the whole session dies on the first touch.
        info->ContextRecord->EFlags |= 0x100;
        g_atkReprotectPending.store(true, std::memory_order_relaxed);

        if (inRow) {
            const unsigned line =
                g_atkTraceLines.fetch_add(1, std::memory_order_relaxed);
            if (line < 1500) {
                const char* rw =
                    info->ExceptionRecord->ExceptionInformation[0] == 0
                        ? "R"
                        : (info->ExceptionRecord->ExceptionInformation[0] == 1
                               ? "W"
                               : "X");
                void* stack[10] = {};
                USHORT frames = RtlCaptureStackBackTrace(0, 10, stack, nullptr);
                std::ostringstream m;
                m << "ATKACC i" << std::dec << i << " " << rw << " off=0x"
                  << std::hex << (addr - target) << " rip=+0x"
                  << (info->ContextRecord->Rip - base);
                int printed = 0;
                for (USHORT f = 0; f < frames && printed < 6; ++f) {
                    const auto pc = reinterpret_cast<std::uintptr_t>(stack[f]);
                    if (pc >= base && pc < base + 0x2000000) {
                        m << (printed == 0 ? " stk=" : ",") << "+0x" << (pc - base);
                        ++printed;
                    }
                }
                Log(m.str());
            }
        } else {
            // Neighbouring allocation on a guarded page: count it, and log
            // the first ones - the reader RIP is the breadcrumb to whoever
            // walks these rows at runtime.
            g_atkNeighborFaults.fetch_add(1, std::memory_order_relaxed);
            const unsigned nl = g_atkNeiLines.fetch_add(1, std::memory_order_relaxed);
            if (nl < 200) {
                const char* rw =
                    info->ExceptionRecord->ExceptionInformation[0] == 0
                        ? "R"
                        : (info->ExceptionRecord->ExceptionInformation[0] == 1
                               ? "W"
                               : "X");
                void* stack[10] = {};
                USHORT frames = RtlCaptureStackBackTrace(0, 10, stack, nullptr);
                std::ostringstream m;
                m << "ATKNEI i" << std::dec << i << " " << rw << " rel="
                  << static_cast<long long>(addr) -
                         static_cast<long long>(target)
                  << " rip=+0x" << std::hex
                  << (info->ContextRecord->Rip - base);
                int printed = 0;
                for (USHORT f = 0; f < frames && printed < 4; ++f) {
                    const auto pc = reinterpret_cast<std::uintptr_t>(stack[f]);
                    if (pc >= base && pc < base + 0x2000000) {
                        m << (printed == 0 ? " stk=" : ",") << "+0x" << (pc - base);
                        ++printed;
                    }
                }
                Log(m.str());
            }
        }
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// SEH can't live in a function with unwinding objects - keep the guard-page
// self-test reads in their own no-throw helper.
static bool AtkSelftestRead(std::uintptr_t addr, std::uint32_t* mv,
                            std::uint32_t* lag) {
    __try {
        const auto* p = reinterpret_cast<const std::uint8_t*>(addr);
        *mv = *reinterpret_cast<const std::uint32_t*>(p + 0x20);
        *lag = *reinterpret_cast<const std::uint32_t*>(p + 0x1F0);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *mv = 0xDEADBEEFu;
        *lag = 0xDEADBEEFu;
        return false;
    }
}

static void AtkScanThread(int wantIdx) {
    const auto base = g_moduleBase.load(std::memory_order_relaxed);
    if (!base) {
        Log("ATKSCAN: no module base yet");
        g_atkScanRunning.store(false, std::memory_order_relaxed);
        return;
    }
    const std::uintptr_t vtable = base + 0x2F12DE0;
    std::uint8_t needle[8];
    std::memcpy(needle, &vtable, sizeof(needle));

    // Raw scans run under SEH: the game streams resources and can free a heap
    // region between VirtualQuery and the read.
    struct Row {
        std::uintptr_t addr;
        std::uint32_t idx;
    };
    std::vector<Row> rows;

    auto scanRegion = [&](const std::uint8_t* start, std::size_t size) {
        if (size < 0x270) {
            return;
        }
        std::uintptr_t candidates[512];
        std::size_t count = 0;
        __try {
            for (std::size_t i = 0; i + 8 <= size; ++i) {
                if (start[i] != needle[0] ||
                    std::memcmp(start + i, needle, 8) != 0) {
                    continue;
                }
                if (count < 512) {
                    candidates[count] =
                        reinterpret_cast<std::uintptr_t>(start + i);
                }
                ++count;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        for (std::size_t c = 0; c < count && c < 512; ++c) {
            const auto a = candidates[c];
            std::uint32_t idx = 0xFFFFFFFFu;
            std::uint32_t type = 0xFFFFFFFFu;
            float mv = 0.0f;
            const auto* p = reinterpret_cast<const std::uint8_t*>(a);
            __try {
                idx = *reinterpret_cast<const std::uint32_t*>(p + 0x08);
                type = *reinterpret_cast<const std::uint32_t*>(p + 0x18);
                mv = *reinterpret_cast<const float*>(p + 0x20);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                continue;
            }
            if (idx < 1024 && type < 32 && std::isfinite(mv) &&
                std::fabs(mv) < 100000.0f) {
                rows.push_back({a, idx});
            }
        }
    };

    std::uintptr_t regionStart = 0;
    while (regionStart < 0x7FFFFFFEFFFF) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(regionStart), &info, sizeof(info))) {
            break;
        }
        const auto next = regionStart + info.RegionSize;
        if (info.State == MEM_COMMIT && info.Type == MEM_PRIVATE &&
            (info.Protect & PAGE_READWRITE) != 0 &&
            (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0) {
            scanRegion(reinterpret_cast<const std::uint8_t*>(info.BaseAddress),
                       info.RegionSize);
        }
        if (next <= regionStart) {
            break;
        }
        regionStart = next;
    }

    std::ostringstream summary;
    summary << "ATKSCAN: " << rows.size() << " cAttackParamPl rows (vtable +0x2F12DE0)";
    Log(summary.str());
    if (rows.empty()) {
        Log("ATKSCAN: no rows resident - load a quest first (col not parsed yet?)");
        g_atkScanRunning.store(false, std::memory_order_relaxed);
        return;
    }

    unsigned dumped = 0;
    std::vector<Row> matches;
    for (const auto& row : rows) {
        auto* p = reinterpret_cast<const std::uint8_t*>(row.addr);
        auto u32 = [p](std::size_t off) {
            return *reinterpret_cast<const std::uint32_t*>(p + off);
        };
        auto f32 = [p](std::size_t off) {
            return *reinterpret_cast<const float*>(p + off);
        };
        if (static_cast<int>(row.idx) == wantIdx) {
            matches.push_back(row);
        }
        if (dumped++ < 120) {
            std::ostringstream m;
            m << "ATKROW ptr=0x" << std::hex << row.addr << std::dec
              << " idx=" << row.idx << " type=" << u32(0x18)
              << " mv=" << f32(0x20) << " fixed=" << f32(0x24)
              << " part=" << f32(0x28) << " stun=" << f32(0x68)
              << " exh=" << f32(0x6c) << " soft=" << f32(0x1c8)
              << " mnt=" << f32(0x1d8) << " flinch=" << u32(0x240);
            Log(m.str());
        }
    }

    if (matches.empty()) {
        std::ostringstream m;
        m << "ATKSCAN: row " << wantIdx
          << " not found among resident rows (col without that row loaded?)";
        Log(m.str());
        g_atkScanRunning.store(false, std::memory_order_relaxed);
        return;
    }
    if (matches.size() > 8) {
        matches.resize(8);
    }

    {
        auto* p = reinterpret_cast<const std::uint8_t*>(matches[0].addr);
        std::ostringstream m;
        m << "ATKHEX row " << matches[0].idx << " @0x" << std::hex
          << matches[0].addr << std::dec << " (u32 view):";
        // 0x270 = the row itself; the tail up to 0xE00 covers the runtime
        // neighbours that ATKNEI showed being read at hit time (row+0xB40..).
        for (std::size_t off = 0; off < 0xE00; off += 64) {
            if (off == 0x270) {
                m << "\n  ---- beyond row: neighbour/context region ----";
            }
            m << "\n  +0x" << std::hex << off << std::dec << ":";
            for (std::size_t w = off; w < off + 64 && w < 0xE00; w += 4) {
                m << ' ' << std::hex << std::setw(8) << std::setfill('0')
                  << *reinterpret_cast<const std::uint32_t*>(p + w) << std::dec;
            }
        }
        Log(m.str());
    }

    if (!g_atkVehInstalled.exchange(true, std::memory_order_relaxed)) {
        AddVectoredExceptionHandler(1, AtkGuardVectored);
        Log("ATKTRACE: VEH installed");
    }
    g_atkTraceLines.store(0, std::memory_order_relaxed);
    g_atkNeighborFaults.store(0, std::memory_order_relaxed);
    g_atkNeiLines.store(0, std::memory_order_relaxed);
    for (std::size_t k = 0; k < matches.size(); ++k) {
        g_atkGuardedRows[k].store(matches[k].addr, std::memory_order_relaxed);
        AtkGuardRowPages(matches[k].addr);
    }
    g_atkGuardedRowCount.store(
        static_cast<unsigned>(matches.size()), std::memory_order_relaxed);
    g_atkRowTarget.store(matches[0].addr, std::memory_order_relaxed);
    g_atkTraceDeadline.store(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                .count() +
            30,
        std::memory_order_relaxed);
    {
        std::ostringstream m;
        m << "ATKTRACE: guarding row " << matches[0].idx << ": "
          << matches.size() << " instance(s) for 30 s - land hits now";
        for (std::size_t k = 0; k < matches.size(); ++k) {
            auto* p = reinterpret_cast<const std::uint8_t*>(matches[k].addr);
            const float mv = *reinterpret_cast<const float*>(p + 0x20);
            m << " [i" << k << " @0x" << std::hex << matches[k].addr << std::dec
              << " mv=" << mv << "]";
        }
        Log(m.str());
    }
    // Self-test: these two reads must each produce one ATKACC line right
    // below. No lines = the guard mechanism itself is broken, not the game.
    {
        std::uint32_t mv = 0;
        std::uint32_t lag = 0;
        const bool ok = AtkSelftestRead(matches[0].addr, &mv, &lag);
        std::ostringstream m;
        m << "ATKTRACE: selftest " << (ok ? "ok" : "FAULTED")
          << " mv=0x" << std::hex << mv << " hitlag_a=0x" << lag
          << " (expect 2 ATKACC lines above)";
        Log(m.str());
    }
    g_atkScanRunning.store(false, std::memory_order_relaxed);
}

static void AtkScanArm() {
    if (g_atkScanRunning.exchange(true, std::memory_order_relaxed)) {
        Log("ATKSCAN: previous scan still running - wait for it to finish");
        return;
    }
    if (g_atkTraceDeadline.load(std::memory_order_relaxed) != 0 ||
        g_atkRowTarget.load(std::memory_order_relaxed) != 0) {
        g_atkScanRunning.store(false, std::memory_order_relaxed);
        Log("ATKTRACE: session still live - wait for 'session over' first");
        return;
    }
    std::ifstream input("nativePC/plugins/ValorProbe.atktrace.txt");
    int idx = 0;
    if (input >> idx) {
        g_atkRowCfg = idx;
    }
    g_atkRowTarget.store(0, std::memory_order_relaxed);
    g_atkTraceDeadline.store(0, std::memory_order_relaxed);
    AtkRemoveGuard();
    std::ostringstream m;
    m << "ATKSCAN: arming for row " << g_atkRowCfg
      << " (config ValorProbe.atktrace.txt)";
    Log(m.str());
    std::thread(AtkScanThread, g_atkRowCfg).detach();
}

static void PollAtkTrace() {
    const auto target = g_atkRowTarget.load(std::memory_order_relaxed);
    if (!target) {
        return;
    }
    const long long deadline = g_atkTraceDeadline.load(std::memory_order_relaxed);
    if (deadline == 0) {
        return;  // scan still running
    }
    const long long now =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    if (now < deadline && g_atkTraceLines.load(std::memory_order_relaxed) < 1500) {
        return;
    }
    g_atkRowTarget.store(0, std::memory_order_relaxed);
    AtkRemoveGuard();
    std::ostringstream m;
    m << "ATKTRACE: session over, " << g_atkTraceLines.load()
      << " accesses logged (cap 1500, neighbor faults "
      << g_atkNeighborFaults.load() << ")";
    Log(m.str());
    g_atkTraceDeadline.store(0, std::memory_order_relaxed);
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

// SHEATHWATCH (2026-10-18): edge logger for the special-sheathe sequence state
// on the weapon object. Reverse findings: FUN_1421237B0 forces the sheathed
// expectation (FUN_142150d10(td,0)) every frame while weapon+0x23C0 or
// weapon+0xA50 is nonzero; +0x23C0 is set by the special-sheathe stage
// machinery (stage handshake events land in +0x23B0/B4/B8; stage advance at
// 0x142144E04 requires newStage > +0x23B4) and its only clear site is the
// full weapon reset FUN_142124590 - so a custom valor chain that never
// completes the vanilla sequence keeps the flag up forever. +0xC71 latch
// polarity (09-25 live): 0 = sheathed, nonzero = drawn. Logs one line only
// when a watched field or the current action changes.
void PollSheathWatch() {
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller || !ModWeaponActive()) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (!IsReadable(player + 0x76B0, sizeof(void*))) {
        return;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
    if (!weapon || !IsReadable(weapon + 0x23C8, 8)) {
        return;
    }
    struct Snap {
        std::uint32_t actSet;
        std::uint32_t actId;
        std::uint8_t c71;
        std::uint8_t a50;
        std::uint8_t c0;
        std::uint8_t c3;
        std::uint8_t s68;
        std::uint8_t ab8;
        std::uint32_t b4;
        bool operator==(const Snap& o) const {
            return actSet == o.actSet && actId == o.actId && c71 == o.c71 &&
                   a50 == o.a50 && c0 == o.c0 && c3 == o.c3 && b4 == o.b4 &&
                   s68 == o.s68 && ab8 == o.ab8;
        }
    };
    Snap snap{};
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction) {
            snap.actSet = g_lastLongSwordAction->actionSet;
            snap.actId = g_lastLongSwordAction->actionId;
        } else {
            snap.actSet = 0xFFFFFFFFu;
            snap.actId = 0xFFFFFFFFu;
        }
    }
    snap.c71 = weapon[0xC71];
    snap.a50 = weapon[0xA50];
    snap.c0 = weapon[0x23C0];
    snap.c3 = weapon[0x23C3];
    snap.s68 = weapon[0x2368];
    snap.ab8 = weapon[0xAB8];
    snap.b4 = *reinterpret_cast<const volatile std::uint32_t*>(weapon + 0x23B4);

    static Snap last{};
    static bool have = false;
    if (have && snap == last) {
        return;
    }
    last = snap;
    have = true;
    std::ostringstream m;
    if (snap.actSet == 0xFFFFFFFFu) {
        m << "SHEATHWATCH act=-";
    } else {
        m << "SHEATHWATCH act=" << snap.actSet << ':' << snap.actId;
    }
    m << " c71=" << static_cast<int>(snap.c71)
      << " a50=" << static_cast<int>(snap.a50)
      << " 23c0=" << static_cast<int>(snap.c0)
      << " 23b4=" << snap.b4
      << " 23c3=" << static_cast<int>(snap.c3)
      << " 2368=" << static_cast<int>(snap.s68)
      << " ab8=" << static_cast<int>(snap.ab8);
    Log(m.str());
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
    PollPoolDiff();
    PollAtkTrace();
    MinerCaptureFrame();
    // F12 arms ATKSCAN/ATKTRACE. Bare F9 is taken by the CSharp loader and
    // Shift+F9 missed presses in the field too; F10/F11 are diff capture and
    // the property histogram. Note: F12 is Steam's default screenshot key -
    // a capture popup alongside the arm is expected and harmless.
    const bool atkKey = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
    if (atkKey && !g_atkKeyWasDown) {
        AtkScanArm();
    }
    g_atkKeyWasDown = atkKey;
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
    PollSheatheCounter();
    PollSheatheBlueSim();
    PollGaugeLock();
    PollSheathWatch();
    PollGuardWindow();
    PollHpLock();
    PollHpWatch();
    PollStaminaControl();

    // Counter window: a spirit-key press while the window is open fires 刚1.
    const bool windowOpen = std::chrono::steady_clock::now() < g_counterWindowUntil;
    const bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    if (windowOpen && ctrlDown && !g_counterCtrlWasDown) {
        g_forceSet = g_forceCfgSet.load(std::memory_order_relaxed);
        g_forceId = g_forceCfgId.load(std::memory_order_relaxed);
        if (!g_forceArmed.exchange(true)) {
            Log("COUNTER INPUT: spirit press inside the counter window - force armed");
        }
    }
    g_counterCtrlWasDown = ctrlDown;

    const bool f6Down = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    const bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    if (f6Down && !g_f6WasDown && shiftDown) {
        // Hot reload: force-action.txt (+ the gauge/sheath sub-configs) is
        // otherwise only read once at plugin init.
        LoadForceActionConfig();
        Log("Force action config reloaded (Shift+F6)");
    } else if (f6Down && !g_f6WasDown && ModWeaponActive()) {
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
    // Sheathe i-frame mining marker (F5): drops a "got hit" marker into the
    // log while the IFRMINE watcher is armed on a watched action.
    if (f5Down && !g_gaugeLockKeyWasDown && !g_sheathMineIds.empty()) {
        std::ostringstream m;
        m << "IFRMINE MARK (user: hit landed)";
        {
            std::scoped_lock lock(g_actionMutex);
            if (g_lastLongSwordAction) {
                m << " act=" << g_lastLongSwordAction->actionSet << ':'
                  << g_lastLongSwordAction->actionId;
            }
        }
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
                    const std::uintptr_t rvaGlow =
                        OverrideOrZero("glowEmit") != 0
                            ? OverrideOrZero("glowEmit")
                            : 0x21224D0;
                    g_fxNotifyA = reinterpret_cast<void* (__fastcall*)(
                        void*, std::uint32_t, std::uint32_t)>(base + rvaA);
                    g_fxNotifyB = reinterpret_cast<void* (__fastcall*)(
                        void*, std::uint32_t, std::uint32_t)>(base + rvaB);
                    g_glowEmit = reinterpret_cast<void (__fastcall*)(void*)>(
                        base + rvaGlow);
                    std::ostringstream fxm;
                    fxm << "FX test notify resolved: ADD670="
                        << (g_fxNotifyA ? "ok" : "null") << " AE0D70="
                        << (g_fxNotifyB ? "ok" : "null") << " glowEmit="
                        << (g_glowEmit ? "ok" : "null");
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
