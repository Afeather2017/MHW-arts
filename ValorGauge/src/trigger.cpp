// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L298-L301, L303-L309, L367-L367, L369-L379, L381-L381, L2028-L2028, L3517-L3602, L4299-L4365, L6314-L6344, L6531-L6711. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

// Force-action: F6 (or the clang detector) arms a one-shot flag; the property
// getter hook consumes it ON the game thread by calling DoAction directly -
// the same entry the FSM's own link execution uses. No game-memory writes.
std::atomic_bool g_forceArmed{false};

std::uint32_t g_forceSet = 3;
std::uint32_t g_forceId = 104;
// force-action.txt 的配置默认值(刚1 目标)。g_forceSet/g_forceId 是"待发目标",
// 会被 sheath 规则/吞单武装覆盖; 弹反链武装前从这里恢复, 两者分离后互不污染。
std::atomic_uint32_t g_forceCfgSet{3};
std::atomic_uint32_t g_forceCfgId{104};
float g_forceInvulnSec = 0.5f;

double g_counterWindowSec = 0.6;

// The mined clang latch: player+0xB228 sets when the deflect presentation
// starts (hit negated during a special evasion) and clears when it ends.
// Silent on whiffs - log-verified 0 changes across 3 whiff runs vs set+reset
// on every clang.
std::uint32_t g_clangLatchPrev = 0;
bool g_counterAutoForce = true;
bool g_negBitPrev = false;
float g_flashPrev = 0.0f;
bool g_fsuccBitPrev = false;
std::chrono::steady_clock::time_point g_counterWindowUntil{};
bool g_counterCtrlWasDown = false;

bool g_clangLatch = false; // one force per 见切 action

std::atomic_uint32_t g_vseekLogCount{0};

#ifndef VALOR_PROBE
// --- 見切り回避成功後の攻撃ヒット condition evaluator hook ---------------------
void OpenCounterWindow(const char* reason);
// FUN_14118bda0 resolves and evaluates FSM condition-tree VariableNodes; the
// 見切り回避成功後の攻撃ヒット node carries the property-name pointer
// 0x1430BA8F0 in its VariableInfo. Scan the node for that pointer: matching
// calls evaluate THE vanilla success latch, and the return value is its
// current state. A nonzero return = the vanilla 刚1 branch condition passed.
constexpr std::uintptr_t kForesightSuccessName = 0x1430BA8F0;
using CondEvalFn = long long(__fastcall*)(unsigned int, long long, long long);
CondEvalFn g_originalCondEval = nullptr;
std::atomic_uint32_t g_condEvalLogCount{0};

long long __fastcall CondEvalHook(unsigned int a1, long long a2, long long a3) {
    // Empirical layout probe: find pointers into the property-name string
    // table (0x1430b0000..0x1430c0000) inside a2/a3 and log where they sit.
    if (g_condEvalLogCount.load() < 24 && ActionIsForesightSlash()) {
        auto scanNames = [&](long long base, const char* tag, std::ostringstream& m) {
            if (!base || !IsReadable(reinterpret_cast<const void*>(base), 0x100)) {
                return;
            }
            const auto* words = reinterpret_cast<const void* const*>(base);
            for (int off = 0; off < 0x100; off += (int)sizeof(void*)) {
                const auto v = reinterpret_cast<std::uintptr_t>(words[off / sizeof(void*)]);
                if (v >= 0x1430b0000 && v < 0x1430c0000) {
                    m << " " << tag << "+0x" << std::hex << std::uppercase << off << "=0x" << v;
                }
            }
        };
        std::ostringstream m;
        m << "CONDSCAN" << std::hex << std::uppercase << " a2=0x" << a2 << " a3=0x" << a3;
        scanNames(a2, "a2", m);
        scanNames(a3, "a3", m);
        Log(m.str());
        ++g_condEvalLogCount;
    }
    bool isForesightSuccess = false;
    if (a3 && IsReadable(reinterpret_cast<const void*>(a3), 0x100)) {
        const auto* node = reinterpret_cast<const void* const*>(a3);
        for (int off = 0; off < 0x100; off += (int)sizeof(void*)) {
            if (node[off / sizeof(void*)] == reinterpret_cast<const void*>(kForesightSuccessName)) {
                isForesightSuccess = true;
                break;
            }
        }
    }
    if (a2 && IsReadable(reinterpret_cast<const void*>(a2), 0x100)) {
        const auto* node = reinterpret_cast<const void* const*>(a2);
        for (int off = 0; off < 0x100; off += (int)sizeof(void*)) {
            if (node[off / sizeof(void*)] == reinterpret_cast<const void*>(kForesightSuccessName)) {
                isForesightSuccess = true;
                break;
            }
        }
    }
    const long long result = g_originalCondEval(a1, a2, a3);
    if (isForesightSuccess) {
        std::ostringstream m;
        m << "COND631 evaluated -> " << std::dec << result;
        Log(m.str());
    }
    if (isForesightSuccess && result != 0 && ModWeaponActive() &&
        g_counterAutoForce && !g_clangLatch) {
        // The vanilla success+hit latch just evaluated true: this is the exact
        // 刚1 trigger moment. Open the counter window and arm the force.
        if (!g_clangLatch) {
            g_clangLatch = true;
        }
        OpenCounterWindow("vanilla success condition evaluated TRUE");
        if (!g_forceArmed.exchange(true)) {
            Log("COUNTER INPUT not needed: vanilla condition passed - force armed");
        }
    }
    return result;
}

bool InstallCondEvalHook() {
    void* target = reinterpret_cast<void*>(g_hookAddr[HkCondEval]);
    if (MH_CreateHook(target, reinterpret_cast<void*>(&CondEvalHook),
                      reinterpret_cast<void**>(&g_originalCondEval)) != MH_OK ||
        MH_EnableHook(target) != MH_OK) {
        Log("Condition evaluator hook failed");
        return false;
    }
    Log("Condition evaluator hook installed");
    return true;
}

// Game-thread consumer of an armed force request: calls DoAction directly -
// the same entry the FSM's own link execution uses, so a mid-action interrupt
// is architecturally identical to what the game itself does. Runs inside the
// property-getter hook (game update thread).
void TryQueuedForceWrite(const void* entity) {
    if (!g_forceArmed.load(std::memory_order_acquire) || g_insideDoAction.load()) {
        return;
    }
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
        return;
    }
    if (!ModWeaponActive()) {
        return;
    }
    const void* player = static_cast<const std::uint8_t*>(controller) - 0x61C8;
    if (entity != player) {
        return;
    }
    g_forceArmed.store(false, std::memory_order_release);

    std::optional<ActionInfo> current;
    {
        std::scoped_lock lock(g_actionMutex);
        current = g_lastLongSwordAction;
    }
    ActionInfo force{};
    force.actionSet = static_cast<std::int32_t>(g_forceSet);
    force.actionId = static_cast<std::int32_t>(g_forceId);
    std::ostringstream m;
    m << "FORCE: DoAction(3:" << g_forceId << ") mid-action";
    if (current) {
        m << " (interrupting 3:" << current->actionId << ')';
    }
    const bool ok = g_originalDoAction(controller, &force) != 0;
    m << " result=" << (ok ? "accepted" : "rejected");
    Log(m.str());
    if (ok) {
        ArmFrameSeekDiagnostics();
        // The forced call goes through the trampoline, so the DoActionHook
        // never sees it: without this sync the tracker keeps reporting 见切
        // and every negation watcher stays armed throughout the counter move
        // (observed 2026-09-30: one clang forced 刚1 twice ~0.1s apart, the
        // second one re-triggered by the new motion's own event bit).
        {
            std::scoped_lock lock(g_actionMutex);
            ActionInfo forced{};
            forced.actionSet = static_cast<std::int32_t>(g_forceSet);
            forced.actionId = static_cast<std::int32_t>(g_forceId);
            g_lastLongSwordAction = forced;
            // One force per 见切 action: every trigger source is blocked until
            // the next 86/101/102 entry re-arms the latch (DoActionHook reset).
            g_clangLatch = true;
        }
        // Consume the counter window: a spirit press after the fire must not
        // arm a second force from the same clang.
        g_counterWindowUntil = {};
        std::ostringstream changed;
        changed << "Long Sword action changed (forced): set=" << g_forceSet
                << " id=" << g_forceId;
        Log(changed.str());
    }
}

void OpenCounterWindow(const char* reason) {
    g_counterWindowUntil = std::chrono::steady_clock::now() +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(g_counterWindowSec));
    // Pool pointer derived straight from the player (mgr = *(player+0x7D20),
    // pool = mgr+0x28) - the mod build installs no pool-query hook.
    std::uintptr_t pool = 0;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lockedPlayerController) {
            auto* player = static_cast<std::uint8_t*>(g_lockedPlayerController) - 0x61C8;
            if (IsReadable(player + 0x7D20, sizeof(void*))) {
                auto* mgr = *reinterpret_cast<std::uint8_t**>(player + 0x7D20);
                if (mgr && IsReadable(mgr + 0x28, sizeof(void*))) {
                    pool = reinterpret_cast<std::uintptr_t>(
                        *reinterpret_cast<void**>(mgr + 0x28));
                }
            }
        }
    }
    if (pool != 0 && g_forceInvulnSec > 0.0f) {
        auto* window = reinterpret_cast<float*>(pool + 0xE4);
        if (IsWritable(window, sizeof(float))) {
            *window = *window > g_forceInvulnSec ? *window : g_forceInvulnSec;
        }
    }
    std::ostringstream m;
    m << "COUNTER WINDOW OPEN (" << reason << ") nohit=" << g_forceInvulnSec
      << "s - press the spirit key to counter";
    Log(m.str());
}

// --- Mod per-frame trigger poll (gauge/counter build only) -------------------
// Three independent clang triggers, any of which opens the counter window,
// injects the no-hit coverage and (auto-force on) queues the 刚1 interrupt:
//   1. iai negation     - event bit 250 (player+0x8954 bit 26) during 101/102
//   2. foresight latch  - event bit 15 (player+0x8938) rising during 3:86
//   3. deflect flash    - weapon+0x23A0 crossing 0.5 upward during 3:86
// Everything is gated by ModWeaponActive(): other weapons never trigger.
void PollModTriggers() {
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
        return;
    }
    if (!ModWeaponActive()) {
        // Still track the latch so re-drawing the Long Sword does not see a
        // stale rising edge.
        g_fsuccBitPrev = false;
        g_negBitPrev = false;
        g_flashPrev = 0.0f;
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    auto* mgrSlot = player + 0x7D20;
    std::uint8_t* mgr = nullptr;
    if (IsReadable(mgrSlot, sizeof(void*))) {
        mgr = *reinterpret_cast<std::uint8_t**>(mgrSlot);
    }
    std::uint32_t standby = 0;
    float nohit = 0.0f;
    if (mgr && IsReadable(mgr, 0x7D0)) {
        standby = *reinterpret_cast<const std::uint32_t*>(mgr + 0x7C0);
    }
    if (mgr && IsReadable(mgr + 0x28 + 0xE4, sizeof(float))) {
        nohit = *reinterpret_cast<const float*>(mgr + 0x28 + 0xE4);
    }

    // Semantic sanity probes: if a game update shifts the struct layout these
    // invariants break first - log once and point at the offending offset
    // (recovery procedure in Reverse/ADDRESS_RECOVERY.md).
    static bool warnedSpirit = false;
    static bool warnedStandby = false;
    static bool warnedNohit = false;
    if (mgr && IsReadable(mgr, 0x7D0)) {
        if (!warnedStandby && standby > 1) {
            warnedStandby = true;
            Log("MEMORYMAP SANITY: mgr+0x7C0 standby > 1 - offset may have shifted");
        }
    }
    if (!warnedNohit && IsReadable(mgr + 0x28 + 0xE4, sizeof(float)) && nohit < -1.0f) {
        warnedNohit = true;
        Log("MEMORYMAP SANITY: pool+0xE4 no-hit < -1 - offset may have shifted");
    }
    if (!warnedSpirit) {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction && IsReadable(player + 0x76B0, sizeof(void*))) {
            auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
            if (weapon && IsReadable(weapon + 0x2370, sizeof(std::int32_t))) {
                const auto spirit =
                    *reinterpret_cast<const std::int32_t*>(weapon + 0x2370);
                if (spirit < 0 || spirit > 3) {
                    warnedSpirit = true;
                    Log("MEMORYMAP SANITY: weapon+0x2370 spirit level outside "
                        "[0,3] - offset may have shifted");
                }
            }
        }
    }

    bool inEvasionNow = false;
    {
        std::scoped_lock lock(g_actionMutex);
        inEvasionNow = g_lastLongSwordAction &&
                       g_lastLongSwordAction->actionSet == 3 &&
                       (g_lastLongSwordAction->actionId == 86 ||
                        g_lastLongSwordAction->actionId == 101 ||
                        g_lastLongSwordAction->actionId == 102);
    }

    // 1) IAI negation bit 250: player+0x8954 bit 26 (mined: clang dword
    //    0x1C000000 vs whiff 0x18000000/0x18400000; self-clears).
    if (inEvasionNow) {
        auto* negBitAddr = reinterpret_cast<std::uint32_t*>(player + 0x8954);
        if (IsReadable(negBitAddr, sizeof(std::uint32_t))) {
            const bool negBit = (*negBitAddr & 0x04000000u) != 0;
            if (negBit && !g_negBitPrev && !g_clangLatch) {
                g_clangLatch = true;
                OpenCounterWindow("negation bit 250 set (player+0x8954)");
                g_forceSet = g_forceCfgSet.load(std::memory_order_relaxed);
                g_forceId = g_forceCfgId.load(std::memory_order_relaxed);
                if (g_counterAutoForce && !g_forceArmed.exchange(true)) {
                    Log("CLANG: negation bit set - auto-force armed");
                }
            }
            g_negBitPrev = negBit;
        }
    } else {
        g_negBitPrev = false;
    }

    // 2) Deflect flash: weapon+0x23A0 crosses 0.5 upward during 3:86
    //    (mined 4/4 clang vs 0/5 whiff).
    if (ActionIsForesightSlash()) {
        auto* weaponSlot = player + 0x76B0;
        if (IsReadable(weaponSlot, sizeof(void*))) {
            auto* weapon = *reinterpret_cast<std::uint8_t**>(weaponSlot);
            if (weapon && IsReadable(weapon + 0x23A0, sizeof(float))) {
                const float flash =
                    *reinterpret_cast<const float*>(weapon + 0x23A0);
                if (flash > 0.5f && g_flashPrev <= 0.5f && !g_clangLatch) {
                    g_clangLatch = true;
                    OpenCounterWindow("deflect flash (weapon+0x23A0)");
                    g_forceSet = g_forceCfgSet.load(std::memory_order_relaxed);
                    g_forceId = g_forceCfgId.load(std::memory_order_relaxed);
                    if (g_counterAutoForce && !g_forceArmed.exchange(true)) {
                        Log("CLANG: deflect flash detected - auto-force armed");
                    }
                }
                g_flashPrev = flash;
            }
        }
    } else {
        g_flashPrev = 0.0f;
    }

    // 3) Vanilla foresight success latch: event bit 15 (player+0x8938) -
    //    the input remapper FUN_14118C320 picks follow-up 763 vs 764 with it.
    {
        std::uint32_t ev0 = 0;
        if (IsReadable(player + 0x8938, sizeof(std::uint32_t))) {
            ev0 = *reinterpret_cast<const std::uint32_t*>(player + 0x8938);
        }
        std::uint8_t m660 = 0, m661 = 0, m662 = 0;
        if (mgr && IsReadable(mgr + 0x660, 3)) {
            m660 = mgr[0x660];
            m661 = mgr[0x661];
            m662 = mgr[0x662];
        }
        const bool bit15 = (ev0 & 0x8000u) != 0;
        const bool in86 = ActionIsForesightSlash();
        static std::uint8_t pm660 = 0, pm661 = 0, pm662 = 0;
        static std::uint32_t pev0 = 0;
        if (bit15 != g_fsuccBitPrev || m660 != pm660 || m661 != pm661 ||
            m662 != pm662 || (in86 && ev0 != pev0)) {
            if (g_vseekLogCount.load() < 240) {
                ++g_vseekLogCount;
                std::ostringstream m;
                m << "VSEEK bit15=" << bit15 << std::hex << std::uppercase
                  << " ev0=0x" << ev0 << std::dec
                  << " m660=" << static_cast<int>(m660)
                  << " m661=" << static_cast<int>(m661)
                  << " m662=" << static_cast<int>(m662);
                {
                    std::scoped_lock lock(g_actionMutex);
                    if (g_lastLongSwordAction) {
                        m << " act=" << g_lastLongSwordAction->actionSet << ':'
                          << g_lastLongSwordAction->actionId;
                    }
                }
                Log(m.str());
            }
            pm660 = m660;
            pm661 = m661;
            pm662 = m662;
            pev0 = ev0;
        }
        if (in86 && bit15 && !g_fsuccBitPrev && !g_clangLatch) {
            g_clangLatch = true;
            OpenCounterWindow("foresight success bit15 (player+0x8938)");
            g_forceSet = g_forceCfgSet.load(std::memory_order_relaxed);
            g_forceId = g_forceCfgId.load(std::memory_order_relaxed);
            if (g_counterAutoForce && !g_forceArmed.exchange(true)) {
                Log("CLANG: vanilla foresight success latch - auto-force armed");
            }
        }
        g_fsuccBitPrev = bit15;
    }
}


#endif // !VALOR_PROBE
