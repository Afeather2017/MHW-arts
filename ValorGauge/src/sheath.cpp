// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L1344-L1428, L1432-L1492, L1511-L1537, L1539-L1673, L1680-L1719, L4631-L4746, L5070-L5166, L8376-L8459. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

#ifndef VALOR_PROBE
// --- Sheathed-state draw rewrite (收刀拔刀刚1) -------------------------------
// Vanilla routes sheathed R2 to {3,11} (draw spirit blade I) purely in code +
// per-weapon static tables; the weapon FSM has no sheathed links to edit
// (CLAUDE.md 2026-09-25). So the rewrite happens on the request itself, in
// DoActionHook, before the action tracker sees it. Sheathed test: the tracked
// previous action is set-1 (sheathed locomotion family 1:0/1:4/1:8/1:5);
// drawn locomotion never emits requests. weapon+0xC71 was NOT usable (read 0
// while sheathed; SHEATHPROBE lines print it for the research backlog).
// Gate (user rule): spirit level >= reqLevel (3 = red) and gauge
// (weapon+0x2368 float) above the threshold, else the vanilla sheathed draw
// spirit blade plays.
// Config nativePC/plugins/ValorGauge.sheath-rewrite.txt:
//   "<toId> <holdVK> <reqLevel> <minGauge> <fromId> [fromId...]"
//   default "112 0 3 0.15 0 11"  (toId 0 = off, holdVK 0 = no key gate,
//   reqLevel 0 = no level gate, minGauge < 0 = no gauge gate,
//   nohitSec <= 0 = no injection)
// Defense (2026-10-11 user decision): the counter moved off 见切 86's i-frame
// recipe onto 刚见切 3:112 (LMT 345) whose TIML carries the native guard
// window (PMC mFlag 0x90080000 f0->f12) - angle-limited engine guard
// arbitration instead of all-angle i-frames. (2026-10-23: the DLL-side GUARD
// borrow and the sheath-counter conversion rules were REMOVED - the defense
// action and special-sheathe transitions are now fully FSM-native. The 刚1
// gauge scope below takes the 0.15 cost. nohitSec stays available for
// i-frame style targets but ships as 0.)
std::atomic<std::uint32_t> g_sheathRewriteToId{112};
std::atomic<std::uint32_t> g_sheathRewriteHoldVK{0};
std::atomic<std::uint32_t> g_sheathRewriteReqLevel{3};
std::atomic<float> g_sheathRewriteMinGauge{0.15f};
std::atomic<float> g_sheathRewriteNohitSec{0.2f};
// 11 = standing sheathed R2 draw; 12 = WALKING/RUNNING sheathed R2 draw
// variant (log-confirmed 2026-10-04: 1:4 -> 3:12 x187 vs 1:0 -> 3:11 x158).
// With 11 alone, R2 while moving silently played the vanilla walking draw.
// NOTE: the effective default lives in LoadSheathRewriteConfig - its local
// fromIds overwrites this global on every load (init + Shift+F6).
std::vector<std::uint32_t> g_sheathRewriteFromIds{11, 12};
std::mutex g_sheathRewriteMutex;
// 刚1 gauge accounting (2026-10-04, user-aligned semantics): the cost is
// 0.15 UNCONDITIONALLY - hit/parry/whiff irrelevant, the only condition is
// "the user used the move". The counter move (3:112, LMT 345) has native
// gauge handlers that move weapon+0x2368 through SEVERAL paths (GAUGEEVT-
// visible events AND direct writes like the whiff full-clear), so those
// writers are BYPASSED, not intercepted:
//   phase 1 (waiting): from the rewrite until the restore moment - the FIRST
//     of: the 叮 (bit15 rising) / the move reaching frame 56 (TIML) / a
//     different set-3 action becoming current (ended or canceled) / the 5s
//     deadline. Vanilla does whatever in the meantime, no guard.
//   settle: write gauge = clamp(snapshot - 0.15), expected = same.
//   phase 2 (guard tail, 1s): FUN_142122410 events feed `expected` (negative
//     only - chain-move costs stack; positive vanilla rewards stay
//     suppressed per the 0.15-net spec). Any drift between the real gauge
//     and `expected` = a direct write (the whiff clear) -> reverted.
// Net cost of a sheathed 刚1 press = 0.15, always.
std::atomic<std::int32_t> g_gang1Phase{0};  // 0 idle / 1 waiting / 2 guard tail
std::atomic<float> g_gang1Snapshot{0.0f};
std::atomic<float> g_gang1Expected{0.0f};
std::atomic<std::int64_t> g_gang1DeadlineMs{0};
// Event bit 15 (the vanilla 见切 success latch, player+0x8938 & 0x8000) at
// arm time - success = the bit RISING during the scope, not its stale value.
// Dormant for the 112 scope: bit15 is set by the vanilla action-86 machinery
// only, so the parry restore never fires and the cost lands unconditionally
// via frame56/left/deadline (user design: 0.15 net, always).
std::atomic_bool g_gang1Bit15AtArm{false};
constexpr float kGang1GaugeCost = 0.15f;
// Restore timing (user design, 2026-10-04): the cost is 0.15 UNCONDITIONALLY
// - hit/parry/whiff irrelevant; the only condition is "the user used the
// move". Mid-move vanilla writers are bypassed, not intercepted: snapshot x
// at the rewrite, then restore x-0.15 at the FIRST of (1) the 叮 (bit15
// rising), (2) the move reaching frame 56 (TIML frames, 60/s), (3) a
// different set-3 action becoming current (ended/canceled - the move came
// out, the cost is owed), (4) the 5s deadline. The 1s guard tail then
// reverts any straggler direct writes (the whiff clear).
constexpr float kGang1RestoreFrame = 56.0f;
// DRAWGANG1 (2026-10-22, user design): sheathed draw bootstrap. Forcing the
// gang move from sheathed never flips the carry-state machinery - the 030/047
// attach pair is table selection only (vanilla DRAWN moves like 104/105 carry
// CONSTANT pairs, so no channel value can express the flip). The native draw
// attack 3:11 is the only reliable flip: its action class owns the whole
// sheathed->drawn transition. Mode: let 3:11 play for N motion frames, then
// interrupt into the gang move from the per-frame poll. 1 frame already
// beats MHGU's own A+B+X timing window (user).
static std::atomic<float> g_drawGang1Frames{0.0f};   // <=0 = mode off
static std::atomic<float> g_drawGang1Nohit{0.0f};
static std::atomic<float> g_drawGang1FireAt{-1.0f};  // >=0: bootstrap armed
static std::atomic<std::uint32_t> g_drawGang1FromId{11};
static std::atomic<std::uint32_t> g_drawGang1Diag{0}; // pending-window tick log

void LoadSheathRewriteConfig() {
    int toId = 112;
    int holdVk = 0;
    int reqLevel = 3;
    float minGauge = 0.15f;
    float nohitSec = 0.2f;
    // THIS local default is the one that ships: it unconditionally overwrites
    // the g_sheathRewriteFromIds global below (config-file trailing fromIds
    // APPEND to it). 12 = the walking/running sheathed draw variant
    // (log-confirmed 2026-10-04: 1:4 -> 3:12 x187 vs 1:0 -> 3:11 x158) -
    // editing only the global default leaves 12 leaks invisible.
    std::vector<std::uint32_t> fromIds{11, 12};
    std::ifstream input("nativePC/plugins/ValorGauge.sheath-rewrite.txt");
    int value = 0;
    if (input >> value && value >= 0 && value <= 0xFFFF) {
        toId = value;
        if (input >> value && value >= 0 && value <= 0xFF) {
            holdVk = value;
        }
        if (input >> value && value >= 0 && value <= 3) {
            reqLevel = value;
        }
        if (!(input >> minGauge)) {
            minGauge = 0.15f;
        }
        if (!(input >> nohitSec) || nohitSec < -1.0f || nohitSec > 3.0f) {
            nohitSec = 0.2f;
        }
        while (input >> value && value >= 0 && value <= 0xFFFF) {
            fromIds.push_back(static_cast<std::uint32_t>(value));
        }
    }
    if (fromIds.empty()) {
        fromIds.push_back(11);
        fromIds.push_back(12);
    }
    {
        std::scoped_lock lock(g_sheathRewriteMutex);
        g_sheathRewriteFromIds = std::move(fromIds);
    }
    g_sheathRewriteToId.store(static_cast<std::uint32_t>(toId), std::memory_order_relaxed);
    g_sheathRewriteHoldVK.store(static_cast<std::uint32_t>(holdVk), std::memory_order_relaxed);
    g_sheathRewriteReqLevel.store(static_cast<std::uint32_t>(reqLevel), std::memory_order_relaxed);
    g_sheathRewriteMinGauge.store(minGauge, std::memory_order_relaxed);
    g_sheathRewriteNohitSec.store(nohitSec, std::memory_order_relaxed);
    std::ostringstream msg;
    msg << "Sheath rewrite: sheathed R2 requests 3:{";
    {
        std::scoped_lock lock(g_sheathRewriteMutex);
        for (std::size_t i = 0; i < g_sheathRewriteFromIds.size(); ++i) {
            if (i) {
                msg << ",";
            }
            msg << g_sheathRewriteFromIds[i];
        }
    }
    msg << "} -> 3:" << toId << " (toId 0 = off), holdVK=" << holdVk
        << ", reqLevel=" << reqLevel << ", minGauge=" << minGauge
        << ", nohit=" << nohitSec << "s";
    Log(msg.str());
}

// DRAWGANG1 config: nativePC/plugins/ValorGauge.draw-gang1.txt
//   <frames> [nohitSec]
// frames = TIML 60fps frames of the native draw attack (3:11) to let play
// before interrupting into the gang move; <=0 or missing file = mode off and
// the in-place 3:11 -> 3:toId rewrite runs instead. Shift+F6 reloads.
void LoadDrawGang1Config() {
    float frames = 0.0f;
    float nohit = 0.0f;
    std::ifstream input("nativePC/plugins/ValorGauge.draw-gang1.txt");
    std::string line;
    while (std::getline(input, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        std::istringstream ss(line);
        if (ss >> frames) {
            ss >> nohit;
        }
    }
    g_drawGang1Frames.store(frames, std::memory_order_relaxed);
    g_drawGang1Nohit.store(nohit, std::memory_order_relaxed);
    std::ostringstream m;
    m << "DrawGang1 bootstrap: " << (frames > 0.0f ? "ON" : "off")
      << " frames=" << frames << " nohit=" << nohit << "s";
    Log(m.str());
}

void ApplySheathRewrite(void* controller, ActionInfo* action) {
    const auto toId = g_sheathRewriteToId.load(std::memory_order_relaxed);
    if (!toId) {
        return;
    }
    if (!IsLocalPlayerActionController(controller)) {
        return;
    }
    const auto fromId = static_cast<std::uint32_t>(action->actionId);
    {
        std::scoped_lock lock(g_sheathRewriteMutex);
        if (std::find(g_sheathRewriteFromIds.begin(), g_sheathRewriteFromIds.end(), fromId) ==
            g_sheathRewriteFromIds.end()) {
            // Not a rewrite target. If this arrives while SHEATHED (set-1
            // predecessor) it silently plays vanilla - the exact leak class
            // that hid the walking variant 3:12 for weeks (non-matching ids
            // used to return with zero logging). Rate-limited visibility so
            // remaining variants (climb/swim draws, ...) surface in one run.
            std::scoped_lock actionLock(g_actionMutex);
            if (g_lastLongSwordAction && g_lastLongSwordAction->actionSet == 1) {
                static std::atomic_uint32_t seenLogs{0};
                if (seenLogs.fetch_add(1, std::memory_order_relaxed) < 30) {
                    std::ostringstream m;
                    m << "SHEATHSEEN: sheathed request 3:" << fromId
                      << " not in fromIds (last=1:"
                      << g_lastLongSwordAction->actionId << ") - vanilla plays";
                    Log(m.str());
                }
            }
            return;
        }
    }
    const auto holdVk = g_sheathRewriteHoldVK.load(std::memory_order_relaxed);
    if (holdVk && (GetAsyncKeyState(static_cast<int>(holdVk)) & 0x8000) == 0) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (!IsReadable(player + 0x76B0, sizeof(void*))) {
        return;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
    if (!weapon || !IsReadable(weapon, 0x2400)) {
        return;
    }
    if (*reinterpret_cast<const volatile std::uint32_t*>(weapon + 0x9F8) !=
        g_modWeaponType.load(std::memory_order_relaxed)) {
        return;
    }
    const auto c71 = static_cast<int>(*(weapon + 0xC71));
    const auto level = *reinterpret_cast<const volatile std::int32_t*>(weapon + 0x2370);
    const auto gauge = *reinterpret_cast<const volatile float*>(weapon + 0x2368);
    const auto reqLevel = g_sheathRewriteReqLevel.load(std::memory_order_relaxed);
    const auto minGauge = g_sheathRewriteMinGauge.load(std::memory_order_relaxed);
    // "Only once" per draw: DoAction also carries buffered presses, so a
    // double-tap during the sheathe delivers a second 3:11 while the tracker
    // still shows the sheathe (the tracker only updates at action begin).
    // The controller's CURRENT action says toId the instant the first
    // rewrite is accepted (DoAction phase-0 direct write) - that is the
    // exact once-per-draw signal.
    {
        auto* ctrlBytes = static_cast<std::uint8_t*>(controller);
        const std::int32_t curSet = *reinterpret_cast<volatile std::int32_t*>(ctrlBytes + 0xAC);
        const std::int32_t curId = *reinterpret_cast<volatile std::int32_t*>(ctrlBytes + 0xB0);
        if (curSet == 3 && static_cast<std::uint32_t>(curId) == toId) {
            static std::atomic_uint32_t onceLogs{0};
            if (onceLogs.fetch_add(1, std::memory_order_relaxed) < 8) {
                std::ostringstream m;
                m << "SHEATHREWRITE once: buffered 3:" << fromId
                  << " dropped (current=3:" << curId << ")";
                Log(m.str());
            }
            return;
        }
    }
    if (reqLevel && level < static_cast<std::int32_t>(reqLevel)) {
        static std::atomic_uint32_t rejectLogs{0};
        if (rejectLogs.fetch_add(1, std::memory_order_relaxed) < 12) {
            std::ostringstream m;
            m << "SHEATHREWRITE reject 3:" << fromId << " level=" << level
              << " (need >= " << reqLevel << ") gauge=" << gauge;
            Log(m.str());
        }
        return;
    }
    if (minGauge >= 0.0f && !(gauge > minGauge)) {
        static std::atomic_uint32_t rejectLogs{0};
        if (rejectLogs.fetch_add(1, std::memory_order_relaxed) < 12) {
            std::ostringstream m;
            m << "SHEATHREWRITE reject 3:" << fromId << " level=" << level
              << " gauge=" << gauge << " (need > " << minGauge << ")";
            Log(m.str());
        }
        return;
    }
    // DRAWGANG1: state-free (user 2026-10-23) - ANY 気刃斬り1-family request
    // (3:11/3:12, any carry state) + red level + gauge >= threshold becomes
    // the gang move; 気刃斬り1 stays reachable when the level/gauge gates fail.
    if (g_drawGang1Frames.load(std::memory_order_relaxed) > 0.0f) {
        // Bootstrap mode: no in-place rewrite - arm the per-frame poll to
        // interrupt the draw attack into the gang move after the configured
        // frames. One motion frame of the native draw attack completes the
        // sheathed->drawn carry-state flip (its action class owns it).
        const float frames = g_drawGang1Frames.load(std::memory_order_relaxed);
        g_drawGang1FireAt.store(frames, std::memory_order_release);
        g_drawGang1FromId.store(static_cast<std::uint32_t>(fromId),
                                std::memory_order_relaxed);
        g_drawGang1Diag.store(0, std::memory_order_relaxed);
        std::ostringstream m;
        m << "DRAWGANG1 arm: 3:" << fromId << " x" << frames << "f -> 3:"
          << toId << " (level=" << level << " gauge=" << gauge << ")";
        Log(m.str());
        return;
    }
    const auto nohitSec = g_sheathRewriteNohitSec.load(std::memory_order_relaxed);
    if (nohitSec > 0.0f) {
        // The FSM never left 待機(1000), so no vanilla defense window opens
        // for this move - inject the clang-trigger no-hit window instead.
        InjectNoHitLocked(nohitSec);
    }
    std::ostringstream m;
    m << "SHEATHREWRITE: 3:" << fromId << " -> 3:" << toId
      << " (level=" << level << " gauge=" << gauge << " nohit=" << nohitSec << "s)"
      // sheath-state snapshot for the drawn-state bug: +0xC71 latch (polarity
      // unproven), +0xA50/+0x23C0 = 030/047 motion sample destinations the
      // per-frame sheath decider also reads, +0x2D0 bits 5/6 = 納刀中/拔刀中.
      << " c71=" << static_cast<int>(*(weapon + 0xC71))
      << " a50=0x" << std::hex
      << *reinterpret_cast<volatile std::uint32_t*>(weapon + 0xA50) << std::dec
      << " a23c0=0x" << std::hex
      << *reinterpret_cast<volatile std::uint32_t*>(weapon + 0x23C0) << std::dec
      << " 2d0=0x" << std::hex
      << *reinterpret_cast<volatile std::uint32_t*>(weapon + 0x2D0) << std::dec;
    Log(m.str());
    action->actionId = static_cast<std::int32_t>(toId);
}

#endif // !VALOR_PROBE
// Arms the 刚1 gauge scope for ANY local-player 112 accept - the sheath
// rewrite target IS the 刚1 move (3:112, LMT 345, TIML guard frames) with
// the same unconditional 0.15 cost per user design.
// Deterministically pre-cost: DoAction accept happens before the updater's
// begin runs the start-cost handler. Skips while a scope is already waiting
// (phase 1); a settled/tail scope (phase 2) is re-armed by the next press.
// Hard LS gate: 86 collides across weapon move tables (the GS lesson).
#ifndef VALOR_PROBE
void ArmGang1GaugeScope(void* controller) {
    if (g_gang1Phase.load(std::memory_order_acquire) == 1) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (!IsReadable(player + 0x76B0, sizeof(void*))) {
        return;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
    if (!weapon || !IsReadable(weapon + 0x2368, sizeof(float)) ||
        !IsReadable(weapon + 0x9F8, sizeof(std::uint32_t)) ||
        *reinterpret_cast<const volatile std::uint32_t*>(weapon + 0x9F8) !=
            g_modWeaponType.load(std::memory_order_relaxed)) {
        return;
    }
    g_gang1Snapshot.store(
        *reinterpret_cast<const volatile float*>(weapon + 0x2368),
        std::memory_order_relaxed);
    g_gang1DeadlineMs.store(
        static_cast<std::int64_t>(GetTickCount64()) + 5000,
        std::memory_order_relaxed);
    {
        const bool bit15 =
            IsReadable(player + 0x8938, sizeof(std::uint32_t)) &&
            (*reinterpret_cast<const volatile std::uint32_t*>(
                 player + 0x8938) &
             0x8000) != 0;
        g_gang1Bit15AtArm.store(bit15, std::memory_order_relaxed);
    }
    g_gang1Phase.store(1, std::memory_order_release);
}
#endif // !VALOR_PROBE

#ifndef VALOR_PROBE
// 刚1 gauge accounting state machine (see the block comment at g_gang1Phase).
// Runs on the game thread (property-getter hook). Phase 1 waits for the
// request tracker to leave 112 (the rewritten request is BUFFERED while the
// previous move plays, so the controller current-action reads the old move -
// an immediate settle fired before the move's own cost in the first version).
// Phase 2 guards the settle against the vanilla whiff full-clear, which is a
// direct +0x2368 write that lands AFTER the settle and bypasses every hook.
void PollGang1GaugeScope() {
    const std::int32_t phase = g_gang1Phase.load(std::memory_order_acquire);
    if (phase == 0) {
        return;
    }
    const auto now = static_cast<std::int64_t>(GetTickCount64());
    if (phase == 1) {
        void* ctrl = g_lockedPlayerController;
        if (!ctrl) {
            return;
        }
        auto* bytes = static_cast<std::uint8_t*>(ctrl);
        const std::int32_t curSet =
            *reinterpret_cast<volatile std::int32_t*>(bytes + 0xAC);
        const std::int32_t curId =
            *reinterpret_cast<volatile std::int32_t*>(bytes + 0xB0);
        auto* player = bytes - 0x61C8;
        // Restore trigger 1: the 叮 (event bit 15 rising since arm).
        bool restore = false;
        const char* why = "";
        if (IsReadable(player + 0x8938, sizeof(std::uint32_t)) &&
            (*reinterpret_cast<const volatile std::uint32_t*>(
                 player + 0x8938) &
             0x8000) != 0 &&
            !g_gang1Bit15AtArm.load(std::memory_order_relaxed)) {
            restore = true;
            why = "parry(bit15)";
        }
        // Trigger 2: the move reached frame 56 (TIML frames, 60/s) - use the
        // 112 instance's own frame counter so a mid-move cancel freezes it.
        if (!restore && curSet == 3 && curId == 112) {
            void* inst = ResolveActionInstance(ctrl, 3, 112);
            if (inst) {
                const float frame = *reinterpret_cast<volatile float*>(
                    static_cast<std::uint8_t*>(inst) + 8);
                if (frame >= kGang1RestoreFrame / 60.0f) {
                    restore = true;
                    why = "frame56";
                }
            }
        }
        // Trigger 3: a DIFFERENT set-3 action is current - 112 ended or was
        // canceled. (During the initial buffer the controller still shows the
        // pre-rewrite move, set 1 - no fire.) The move came out, cost owed.
        if (!restore && curSet == 3 && curId != 112) {
            restore = true;
            why = "left112";
        }
        // Trigger 4: deadline backstop.
        if (!restore &&
            now > g_gang1DeadlineMs.load(std::memory_order_relaxed)) {
            restore = true;
            why = "deadline";
        }
        if (!restore) {
            return;
        }
        if (!IsReadable(player + 0x76B0, sizeof(void*))) {
            return;
        }
        auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
        if (!weapon || !IsWritable(weapon + 0x2368, sizeof(float))) {
            return;
        }
        const float snapshot = g_gang1Snapshot.load(std::memory_order_relaxed);
        const float target = std::clamp(snapshot - kGang1GaugeCost, 0.0f, 1.0f);
        auto* gauge = reinterpret_cast<volatile float*>(weapon + 0x2368);
        const float before = *gauge;
        *gauge = target;
        g_gang1Expected.store(target, std::memory_order_relaxed);
        g_gang1DeadlineMs.store(now + 1000, std::memory_order_relaxed);
        g_gang1Phase.store(2, std::memory_order_release);
        std::ostringstream m;
        m << "GAUGERULE gang1 settle[" << why << "]: gauge " << before << " -> "
          << target << " (snapshot " << snapshot << " - " << kGang1GaugeCost
          << ')';
        Log(m.str());
        return;
    }
    // Phase 2: guard tail - revert direct writes that bypass FUN_142122410.
    auto* player = static_cast<std::uint8_t*>(g_lockedPlayerController) -
                   0x61C8;
    if (!g_lockedPlayerController ||
        !IsReadable(player + 0x76B0, sizeof(void*))) {
        g_gang1Phase.store(0, std::memory_order_release);
        return;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
    if (!weapon || !IsWritable(weapon + 0x2368, sizeof(float))) {
        g_gang1Phase.store(0, std::memory_order_release);
        return;
    }
    const float expected = g_gang1Expected.load(std::memory_order_relaxed);
    auto* gauge = reinterpret_cast<volatile float*>(weapon + 0x2368);
    const float actual = *gauge;
    if (actual < expected - 0.01f || actual > expected + 0.01f) {
        *gauge = expected;
        static std::atomic_uint32_t guardLogs{0};
        if (guardLogs.fetch_add(1, std::memory_order_relaxed) < 10) {
            std::ostringstream m;
            m << "GAUGERULE gang1 guard: reverted direct write " << actual
              << " -> " << expected;
            Log(m.str());
        }
    }
    if (now > g_gang1DeadlineMs.load(std::memory_order_relaxed)) {
        g_gang1Phase.store(0, std::memory_order_release);
    }
}

// DRAWGANG1 per-frame consumer: while the armed bootstrap has the native
// draw attack (3:11) as the current action and its motion clock reached the
// configured frame, interrupt into the gang move (TryQueuedForceWrite
// pattern: trampoline call + manual tracker sync, because the forced call
// never passes through DoActionHook). Leaving 3:11 early (knockback, cancel)
// disarms without firing.
void PollDrawGang1(const void* entity) {
    if (g_drawGang1FireAt.load(std::memory_order_acquire) < 0.0f) {
        return;
    }
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller || !ModWeaponActive() || g_insideDoAction.load()) {
        return;
    }
    const auto* player = static_cast<const std::uint8_t*>(controller) - 0x61C8;
    if (entity != static_cast<const void*>(player)) {
        return;
    }
    auto* bytes = static_cast<std::uint8_t*>(controller);
    const std::int32_t phase = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xA8);
    const std::int32_t set = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xAC);
    const std::int32_t id = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xB0);
    // pending-window diagnostics: the first 12 poll ticks per arm show the
    // exact phase/action/motion-clock timeline of a failed bootstrap.
    float motNow = -1.0f;
    if (IsReadable(player + 0x468, sizeof(void*))) {
        void* anim = *reinterpret_cast<void* const*>(player + 0x468);
        if (anim && IsReadable(static_cast<std::uint8_t*>(anim) + 0x10C, 4)) {
            motNow = *reinterpret_cast<volatile float*>(static_cast<std::uint8_t*>(anim) + 0x10C);
        }
    }
    const std::uint32_t tick = g_drawGang1Diag.fetch_add(1, std::memory_order_relaxed);
    if (tick < 12) {
        std::ostringstream m;
        m << "DRAWGANG1 tick " << tick << ": phase=" << phase << " cur=3:" << id
          << " mot=" << motNow;
        Log(m.str());
    }
    // Disarm ONLY on a genuinely different current action (phase 2 = begun).
    // Two non-fire states are legitimate and must keep the pending alive:
    //   phase 0/1 with the draw id = accept->begin pipeline;
    //   current still showing the stale pre-draw action (e.g. the drawn idle
    //   3:0 keeps phase=2 on the controller while sheathed-standing) with the
    //   draw request QUEUED in the buffer (+0xBC/+0xC0) - sheathed-standing
    //   R2 goes through that buffer, so fire only once the draw attack really
    //   becomes current and its motion clock reaches the configured frame.
    const std::int32_t bufSet = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xBC);
    const std::int32_t bufId = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xC0);
    const std::int32_t fromId = static_cast<std::int32_t>(g_drawGang1FromId.load(std::memory_order_relaxed));
    const bool curMatch = (set == 3 && id == fromId);
    const bool bufMatch = (bufSet == 3 && bufId == fromId);
    if (!curMatch && !bufMatch) {
        if (phase == 2) {
            g_drawGang1FireAt.store(-1.0f, std::memory_order_release);
        }
        return;
    }
    if (!curMatch || phase != 2) {
        return;
    }
    float mot = -1.0f;
    if (IsReadable(player + 0x468, sizeof(void*))) {
        void* anim = *reinterpret_cast<void* const*>(player + 0x468);
        if (anim && IsReadable(static_cast<std::uint8_t*>(anim) + 0x10C, 4)) {
            mot = *reinterpret_cast<volatile float*>(static_cast<std::uint8_t*>(anim) + 0x10C);
        }
    }
    const float fireAt = g_drawGang1FireAt.load(std::memory_order_relaxed);
    if (mot < fireAt) {
        return;
    }
    g_drawGang1FireAt.store(-1.0f, std::memory_order_release);
    const auto toId = g_sheathRewriteToId.load(std::memory_order_relaxed);
    ActionInfo force{};
    force.actionSet = 3;
    force.actionId = static_cast<std::int32_t>(toId);
    const bool ok = g_originalDoAction(controller, &force) != 0;
    std::ostringstream m;
    m << "DRAWGANG1 fire: 3:11@" << mot << "f -> 3:" << toId
      << " result=" << (ok ? "accepted" : "rejected");
    Log(m.str());
    if (ok) {
        std::scoped_lock lock(g_actionMutex);
        ActionInfo forced{};
        forced.actionSet = 3;
        forced.actionId = static_cast<std::int32_t>(toId);
        g_lastLongSwordAction = forced;
        const float nohit = g_drawGang1Nohit.load(std::memory_order_relaxed);
        if (nohit > 0.0f) {
            InjectNoHitLocked(nohit);
        }
    }
}

// SHEATHWATCH (2026-10-18): edge logger for the special-sheathe sequence state
// on the weapon object. Reverse findings: FUN_1421237B0 forces the sheathed
// expectation (FUN_142150d10(td,0)) every frame while weapon+0x23C0 or
// weapon+0xA50 is nonzero; +0x23C0 is set by the special-sheathe stage
// machinery (stage handshake events land in +0x23B0/B4/B8; stage advance at
// 0x142144E04 requires newStage > +0x23B4) and its only clear site is the
// full weapon reset FUN_142124590 - so a custom valor chain that never
// completes the vanilla sequence keeps the flag up forever. +0xC71 latch
// polarity (09-25 live): 0 = sheathed, nonzero = drawn. Logs one line only
// when a watched field or the current action changes.
void PollSheathWatch() {
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller || !ModWeaponActive()) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (!IsReadable(player + 0x76B0, sizeof(void*))) {
        return;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
    if (!weapon || !IsReadable(weapon + 0x23C8, 8)) {
        return;
    }
    struct Snap {
        std::uint32_t actSet;
        std::uint32_t actId;
        std::uint8_t c71;
        std::uint8_t a50;
        std::uint8_t c0;
        std::uint8_t c3;
        std::uint8_t s68;
        std::uint8_t ab8;
        std::uint32_t b4;
        bool operator==(const Snap& o) const {
            return actSet == o.actSet && actId == o.actId && c71 == o.c71 &&
                   a50 == o.a50 && c0 == o.c0 && c3 == o.c3 && b4 == o.b4 &&
                   s68 == o.s68 && ab8 == o.ab8;
        }
    };
    Snap snap{};
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction) {
            snap.actSet = g_lastLongSwordAction->actionSet;
            snap.actId = g_lastLongSwordAction->actionId;
        } else {
            snap.actSet = 0xFFFFFFFFu;
            snap.actId = 0xFFFFFFFFu;
        }
    }
    snap.c71 = weapon[0xC71];
    snap.a50 = weapon[0xA50];
    snap.c0 = weapon[0x23C0];
    snap.c3 = weapon[0x23C3];
    snap.s68 = weapon[0x2368];
    snap.ab8 = weapon[0xAB8];
    snap.b4 = *reinterpret_cast<const volatile std::uint32_t*>(weapon + 0x23B4);

    static Snap last{};
    static bool have = false;
    if (have && snap == last) {
        return;
    }
    last = snap;
    have = true;
    std::ostringstream m;
    if (snap.actSet == 0xFFFFFFFFu) {
        m << "SHEATHWATCH act=-";
    } else {
        m << "SHEATHWATCH act=" << snap.actSet << ':' << snap.actId;
    }
    m << " c71=" << static_cast<int>(snap.c71)
      << " a50=" << static_cast<int>(snap.a50)
      << " 23c0=" << static_cast<int>(snap.c0)
      << " 23b4=" << snap.b4
      << " 23c3=" << static_cast<int>(snap.c3)
      << " 2368=" << static_cast<int>(snap.s68)
      << " ab8=" << static_cast<int>(snap.ab8);
    Log(m.str());
}

#endif // !VALOR_PROBE
