#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>

// Legacy (pre-hit-charge) valor bar + mode; the charge percent is set by
// ValorGauge.hit-charge.txt (>0 per connected hit, 0 = action-start legacy,
// <0 = charging off).
extern std::atomic_uint32_t g_valorCharge;
extern std::atomic_bool g_valorMode;
extern std::atomic_int g_hitChargePercent;

// Gauge-lock experiment (2026-09-24): per-frame overwrite of an LS gauge
// float (target 1 = weapon+0x2374, 2 = weapon+0x2368). F5 toggles at runtime;
// both fields are always watched (drift = the game wrote underneath).
extern std::atomic<int> g_gaugeLockTarget;
extern std::atomic<float> g_gaugeLockValue;
extern std::atomic_bool g_gaugeLockEnabled;
extern bool g_gaugeLockKeyWasDown;
// DrawGauge(Present)-thread only state. Index 0 = +0x2374, 1 = +0x2368.
extern float g_gaugeLockLastWritten[2];
extern float g_gaugeWatchDrift[2];
extern std::uint32_t g_gaugeWatchFrames[2];
extern std::chrono::steady_clock::time_point g_gaugeLockNextReport;

void ActivateValorMode(void* controller);
void PollValorLifecycle();
void LoadGaugeLockConfig();
void PollGaugeLock();
