#pragma once
#include <atomic>
#include <cstdint>

// Probe-build observation modules: state logger, pool-query/damage-calc/hit-
// dispatch diagnostics, RXN watcher, POOLDIFF. Nothing here runs in the mod
// build (definitions are #ifdef VALOR_PROBE); declarations are shared so both
// builds see the same headers.
using Aee050Fn = float(__fastcall*)(void* pool, int reactionType);
extern Aee050Fn g_originalAee050;

// DoActionHook probe diagnostics sampler cap counter.
extern std::atomic_uint32_t g_actionDiagnosticCount;
// Reaction-pool latch recorded by Aee050Hook, consumed by the F10 diff capture.
extern std::atomic<std::uintptr_t> g_lastPlayerPool;

// RXN watcher watch-list bound (array bounds below must match).
constexpr int kMaxWatchIds = 6;
extern std::uint32_t g_triggerId;
extern std::uint32_t g_triggerIndex;

using HitDispatchFn = std::int32_t* (__fastcall*)(std::int64_t*, std::int64_t*);
extern HitDispatchFn g_originalHitDispatch;

// State logger (ValorGauge.state-address.txt, F7 reload).
extern std::atomic_bool g_stateLoggerEnabled;
extern std::atomic_uintptr_t g_stateAddress;
void LoadStateAddress();
void PollStateLogger();

float __fastcall Aee050Hook(void* pool, int reactionType);
std::int32_t* __fastcall HitDispatchHook(std::int64_t* a1, std::int64_t* a2);

// RXN watcher: reaction manager + configurable FSM property polling. Array
// bounds must match kMaxWatchIds in probe.cpp.
extern std::uint64_t g_rxnFrame;
extern std::uint32_t g_rxnStandby;
extern std::uint8_t g_rxnGate;
extern float g_rxnNohit;
extern bool g_rxnValid;
extern std::uint32_t g_rxnRowMissLog;
extern int g_watchCount;
extern std::uint32_t g_watchIds[kMaxWatchIds];
extern std::int16_t g_watchPrev[kMaxWatchIds][3];
extern std::int16_t g_watchCur[kMaxWatchIds][3];
extern std::uint32_t g_watchKeys[kMaxWatchIds];
extern std::int16_t* g_watchCounters[kMaxWatchIds];

std::int16_t* ResolvePropCounters(std::uint8_t* player, std::uint32_t id,
                                  std::uint32_t* keyOut);
void LogKeyTableDump(std::uint8_t* player);
void LoadPropWatchConfig();
void PollReactionWatch();
void PollPoolDiff();
