#pragma once
#include <atomic>
#include <cstdint>

using DmgCalcFn = long long(__fastcall*)(long long, long long, std::uint32_t*);
extern DmgCalcFn g_originalDmgCalc;
// Set by whichever DmgCalcHook is installed while inside a hit-dispatch call.
// A type-6 dispatch that finishes without this flag = the hit was negated.
extern bool g_damageThisDispatch;  // game thread only

// The DealDamage signature carries no attacker, so the hit's owner is the
// attacker latched in DmgCalcHook. Game thread only.
extern long long g_lastHitAttacker;
// Set by the DealDamage hook for every applied hit: was the victim a large
// monster? Read by GaugeAddHook to keep the native spirit refill monster-only.
extern std::atomic<bool> g_lastAppliedTargetMonster;
// Multiplayer/terrain skip counters, diagnostics only.
extern std::atomic_uint32_t g_hitChargeSkips;
extern std::atomic_uint32_t g_nonMonsterSkips;

bool IsLargeMonsterId(std::int32_t id);
bool IsChargeableTargetId(std::int32_t id);
void NotifyPlayerHitConnected(long long attacker);
void NotifyAppliedHitCharges(long long targetEm, int damage, int isCrit);
void LoadHitChargeConfig();
void MotionTraceTick(const void* entity);

long long __fastcall DmgCalcHook(long long a1, long long a2, std::uint32_t* a3);

using DealDamageFn = void(__fastcall*)(long long, int, float*, int, int, int,
                                       int, char, int);
extern DealDamageFn g_originalDealDamage;
void __fastcall DealDamageHook(long long targetEm, int damage, float* position,
                               int isTenderized, int isCrit, int unk0, int unk1,
                               char unk2, int attackId);

using DmgFormulaFn = void(__fastcall*)(long long, long long*, int, int, int,
                                       float*);
extern DmgFormulaFn g_originalDmgFormula;
void __fastcall DmgFormulaHook(long long context, long long* hitCtx, int a3,
                               int a4, int a5, float* out);
