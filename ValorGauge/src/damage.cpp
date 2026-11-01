// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L2103-L2119, L2121-L2126, L2134-L2159, L2932-L3042, L3046-L3047, L3050-L3061, L3062-L3066, L3068-L3142, L3143-L3148, L3150-L3154, L3157-L3177, L3179-L3448. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

// --- 刚1 clang detector (hit dispatcher) --------------------------------------
// FUN_141f71960 routes every hit attempt; 0xD returns are the per-frame
// ambient no-collision checks. Live diagnostics (log-verified): during 见切
// (3:86) a negated hit (the clang) returns a NON-0xD code and the damage calc
// (FUN_141f66dd0) is never entered; a real hit enters the damage calc. So:
// clang = non-0xD return while action is 3:86 and no damage processing ran
// for this dispatch. Fires on the game thread at the clang frame.
// FUN_141f66dd0 = damage calculation entry (3 args). a1 is the ATTACKER
// entity: its body dereferences a1+0x7D20 (the player mgr pointer), and for
// player-dealt hits a1 equals the player pointer. a2 is a small per-hit
// victim CONTEXT object handed down by the dispatcher (FUN_141f71960 damage
// case passes its interaction's param_2[3]) - log-proven NOT the enemy em
// (one real monster hit: a2=0xE51F240 vs DealDamage em=0x41C2EA1D30), which
// is why target filtering never worked from here and moved to the
// DealDamage hook. The hook is session-wide -
// in multiplayer it fires for every hunter's and monster's connected hits -
// so the mod filters a1 against the locked local player before charging.

DmgCalcFn g_originalDmgCalc = nullptr;
// Set by whichever DmgCalcHook is installed while inside a hit-dispatch call.
// A type-6 dispatch that finishes without this flag = the hit was negated
// (见切-style arbitration rejection or a no-hit window eating the hit).
bool g_damageThisDispatch = false; // game thread only


#ifndef VALOR_PROBE
// --- Hit-based valor charging -------------------------------------------------
// Hits from other entities in multiplayer, counted for diagnostics only.
std::atomic_uint32_t g_hitChargeSkips{0};
// Connected hits on non-living targets (destructible terrain spawned by
// monster attacks, barrels, gimmicks), counted for diagnostics only.
std::atomic_uint32_t g_nonMonsterSkips{0};

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

// Chargehit resolution (2026-10-25 redesign): pick the row for this hit.
//   1. ctx cell      "chargehit 3 82@98 ..."  - move 82 while prev is 3:98
//   2. from default  "chargehit-from 3 98 ..." - any move while prev is 3:98
//   3. plain move    "chargehit 3 82 ..."      - move 82 anywhere
//   4. wildcard      "chargehit 3 0 ..."       - every move
// *tierOut labels which tier matched (chargehit log). nullptr = no charging.
static const GaugeRule* ResolveChargeHit(std::int32_t actSet,
                                         std::int32_t actId,
                                         std::int32_t prevSet,
                                         std::int32_t prevId,
                                         const char** tierOut) {
    for (const auto& r : g_gaugeRules) {
        if (r.type == GaugeRule::Type::ChargeHit && r.set == actSet &&
            r.hasCtx && r.ctxPrev == prevId && r.id == actId) {
            *tierOut = "ctx";
            return &r;
        }
    }
    if (prevSet == actSet && prevId >= 0) {
        for (const auto& r : g_gaugeRules) {
            if (r.type == GaugeRule::Type::ChargeFrom && r.set == actSet &&
                r.id == prevId) {
                *tierOut = "from";
                return &r;
            }
        }
    }
    for (const auto& r : g_gaugeRules) {
        if (r.type == GaugeRule::Type::ChargeHit && r.set == actSet &&
            !r.hasCtx && r.id == actId && actId != 0) {
            *tierOut = "move";
            return &r;
        }
    }
    for (const auto& r : g_gaugeRules) {
        if (r.type == GaugeRule::Type::ChargeHit && r.set == actSet &&
            !r.hasCtx && r.id == 0) {
            *tierOut = "wild";
            return &r;
        }
    }
    return nullptr;
}

// Large-monster target filter (2026-10-29): destructible terrain spawned by
// monster attacks reaches every charging gate like any other hit and was
// charging every gauge. HunterPie's damage meter solves the same problem with
// an id bitset on the enemy instance (Games/World/Utils/WorldUtils.cpp; id is
// int32 @ em+0x12280 - the game's own DealDamage FUN_141cc5f80 receives the
// em as arg1 and compares it right in the prologue). All charging therefore
// lives behind the DealDamage hook, where the em is exact - probing it from
// the dmgCalc victim object failed in-game (log 2026-10-29: a2 is a small
// victim context object, NOT the em - one real monster hit logged a2=0xE51F240
// vs em=0x41C2EA1D30 - and candidate reads flipped verdicts hit to hit).
constexpr std::uint64_t kBigMonsterBitsetLo =
    0b1110'1000000010'0000000000'1111111111'1111111111'1111111111'1010010011ULL;
constexpr std::uint64_t kBigMonsterBitsetHi =
    0b00000000'0000000000'0000000011'1111111111'1110000011'1111111111'111111ULL;
// Bit 59 (low set) is the training-area large pillar - deliberately counted,
// the gauge is a combat-zone session state and the pillar must charge.

bool IsLargeMonsterId(std::int32_t id) {
    if (id < 0) return false;
    const auto u = static_cast<std::uint32_t>(id);
    if (u < 64) return (kBigMonsterBitsetLo >> u) & 1ULL;
    if (u < 128) return (kBigMonsterBitsetHi >> (u - 64)) & 1ULL;
    return false;
}

// The chargeable-target set: large monsters plus the training-area practice
// targets. Vanilla refills the spirit gauge on the training barrels too
// (user call 2026-10-29: normal attacks on them must keep charging) - their
// em ids are 57/58 (log: DDART id=57/58 suppressed before this exception,
// id=59 pillar already inside the bitset).
bool IsChargeableTargetId(std::int32_t id) {
    return IsLargeMonsterId(id) || id == 57 || id == 58;
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
    // Charging moved 2026-10-29(2): every gauge charge (timerhit/gainhit/
    // chargehit/legacy) now runs behind the DealDamage hook's exact em filter
    // (NotifyAppliedHitCharges below). The dmgCalc victim object (a2) proved
    // unusable for target filtering - see the IsLargeMonsterId comment block.
}

// FUN_141cc5f80 = HunterPie's DealDamage signature (void, 9 args): applied-
// damage entry, arg1 = victim em, arg2 = int damage, arg9 = attack id.

DealDamageFn g_originalDealDamage = nullptr;
// The DealDamage signature carries no attacker, so the hit's owner is the
// attacker latched in DmgCalcHook (the dispatcher runs the calc on every
// hunter's and monster's hit before the apply). Game thread only.
long long g_lastHitAttacker = 0;
// Set by the DealDamage hook for every applied hit: was the victim a large
// monster? The native spirit-gauge refill (GaugeAddHook, +0x2124f13 caller)
// reads this to stay monster-only - the game refills 0x2368 for ANY applied
// damage, terrain included (log-proven: GAUGEEVT val=0.26 behind terrain
// hits). DealDamage runs before the refill within one hit (DDART -> GAUGEEVT
// ordering in the log). True = no verdict yet, vanilla behavior.
std::atomic<bool> g_lastAppliedTargetMonster{true};

// MOTIONTRACE: log current-motion id changes (instance+0x30, entry|0xC000)
// during set-3 actions so mid-action motion switches (FSM LinkMotion 衔接)
// become visible against the damage-window timeline.
static std::atomic<std::int32_t> g_motionTraceLeft{150};
static std::uint32_t g_motionTraceLast = 0xFFFFFFFFu;

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
void MotionTraceTick(const void* entity) {
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
    g_lastHitAttacker = a1;  // DealDamageHook reads this as the hit's owner
    NotifyPlayerHitConnected(a1);
    LogHitFrame(a1);
    return g_originalDmgCalc(a1, a2, a3);
}

// Attack-field hunt round 3 (2026-10-29): stop guessing offsets - hook the
// damage formula itself and dump its attack-context struct (built by
// FUN_1402c1050) as both float and int views. The UI attack value (user
// reference: 1366/1399) must appear in here because the formula multiplies
// it. First 10 formula calls after each (re)load.

DmgFormulaFn g_originalDmgFormula = nullptr;
// Latched per hit: the formula's attack context. Field +0x34 read ~131-212
// and varies per move - either pure MV or attack*MV/100 (decide by the
// food-buff test: same move, before/after an attack change, +34 moves
// proportionally = attack-bearing). DealDamage runs AFTER the formula, so
// the latch is fresh when the charge reads it. Game thread only.
const long long* g_lastFormulaCtx = nullptr;
void __fastcall DmgFormulaHook(long long context, long long* hitCtx, int a3,
                               int a4, int a5, float* out) {
    // Keepalive latch only: the atkMV route (charge = damage x MV / (ctx+0x34
    // x 100), attack cancels exactly) is dormant - 2026-10-29 concluded on
    // the manual charge-dmg divisor instead. The latch costs one store per
    // hit and keeps that route one config line away from resurrection.
    g_lastFormulaCtx = hitCtx;
    g_originalDmgFormula(context, hitCtx, a3, a4, a5, out);
}

// Attack-field hunt round 4 retired (2026-10-29): FUN_1402c1050's stats
// object X (= *(mgr+0x10), read by getters 0x141216910/920 at X+0x64/X+0x60)
// holds +0x60=300.0/+0x64=1.0 constant - not the combined attack either.
// Sig stays in sig_check.py (atkCtxBuilder, RVA 0x2C1050) for any revival.

// Every gauge charge lives here now (2026-10-29(2)): the applied-damage
// point. arg1 IS the victim em - its monster id at +0x12280 reads exactly
// (log-proven: one real hit had dmgCalc a2=0xE51F240 vs this em=0x41C2EA1D30,
// so the a2 candidate probing was judging the wrong object). Terrain and
// destructible gimmicks reach it too and show damage numbers, but their id
// is not a large-monster id, so timerhit/gainhit/chargehit/art/legacy all
// drop here for them; the native spirit refill is latched off via
// g_lastAppliedTargetMonster (GaugeAddHook zeroes +0x2124f13 gains).
void NotifyAppliedHitCharges(long long targetEm, int damage, int isCrit) {
    void* controller = g_lockedPlayerController;
    if (!controller) {
        return;
    }
    // Crit-normalized damage (2026-10-29, user request): the charge should
    // not swing with crit RNG. World crits are x1.25 physical, so divide the
    // crit draws back down before any damage-scaled charging. Non-crit
    // negative-affinity draws (x0.75) have no flag here - rare, left as is.
    const float critDiv = isCrit ? 1.25f : 1.0f;
    const float dmgNorm =
        static_cast<float>(damage) / critDiv;
    const auto localPlayer = reinterpret_cast<long long>(
        static_cast<std::uint8_t*>(controller) - 0x61C8);
    const bool localAttacker = g_lastHitAttacker == localPlayer;
    std::int32_t emId = -1;
    if (IsReadable(reinterpret_cast<const void*>(targetEm + 0x12280),
                   sizeof(std::int32_t))) {
        emId = *reinterpret_cast<const volatile std::int32_t*>(targetEm + 0x12280);
    }
    // First verdicts log the latch quality: a real player hit that ever logs
    // "skip" means the calc -> apply ordering assumption needs revisiting.
    // crit/dmgNorm: the charge damage is crit-normalized (see dmgNorm above).
    static std::atomic_uint32_t ddLogs{0};
    if (ddLogs.load(std::memory_order_relaxed) < 12) {
        ddLogs.fetch_add(1, std::memory_order_relaxed);
        std::ostringstream m;
        m << "DDART em=0x" << std::hex << std::uppercase << targetEm << std::dec
          << " id=" << emId << " dmg=" << damage
          << " crit=" << isCrit << " dmgNorm=" << dmgNorm
          << " latch=" << (localAttacker ? "MATCH" : "skip");
        Log(m.str());
    }
    if (!localAttacker) {
        // Not our hit: the latch goes false so the native refill stays quiet
        // if anything of ours would fire behind someone else's apply.
        g_lastAppliedTargetMonster.store(false, std::memory_order_relaxed);
        return;
    }
    const bool monster = IsChargeableTargetId(emId);
    // Latch BEFORE any charging - the native spirit refill (GaugeAddHook,
    // +0x2124f13) fires later within this same hit and reads this.
    g_lastAppliedTargetMonster.store(monster, std::memory_order_relaxed);
    if (!monster) {
        g_nonMonsterSkips.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    if (!ModWeaponActive()) {
        return;
    }
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
        // timerhit: connected hits of the current action extend the
        // level-drop countdown. Independent of the valor-charge gates below
        // so it also runs in valor mode / with charging off.
        if (const auto* rule =
                FindGaugeRuleLoose(GaugeRule::Type::TimerHit, actSet, actId)) {
            ExtendLevelDropTimer(
                static_cast<std::uint8_t*>(controller) - 0x61C8, rule->a,
                actSet, actId, true);
        }
        // gainhit (moved 2026-10-29(2) from the dmgCalc gate): the applied-
        // hit gate is upstream, so the amount goes straight through the
        // accumulator. gainhit feeds the 练气 gauge (0x2368) only - the
        // 76/77 rows keep the 刚1 gauge gate topped up.
        if (const auto* gain =
                FindGaugeRuleLoose(GaugeRule::Type::GainHit, actSet, actId)) {
            auto* player =
                static_cast<std::uint8_t*>(controller) - 0x61C8;
            auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
            if (weapon && g_originalGaugeAdd &&
                IsReadable(weapon + 0x2368, sizeof(float)) &&
                IsReadable(weapon + 0x9F8, sizeof(std::uint32_t)) &&
                *reinterpret_cast<const volatile std::uint32_t*>(
                    weapon + 0x9F8) ==
                    g_modWeaponType.load(std::memory_order_relaxed)) {
                const auto level =
                    *reinterpret_cast<const volatile std::int32_t*>(
                        weapon + 0x2370);
                // Defense in depth: only touch sane levels (struct-shift
                // alarm would read garbage).
                if (level >= 0 && level <= 3) {
                    const bool valor = level >= 3;
                    const float amount =
                        (gain->hasValor && valor) ? gain->b : gain->a;
                    const float before = *reinterpret_cast<volatile float*>(
                        weapon + 0x2368);
                    g_originalGaugeAdd(weapon, amount);
                    // The 刚1 guard tail reverts ANY drift from its expected
                    // gauge - including accumulator-confirmed positives (that
                    // policy is what suppresses the 见切 refill). Account for
                    // our own addition so counter-chain hits keep their gain.
                    if (g_gang1Phase.load(std::memory_order_acquire) == 2) {
                        const float e =
                            g_gang1Expected.load(std::memory_order_relaxed);
                        g_gang1Expected.store(
                            std::clamp(e + amount, 0.0f, 1.0f),
                            std::memory_order_relaxed);
                    }
                    const float after = *reinterpret_cast<volatile float*>(
                        weapon + 0x2368);
                    std::ostringstream m;
                    m << "GAUGERULE gainhit act=" << actSet << ':' << actId
                      << " +" << amount << (valor ? " (valor)" : "")
                      << " gauge=" << before << "->" << after;
                    Log(m.str());
                }
            }
        }
        // chargehit (2026-10-24 design round 2, moved 2026-10-29(2)): the
        // valor charge bar IS the level-drop timer (weapon+0x2374, the
        // user's "气刃值"). Connected hits push the bar up by <amount>
        // (optional 4th token = the amount in valor mode); a charge that
        // CROSSES into full while WHITE promotes straight to RED = valor
        // entry (2026-10-25 plan B). Entering white always starts the bar
        // near empty, so "full" is only reachable by hitting. The previous
        // action comes from the game's own controller record (+0xC4/+0xC8);
        // ResolveChargeHit picks ctx-cell > from-default > move-row >
        // wildcard so 勇气纳刀后接的招式 can carry per-move amounts.
        const char* tier = nullptr;
        std::int32_t prevSet = -1;
        std::int32_t prevId = -1;
        if (IsReadable(controller, 0xD0)) {
            prevSet = *reinterpret_cast<const volatile std::int32_t*>(
                static_cast<std::uint8_t*>(controller) + 0xC4);
            prevId = *reinterpret_cast<const volatile std::int32_t*>(
                static_cast<std::uint8_t*>(controller) + 0xC8);
        }
        if (const auto* rule = ResolveChargeHit(actSet, actId, prevSet,
                                                prevId, &tier)) {
            auto* player =
                static_cast<std::uint8_t*>(controller) - 0x61C8;
            auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
            if (weapon && IsReadable(weapon + 0x2374, sizeof(float)) &&
                IsReadable(weapon + 0x9F8, sizeof(std::uint32_t)) &&
                *reinterpret_cast<const volatile std::uint32_t*>(
                    weapon + 0x9F8) ==
                    g_modWeaponType.load(std::memory_order_relaxed)) {
                const auto level =
                    *reinterpret_cast<const volatile std::int32_t*>(
                        weapon + 0x2370);
                if (level >= 0 && level <= 3) {
                    if (level == 0) {
                        // 未开刃 -> 白刃, charge bar starts near empty.
                        SetSpiritLevel(player, weapon, 1, "charge", 0.05f);
                    }
                    const bool valor = level >= 3;
                    float amount =
                        (rule->hasValor && valor) ? rule->b : rule->a;
                    // chargehit-dmg: scale the resolved amount (any tier)
                    // with the crit-normalized damage; capped in bar units.
                    const float chScale =
                        g_chargeHitDmgScale.load(std::memory_order_relaxed);
                    if (chScale > 0.0f) {
                        amount = std::min(
                            g_chargeHitDmgCap.load(std::memory_order_relaxed),
                            amount * dmgNorm / chScale);
                    }
                    // Per-row crit multiplier: hand-written rows pay a bonus
                    // on crit draws ("chargehit ... <amt> [valor] [critMult]").
                    const bool critApplied =
                        isCrit && rule->critMult != 1.0f;
                    if (critApplied) {
                        amount *= rule->critMult;
                    }
                    auto* timer =
                        reinterpret_cast<volatile float*>(weapon + 0x2374);
                    const float prev = *timer;
                    const float next = std::min(1.0f, prev + amount);
                    *timer = next;
                    // Crossing into full is what promotes - an already-
                    // full bar (fresh red) never re-fires.
                    if (next >= 0.999f && prev < 0.999f &&
                        *reinterpret_cast<const volatile std::int32_t*>(
                            weapon + 0x2370) == 1) {
                        SetSpiritLevel(player, weapon, 3, "charge-full");
                    }
                    std::ostringstream m;
                    m << "GAUGERULE chargehit act=" << actSet << ':'
                      << actId << " +" << amount << (valor ? " (valor)" : "")
                      << (critApplied ? " critx" : "")
                      << (critApplied ? std::to_string(rule->critMult) : "")
                      << " dmg=" << damage
                      << " bar=" << prev << "->" << next
                      << " prev=" << prevSet << ':' << prevId
                      << " tier=" << tier;
                    Log(m.str());
                }
            }
        }
    }
    // ARTGAIN (战技槽): exact row wins over the wildcard, whiffs never reach
    // this code. Capped at 100; the FULL crossing is logged once per fill.
    // charge-dmg: when enabled the row amount scales with the applied damage
    // (damage/100 with the shipped config = row amount at a 100-damage hit).
    if (g_artEnabled.load(std::memory_order_relaxed) && hasAct && actSet == 3) {
        float pct = ArtChargePercentFor(actSet, actId);
        const float scale = g_artDmgScale.load(std::memory_order_relaxed);
        if (scale > 0.0f) {
            pct = std::min(g_artDmgCap.load(std::memory_order_relaxed),
                           pct * dmgNorm / scale);
        }
        if (pct > 0.0f) {
            const float before = g_artGauge.load(std::memory_order_relaxed);
            const float next = std::min(100.0f, before + pct);
            g_artGauge.store(next, std::memory_order_relaxed);
            static std::atomic_uint32_t artGainLogs{0};
            const auto n = artGainLogs.fetch_add(1, std::memory_order_relaxed);
            if (n < 8 || (before < 100.0f && next >= 100.0f)) {
                std::ostringstream m;
                m << "ARTGAIN act=" << actSet << ':' << actId << " +" << pct
                  << "% dmg=" << damage << " gauge=" << static_cast<int>(before + 0.5f)
                  << "->" << static_cast<int>(next + 0.5f)
                  << (next >= 100.0f && before < 100.0f ? " FULL" : "");
                Log(m.str());
            }
        }
    }
    // Legacy HITCHARGE (round-1 valor bar): dormant with hit-charge=-1,
    // moved here so every charge shares the one applied-hit gate.
    const int percent = g_hitChargePercent.load(std::memory_order_relaxed);
    if (percent > 0 && !g_valorMode.load(std::memory_order_relaxed) &&
        LockedPlayerActionChargesValor()) {
        const auto oldCharge = g_valorCharge.load();
        const auto newCharge = static_cast<std::uint32_t>(
            std::min(100, static_cast<int>(oldCharge) + percent));
        if (newCharge != oldCharge) {
            g_valorCharge = newCharge;
            static std::atomic_uint32_t hitChargeLogs{0};
            if (hitChargeLogs.load(std::memory_order_relaxed) < 64) {
                hitChargeLogs.fetch_add(1, std::memory_order_relaxed);
                std::ostringstream m;
                m << "HITCHARGE " << oldCharge << "% -> " << newCharge
                  << "% (+" << percent << ") attacker=0x" << std::hex
                  << std::uppercase << localPlayer;
                Log(m.str());
            }
            if (newCharge == 100) {
                ActivateValorMode(controller);
            }
        }
    }
}

void __fastcall DealDamageHook(long long targetEm, int damage, float* position,
                               int isTenderized, int isCrit, int unk0, int unk1,
                               char unk2, int attackId) {
    NotifyAppliedHitCharges(targetEm, damage, isCrit);
    g_originalDealDamage(targetEm, damage, position, isTenderized, isCrit,
                         unk0, unk1, unk2, attackId);
}


#endif // !VALOR_PROBE
