#pragma once
#include <atomic>
#include <cstdint>
#include <vector>

// Per-action gauge rules (nativePC/plugins/ValorGauge.gauge-rules.txt):
// rewrite / cost / gate / regen / timer / timerhit / gainhit / chargehit /
// chargehit-from / chargestart / fx / trail / option lines - the full syntax
// reference lives at the GaugeRule definition in gauge.cpp and in
// docs/gauge-valor.md.
struct GaugeRule {
    enum class Type { Rewrite, Cost, Gate, Regen, Timer, TimerHit, Fx, Trail, GainHit,
                      ChargeHit, ChargeFrom, ChargeStart };
    Type type = Type::Rewrite;
    std::int32_t set = 3;
    std::int32_t id = 0;
    float a = 0.0f;  // Rewrite: new delta; Cost: amount; Gate: minGauge; Regen: seconds
    float b = 0.0f;  // Rewrite: origVal; Gate: nohitSec; Regen: variant (0=hb, 1=iai)
    bool hasOrig = false;
    bool hasValor = false;  // GainHit/ChargeHit 4th token: the amount at level >= 2
    // ChargeHit context cell ("82@98"): only applies when the previous
    // action was 3:<ctxPrev>. Resolution order: ctx cell > chargehit-from
    // default > plain move row > wildcard.
    bool hasCtx = false;
    std::int32_t ctxPrev = 0;
    float rate = 0.0f;      // Regen optional 5th token: override the rate param
    bool hasRate = false;
    std::uint32_t fxId = 0; // Fx: player-emitter effect id (0x prefix accepted)
    float critMult = 1.0f;  // ChargeHit/ChargeFrom optional 5th token: the
                            // amount multiplier on crit draws (1.0 = none)
};

extern std::vector<GaugeRule> g_gaugeRules;
// option regen-freeze 1: while a regen timer is active negative deltas are zeroed.
extern std::atomic_bool g_gaugeRegenFreeze;
// option charge-takeover 1: native TIML hit-gains zeroed so gainhit rows are
// the only charging source.
extern std::atomic_bool g_gaugeChargeTakeover;
// Caller RVA of the accumulator call inside the TIML gain dispatcher
// (GAUGEEVT +0x2124f13). addresses.txt override: gaugeTimlGain=0xRVA.
extern std::uintptr_t g_timlGainCallerRva;
// chargehit-dmg scaling (bar units, 0 = flat amounts).
extern std::atomic<float> g_chargeHitDmgScale;
extern std::atomic<float> g_chargeHitDmgCap;

// FUN_142122410 detour: the ONLY float writer of weapon+0x2368 (spirit gauge).
extern float (*g_originalGaugeAdd)(void* weapon, float value);
extern std::atomic<float> g_gaugeLogMinAbs;

const GaugeRule* FindGaugeRule(GaugeRule::Type type, std::int32_t set,
                               std::int32_t id);
const GaugeRule* FindGaugeRuleLoose(GaugeRule::Type type, std::int32_t set,
                                    std::int32_t id);
std::uint8_t* GaugeRuleWeapon();
void InjectNoHitLocked(float seconds);
void ApplyActionGaugeRules(std::int32_t actionSet, std::int32_t actionId);
void ApplyBeginGaugeRules(std::int32_t actionSet, std::int32_t actionId);
void PollGaugeBeginRules(const void* entity);
void PollTrailRules(const void* entity);
void PollGaugeWatch();
void LoadGaugeRulesConfig();
void LoadGaugeLogConfig();
float __fastcall GaugeAddHook(void* weapon, float value);
