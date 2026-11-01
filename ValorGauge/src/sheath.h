#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#include "game_types.h"

// --- Sheathed-state draw rewrite (收刀拔刀刚1) -------------------------------
// Vanilla routes sheathed R2 to {3,11} in code + per-weapon static tables; the
// rewrite happens on the request itself, in DoActionHook, before the tracker
// sees it. Gate (user rule): spirit level >= reqLevel and gauge above the
// threshold. Config nativePC/plugins/ValorGauge.sheath-rewrite.txt.
extern std::atomic<std::uint32_t> g_sheathRewriteToId;
extern std::atomic<std::uint32_t> g_sheathRewriteHoldVK;
extern std::atomic<std::uint32_t> g_sheathRewriteReqLevel;
extern std::atomic<float> g_sheathRewriteMinGauge;
extern std::atomic<float> g_sheathRewriteNohitSec;
extern std::vector<std::uint32_t> g_sheathRewriteFromIds;
extern std::mutex g_sheathRewriteMutex;

// 刚1 gauge accounting state machine: 0 idle / 1 waiting / 2 guard tail.
// The cost is 0.15 UNCONDITIONALLY; the settle+guard revert direct writes
// (the vanilla whiff full-clear). See the block comment at g_gang1Phase in
// sheath.cpp.
extern std::atomic<std::int32_t> g_gang1Phase;
extern std::atomic<float> g_gang1Snapshot;
extern std::atomic<float> g_gang1Expected;
extern std::atomic<std::int64_t> g_gang1DeadlineMs;
extern std::atomic_bool g_gang1Bit15AtArm;

void LoadSheathRewriteConfig();
void ApplySheathRewrite(void* controller, ActionInfo* action);
void ArmGang1GaugeScope(void* controller);
void PollGang1GaugeScope();
void LoadDrawGang1Config();
void PollDrawGang1(const void* entity);
void PollSheathWatch();
