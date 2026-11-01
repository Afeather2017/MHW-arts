// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L137-L144, L180-L244, L266-L296, L2199-L2211, L2213-L2390, L2392-L2596, L2598-L2625, L2627-L2634, L2755-L2851, L4534-L4629, L4748-L4797, L4799-L4839, L4841-L4879. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

// Gauge-event logger (GaugeAddHook): log every gauge delta with the caller
// RVA so one play session maps "which move moves the gauge by how much".
// Config nativePC/plugins/ValorGauge.gauge-log.txt = "<minAbsValue>"
// (default 0.01 filters the per-frame 0.000556 drains; 0 logs everything).
// FUN_142122410 returns the clamped gauge in xmm0 - the type must say float
// so the detour forwards it (return-value clobbering = caller eats garbage).
float (*g_originalGaugeAdd)(void* weapon, float value) = nullptr;
std::atomic<float> g_gaugeLogMinAbs{0.01f};

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
//   gainhit <set> <id> <amount> [valor]   - add <amount> gauge on CONNECTED
//       hits of the action (same hit gate as timerhit; whiffs never gain).
//       Goes through the accumulator, so it is clamped and regen/HUD stay
//       consistent. Optional 4th token = the amount while the spirit level
//       is yellow/red (valor mode, weapon+0x2370 >= 2). id 0 = every set-3
//       move (exact rows win over the wildcard).
//       With "option charge-takeover 1" native TIML hit-gains are zeroed
//       (see that option), so gainhit becomes the ONLY charging source:
//       charging at level 0 converts it to white (fresh timer), and a full
//       gauge while white promotes to yellow - the valor-mode entry
//       (user design 2026-10-24). Both mirror the vanilla level-change
//       recipe incl. the blade-aura transition FX (FUN_14211c5f0 branches).
//       Yellow->red stays vanilla (upgrade moves, FSM gauge-gated).
//   chargehit <set> <id> <bar> [valorBar] - THE valor charge (2026-10-24
//       round 2, user-corrected): connected hits push the level-drop TIMER
//       (weapon+0x2374, the user's "气刃值", bar units 0..1 - NOT seconds)
//       up by <bar>; crossing into full while WHITE promotes to yellow =
//       valor entry. Optional 4th token = the bar amount at yellow/red.
//       id 0 = every set-3 move. Every entry into white starts the bar near
//       empty (conversion below + the PollValorLifecycle drop edge), so the
//       promotion is only reachable by hitting. The 练气 gauge (0x2368)
//       stays 100% vanilla - do NOT pair this with gainhit charging.
//   chargehit-from <set> <prevId> <bar> [valorBar] - CONTEXT variant: the
//       amount applies when the hit's PREVIOUS action (controller+0xC8, the
//       game's own record) was <set>:<prevId> - "勇气纳刀后接的招式" (MHGU
//       valor sheathe boosts its follow-ups; the stance exits share ids with
//       normal chains, only prev tells them apart). Beats the per-move row;
//       the chargehit block only runs when some chargehit row (wildcard ok)
//       matches, keep that row present.
//   chargehit ... <bar> [valorBar] [critMult] - the optional 5th token
//       multiplies the amount on CRIT draws (2026-10-30, user format;
//       absent or 1.0 = no crit bonus).
//   chargehit-dmg <dmg> [cap] - damage-based scaling (2026-10-29): every
//       RESOLVED chargehit amount (ctx cell / from default / move row /
//       wildcard) is applied as amount*damage/<dmg>, capped at <cap> per hit
//       in bar units (default 0.30). chargestart stays flat. Delete the
//       line = flat amounts. Shift+F6 reloads.

std::vector<GaugeRule> g_gaugeRules;
// option regen-freeze 1: while a regen timer is active the gauge never
// decreases (negative deltas are zeroed) - "glow without dropping".
std::atomic_bool g_gaugeRegenFreeze{false};
// option charge-takeover 1: zero the native TIML hit-gains (the positive
// accumulator calls from FUN_142124b20's type-7 path) for the local player,
// so gainhit rows are the only charging source and the valor design holds
// exactly. Costs (negative) and the 见切 success refill (different caller)
// pass untouched. Explicit rewrite rows already won before this check.
std::atomic_bool g_gaugeChargeTakeover{false};
// Caller RVA of the accumulator call inside the TIML gain dispatcher
// (GAUGEEVT +0x2124f13, inside FUN_142124b20). addresses.txt override:
// gaugeTimlGain=0xRVA.
std::uintptr_t g_timlGainCallerRva = 0x2124F13;
// Damage-based valor charging (2026-10-29, gauge-rules.txt
// "chargehit-dmg <dmg> [cap]"): when <dmg> > 0 every RESOLVED chargehit
// amount (ctx cell / from default / move row / wildcard) is scaled by
// damage/<dmg>, capped per hit in bar units. 0 = off (flat amounts).
// chargestart stays flat - no damage exists at action start.
std::atomic<float> g_chargeHitDmgScale{0.0f};
std::atomic<float> g_chargeHitDmgCap{0.30f};

const GaugeRule* FindGaugeRule(GaugeRule::Type type, std::int32_t set,
                               std::int32_t id) {
    for (const auto& rule : g_gaugeRules) {
        if (rule.type == type && rule.set == set && rule.id == id) {
            return &rule;
        }
    }
    return nullptr;
}

#ifndef VALOR_PROBE
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
// Defined with the DealDamage hook below; set per applied hit there.
extern std::atomic<bool> g_lastAppliedTargetMonster;
float __fastcall GaugeAddHook(void* weapon, float value) {
    void* returnAddress = _ReturnAddress();
    const std::uintptr_t callerRva =
        returnAddress
            ? reinterpret_cast<std::uintptr_t>(returnAddress) -
                  g_moduleBase.load(std::memory_order_relaxed)
            : 0;
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
    bool rewritten = false;
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
                rewritten = true;
            }
        }
    }
    // charge-takeover: native TIML hit-gains (the positive accumulator calls
    // from FUN_142124b20's type-7 dispatcher path) are zeroed so the gainhit
    // rows define charging exactly. Consumption (negative) and the 见切
    // success refill (FUN_142123510, different caller) pass untouched, and
    // explicit rewrite rows already won above.
    if (own && !rewritten && value > 0.0f &&
        g_gaugeChargeTakeover.load(std::memory_order_relaxed) &&
        callerRva != 0 && callerRva == g_timlGainCallerRva) {
        static std::atomic_uint32_t takeoverLogs{0};
        if (takeoverLogs.load(std::memory_order_relaxed) < 40) {
            takeoverLogs.fetch_add(1, std::memory_order_relaxed);
            std::ostringstream m;
            m << "GAUGERULE takeover: suppressed native gain +" << value
              << " (act " << actSet << ':' << actId << ')';
            Log(m.str());
        }
        value = 0.0f;
    }
    // Monster-only native refill (2026-10-29): the same +0x2124f13 caller
    // also fires for terrain/gimmick applied hits and the user watches the
    // red bar climb on them. Zero it whenever the last applied hit (DealDamage
    // latch) was not a large monster. Costs (negative), the 见切 refill
    // (different caller) and explicit rewrites above pass untouched.
    if (own && !rewritten && value > 0.0f && !g_lastAppliedTargetMonster.load(
                                                     std::memory_order_relaxed) &&
        callerRva != 0 && callerRva == g_timlGainCallerRva) {
        static std::atomic_uint32_t terrainGainLogs{0};
        if (terrainGainLogs.load(std::memory_order_relaxed) < 40) {
            terrainGainLogs.fetch_add(1, std::memory_order_relaxed);
            std::ostringstream m;
            m << "GAUGERULE terrain: suppressed native gain +" << value
              << " (act " << actSet << ':' << actId << ')';
            Log(m.str());
        }
        value = 0.0f;
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
    g_chargeHitDmgScale.store(0.0f, std::memory_order_relaxed);  // reload opt-out
    g_chargeHitDmgCap.store(0.30f, std::memory_order_relaxed);
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
            float valor = 0.0f;
            if (ss >> valor) {
                rule.b = valor;
                rule.hasValor = true;
            }
        } else if (kind == "chargehit" || kind == "chargehit-from") {
            // chargehit: connected hits push the level-drop TIMER (0x2374,
            // bar units 0..1, NOT seconds) up; crossing into full while
            // white promotes to yellow. Optional 4th token = the amount at
            // yellow/red (valor mode). id 0 = every set-3 move. The id may
            // carry a context qualifier "82@98": that row only applies when
            // the previous action (controller+0xC4/+0xC8) was 3:98 - per-move
            // amounts for 勇气纳刀 follow-ups (MHGU: every stance exit has
            // its own gauge gain). Resolution order: ctx cell > chargehit-from
            // default > plain move row > wildcard.
            // chargehit-from: the context DEFAULT - applies to any move whose
            // previous action was <set>:<id> ("勇气纳刀后接的招式").
            rule.type = kind == "chargehit" ? GaugeRule::Type::ChargeHit
                                            : GaugeRule::Type::ChargeFrom;
            std::string idTok;
            if (!(ss >> rule.set >> idTok >> rule.a)) {
                continue;
            }
            rule.id = std::atoi(idTok.c_str());
            const auto at = idTok.find('@');
            if (at != std::string::npos) {
                rule.ctxPrev = std::atoi(idTok.c_str() + at + 1);
                rule.hasCtx = true;
            }
            float valor = 0.0f;
            if (ss >> valor) {
                rule.b = valor;
                rule.hasValor = true;
            }
            // Optional 5th token: crit multiplier (applied on crit draws).
            float critMult = 0.0f;
            if (ss >> critMult && critMult > 0.0f) {
                rule.critMult = critMult;
            }
        } else if (kind == "chargehit-dmg") {
            // Damage-based valor charging: <damagePerScale> [capBar]. When
            // set, every RESOLVED chargehit amount (any tier incl. ctx/from)
            // is scaled by damage/<damagePerScale>, capped in bar units.
            // chargestart stays flat. Delete the line = flat mode.
            float scale = 0.0f;
            if (ss >> scale) {
                float cap = 0.30f;
                if (ss >> cap) {
                    g_chargeHitDmgCap.store(cap, std::memory_order_relaxed);
                }
                g_chargeHitDmgScale.store(scale, std::memory_order_relaxed);
            }
        } else if (kind == "chargestart") {
            // Unconditional bar charge when the move BEGINS - for moves that
            // never connect hits but must charge (100 收刀回避: LMT312 has no
            // live attack windows, the hit-gated chargehit can never fire;
            // the move itself IS the "guard success" since its only entry is
            // the 98 stance guard). Same 0->white conversion and full-cross
            // promotion as chargehit.
            rule.type = GaugeRule::Type::ChargeStart;
            if (!(ss >> rule.set >> rule.id >> rule.a)) {
                continue;
            }
            float valor = 0.0f;
            if (ss >> valor) {
                rule.b = valor;
                rule.hasValor = true;
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
            if (ss >> key >> v) {
                if (key == "regen-freeze") {
                    g_gaugeRegenFreeze.store(v != 0.0f,
                                             std::memory_order_relaxed);
                } else if (key == "charge-takeover") {
                    g_gaugeChargeTakeover.store(v != 0.0f,
                                                std::memory_order_relaxed);
                }
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
    // chargestart: unconditional level-timer charge on BEGIN (2026-10-25).
    // For moves that never connect hits but must charge the valor bar - the
    // move starting IS the event (100 收刀回避: only entered from the 98
    // stance guard, so beginning = "the guard happened"). Same semantics as
    // the chargehit path: level 0 converts to white (bar 0.05), a charge
    // crossing into full while white promotes straight to red = valor entry
    // (2026-10-25 plan B).
    if (const auto* rule =
            FindGaugeRule(GaugeRule::Type::ChargeStart, actionSet, actionId)) {
        if (IsReadable(weapon + 0x2370, sizeof(float) * 2) &&
            IsReadable(weapon + 0x9F8, sizeof(std::uint32_t)) &&
            *reinterpret_cast<const volatile std::uint32_t*>(weapon + 0x9F8) ==
                g_modWeaponType.load(std::memory_order_relaxed)) {
            auto* player =
                static_cast<std::uint8_t*>(g_lockedPlayerController) - 0x61C8;
            const auto level =
                *reinterpret_cast<const volatile std::int32_t*>(weapon + 0x2370);
            if (level >= 0 && level <= 3) {
                if (level == 0) {
                    SetSpiritLevel(player, weapon, 1, "charge", 0.05f);
                }
                const bool valor = level >= 3;
                const float amount =
                    (rule->hasValor && valor) ? rule->b : rule->a;
                auto* timer =
                    reinterpret_cast<volatile float*>(weapon + 0x2374);
                const float prev = *timer;
                const float next = std::min(1.0f, prev + amount);
                *timer = next;
                if (next >= 0.999f && prev < 0.999f &&
                    *reinterpret_cast<const volatile std::int32_t*>(
                        weapon + 0x2370) == 1) {
                    SetSpiritLevel(player, weapon, 3, "charge-full");
                }
                std::ostringstream m;
                m << "GAUGERULE chargestart act=" << actionSet << ':'
                  << actionId << " +" << amount << (valor ? " (valor)" : "")
                  << " bar=" << prev << "->" << next;
                Log(m.str());
            }
        }
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

#endif // !VALOR_PROBE
