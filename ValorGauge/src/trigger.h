#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>

// Force-action: F6 (or a clang trigger) arms a one-shot flag; the property
// getter hook consumes it ON the game thread by calling DoAction directly -
// the same entry the FSM's own link execution uses. No game-memory writes.
extern std::atomic_bool g_forceArmed;
extern std::uint32_t g_forceSet;
extern std::uint32_t g_forceId;
// force-action.txt 配置默认值(刚1 目标)。g_forceSet/g_forceId 是"待发目标",
// 会被 sheath 规则/吞单武装覆盖; 弹反链武装前从这里恢复, 两者分离后互不污染。
extern std::atomic_uint32_t g_forceCfgSet;
extern std::atomic_uint32_t g_forceCfgId;
extern float g_forceInvulnSec;

// Counter window (见切 success / clang): while open, a spirit-key press fires
// the counter; the window also carries the no-hit injection.
extern double g_counterWindowSec;
extern std::chrono::steady_clock::time_point g_counterWindowUntil;
extern bool g_counterAutoForce;
extern bool g_counterCtrlWasDown;
// One force per 见切 action: every trigger source is blocked until the next
// 86/101/102 entry re-arms the latch.
extern bool g_clangLatch;
extern std::atomic_uint32_t g_vseekLogCount;

void OpenCounterWindow(const char* reason);
void PollModTriggers();
void TryQueuedForceWrite(const void* entity);
bool InstallCondEvalHook();

// Hot-reload orchestrator (dllmain.cpp): force-action.txt + every sub-config
// of every module. Bound to Shift+F6 in the overlay and run once at install.
void LoadForceActionConfig();
