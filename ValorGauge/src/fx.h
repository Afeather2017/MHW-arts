#pragma once
#include <atomic>
#include <cstdint>

// FUN_140ADD670 detour: every player-emitter body effect passes here. It
// RETURNS the created effect handle in rax - callers may keep using it, so
// the detour must forward it (return-value clobbering crashed the game).
extern void* (*g_originalFxNotify)(void* emitter, std::uint32_t id,
                                   std::uint32_t arg);
// Native fns resolved at install (addresses.txt overridable): the generic
// player emitter, the AE0D70 variant, the red-glow emitter FUN_1421224d0 and
// the blade-aura transition FUN_142110a90.
extern void* (__fastcall* g_fxNotifyA)(void*, std::uint32_t, std::uint32_t);
extern void* (__fastcall* g_fxNotifyB)(void*, std::uint32_t, std::uint32_t);
extern void (__fastcall* g_glowEmit)(void* weapon);
extern void* (__fastcall* g_auraSet)(void*, std::uint32_t, std::uint32_t,
                                     std::uint32_t, std::uint32_t);
// FX test (F7 fires once, Shift+F7 reloads ValorGauge.fx-test.txt).
extern std::atomic_int g_fxFireFunc;
extern std::atomic<std::uint32_t> g_fxFireId;
extern int g_fxCfgFunc;
extern std::uint32_t g_fxCfgId;
extern bool g_fxKeyWasDown;

void* __fastcall FxNotifyHook(void* emitter, std::uint32_t id,
                              std::uint32_t arg);
void LoadFxTestConfig();
void TryQueuedFx(const void* entity);
