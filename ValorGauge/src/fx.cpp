// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L145-L179, L2853-L2922, L4367-L4435. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

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
// FUN_142110a90 (RVA 0x2110A90): blade-aura transition - the game calls it
// with (weapon, fxId, 0, 0, 0) then latches the steady color at weapon+0xad4
// (to white: fx 100/ad4 100, yellow: 0x65/0x66, red: 0x68/0x69).
void* (__fastcall* g_auraSet)(void*, std::uint32_t, std::uint32_t,
                              std::uint32_t, std::uint32_t) = nullptr;
std::atomic_int g_fxFireFunc{-1};
std::atomic<std::uint32_t> g_fxFireId{0};
int g_fxCfgFunc = 0;
std::uint32_t g_fxCfgId = 0x7D2;
bool g_fxKeyWasDown = false;

#ifndef VALOR_PROBE
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

#endif // !VALOR_PROBE
