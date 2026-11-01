// Guard chip-damage experiments (2026-11-01 reverse engineering).
// FUN_1411c10c0 = tail-jump thunk to FUN_141922190(*(propMgr+8), 0x94, 0, 0):
// the FSM property getter with property id 0x94. Its single caller is the
// player victim-damage main FUN_141217e50 (return address RVA 0x1218C05):
//   threshold = FUN_1411c10c0(weaponCtrl);
//   if (threshold == 0 || threshold < damage || damage < 1.0f) -> HP -= dmg
//   else -> chip = 0, and the engine plays a dedicated zero-chip emitter and
//           queues NO guard reaction (damage 0 = no reaction upstream) -
//           confirmed in-game round 2: forcing the threshold kills the 35-37
//           knockback the user wants to keep.
// So the final design is NOT threshold forcing. ChipGuardRollback below lets
// the guard take its fully vanilla path (clang + 35-37 reaction + chip) and
// writes the HP box back to its pre-hit snapshot when the weapon FSM enters
// the guard-reaction family {35,36,37}. Vanilla hits outside the guard window
// transition via the body controller (1:339 family) and never enter 35-37,
// so the rollback edge is precise; re-hits inside the reaction keep being
// pressed back to the snapshot while it is fresh.

#include "prelude.h"
#include "all.h"

int (__fastcall *g_originalChipThreshold)(void* propMgr) = nullptr;

static std::atomic<int> g_chipForce{0};   // 0 = vanilla; >0 = max(orig, N)
static std::atomic<int> g_chipTrace{1};   // ungated-call logging
static std::atomic<int> g_rbEnabled{1};   // GuardRollback switch
static std::atomic<int> g_lethalVeto{1};  // drop lethal vitals writes in-guard
static std::atomic<double> g_chipWindow{0.20};  // guard frame window (seconds)
static std::atomic<int> g_chipGuardId{112};     // guard carrier action id (set 3)
static std::atomic<std::int64_t> g_chipLastLogMs{0};

void (__fastcall *g_originalVitSet)(void* box, float value, bool flag) = nullptr;

// --- probe-side vitals access (vitals.cpp is mod-build only) ----------------

namespace {

// Probe-side copies of the vitals accessors (vitals.cpp is mod-build only;
// f76c937 kept them out of the probe). Slot default matches vitals.cpp.
constexpr std::size_t kHpSlot = 0x7630;

std::uint8_t* ProbeVitalsBox(std::uint8_t* player, std::size_t slot) {
    if (!IsReadable(player + slot, sizeof(void*))) {
        return nullptr;
    }
    auto* box = *reinterpret_cast<std::uint8_t**>(player + slot);
    if (box && IsReadable(box + 0x60, 8)) {
        return box;
    }
    return nullptr;
}

struct RbState {
    float snapshotHp = -1.0f;
    std::int64_t snapshotMs = 0;
    bool snapshotValid = false;
};

float ReadHpCur(std::uint8_t* player) {
    if (auto* box = ProbeVitalsBox(player, kHpSlot)) {
        const float cur = *reinterpret_cast<const float*>(box + 0x64);
        const float max = *reinterpret_cast<const float*>(box + 0x60);
        if (cur >= 0.0f && max > 0.0f && cur <= max) {
            return cur;
        }
    }
    return -1.0f;
}

}  // namespace

// Guard gate shared by lethal-veto: local LS player + current set-3 action in
// the configured guard family + action frame inside the window (seconds).
static bool GuardGateActive(std::int32_t guardId, double windowSec) {
    if (!ModWeaponActive()) {
        return false;
    }
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    std::uint8_t* chainPlayer =
        static_cast<std::uint8_t*>(ResolveLocalPlayerViaManager());
    if (chainPlayer && controller &&
        static_cast<std::uint8_t*>(controller) - 0x61C8 != chainPlayer) {
        controller = nullptr;
    }
    if (!controller) {
        return false;
    }
    std::int32_t curSet = -1;
    std::int32_t curId = -1;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction.has_value()) {
            curSet = g_lastLongSwordAction->actionSet;
            curId = g_lastLongSwordAction->actionId;
        }
    }
    if (curSet != 3 || curId != guardId) {
        return false;
    }
    void* inst = ResolveActionInstance(controller, curSet, curId);
    if (!inst || !IsReadable(inst, 0x10)) {
        return false;
    }
    const float frame =
        *reinterpret_cast<volatile float*>(static_cast<std::uint8_t*>(inst) + 8);
    return frame >= 0.0f && frame <= windowSec;
}

// Lethal veto: the engine's zero-write path (flag==0 && value<1) inside the
// gang-1 guard window is dropped, so a killing chip never lands and the death
// check (reading the box) never fires - the 35-37 reaction still queues. The
// paired non-lethal chip is handled by GuardRollback below.
void __fastcall VitSetHook(void* box, float value, bool flag) {
    if (g_lethalVeto.load(std::memory_order_relaxed) && !flag && value < 1.0f &&
        GuardGateActive(g_chipGuardId.load(std::memory_order_relaxed),
                        g_chipWindow.load(std::memory_order_relaxed))) {
        void* controller = nullptr;
        {
            std::scoped_lock lock(g_actionMutex);
            controller = g_lockedPlayerController;
        }
        auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
        if (ProbeVitalsBox(player, kHpSlot) == box) {
            static std::int64_t lastLogMs = 0;
            const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count();
            if (nowMs - lastLogMs > 200) {
                lastLogMs = nowMs;
                std::ostringstream m;
                m << "CHIPVETO lethal write value=" << value
                  << " dropped (guard window)";
                Log(m.str());
            }
            return;
        }
    }
    g_originalVitSet(box, value, flag);
}

int __fastcall ChipThresholdHook(void* propMgr) {
    const int orig = g_originalChipThreshold(propMgr);
    const int force = g_chipForce.load(std::memory_order_relaxed);
    if (force > orig) {
        return force;
    }
    if (g_chipTrace.load(std::memory_order_relaxed)) {
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now().time_since_epoch())
                               .count();
        auto lastMs = g_chipLastLogMs.load(std::memory_order_relaxed);
        if (nowMs - lastMs > 500 &&
            g_chipLastLogMs.compare_exchange_strong(lastMs, nowMs)) {
            const std::uintptr_t callerRva =
                _ReturnAddress()
                    ? reinterpret_cast<std::uintptr_t>(_ReturnAddress()) -
                          g_moduleBase.load(std::memory_order_relaxed)
                    : 0;
            std::ostringstream m;
            m << "CHIP caller=+0x" << std::hex << callerRva << std::dec
              << " threshold=" << orig;
            if (g_lastLongSwordAction.has_value()) {
                m << " act=" << g_lastLongSwordAction->actionSet << ":"
                  << g_lastLongSwordAction->actionId;
            }
            Log(m.str());
        }
    }
    return orig;
}

void InstallChipGuardHook() {
#ifndef VALOR_PROBE
    if (g_hookAddr[HkVitSet] == 0) {
        Log("CHIP vitSet unavailable: target unresolved (addresses.txt vitSet=0xRVA)");
        return;
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkVitSet]),
                      reinterpret_cast<void*>(&VitSetHook),
                      reinterpret_cast<void**>(&g_originalVitSet)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkVitSet])) != MH_OK) {
        Log("CHIP vitSet hook failed");
        return;
    }
    Log("CHIP hooks installed (vitSet lethal veto; rollback polls)");
#else
    if (g_hookAddr[HkChipThreshold] == 0) {
        Log("CHIP hook unavailable: target unresolved (addresses.txt chipThreshold=0xRVA)");
        return;
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkChipThreshold]),
                      reinterpret_cast<void*>(&ChipThresholdHook),
                      reinterpret_cast<void**>(&g_originalChipThreshold)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkChipThreshold])) != MH_OK) {
        Log("CHIP hook failed: MH_CreateHook/MH_EnableHook");
        return;
    }
    Log("CHIP threshold hook installed (probe trace/force)");
    if (g_hookAddr[HkVitSet] == 0) {
        Log("CHIP vitSet unavailable: target unresolved");
    } else if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkVitSet]),
                             reinterpret_cast<void*>(&VitSetHook),
                             reinterpret_cast<void**>(&g_originalVitSet)) != MH_OK ||
               MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkVitSet])) != MH_OK) {
        Log("CHIP vitSet hook failed");
    } else {
        Log("CHIP vitSet hook installed (lethal veto in guard window)");
    }
    Log("CHIP hooks installed (rollback is the main path)");
#endif
}


// --- GuardRollback: HP snapshot restore on guard-reaction entry -------------

void PollGuardRollback() {
    static RbState rb;
    if (!g_rbEnabled.load(std::memory_order_relaxed)) {
        return;
    }
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    // Same staleness cross-check as PollValorLifecycle: the chain player is
    // the live entity across scene swaps, a stale action lock must not write.
    std::uint8_t* chainPlayer =
        static_cast<std::uint8_t*>(ResolveLocalPlayerViaManager());
    if (chainPlayer && controller &&
        static_cast<std::uint8_t*>(controller) - 0x61C8 != chainPlayer) {
        controller = nullptr;
    }
    if (!controller || !ModWeaponActive()) {
        rb.snapshotValid = false;
        return;
    }
    std::int32_t curSet = -1;
    std::int32_t curId = -1;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction.has_value()) {
            curSet = g_lastLongSwordAction->actionSet;
            curId = g_lastLongSwordAction->actionId;
        }
    }
    if (curSet != 3) {
        rb.snapshotValid = false;
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    const bool inReaction = curId == 35 || curId == 36 || curId == 37;
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();

    if (inReaction) {
        // Press HP back to the pre-hit snapshot while it is fresh (<800ms).
        // Freshness bounds the write to this guard exchange - a heal (pot)
        // taken DURING the reaction tail is older than the window and wins.
        const float hp = ReadHpCur(player);
        if (rb.snapshotValid && hp >= 0.0f && hp < rb.snapshotHp &&
            nowMs - rb.snapshotMs < 800) {
            if (auto* box = ProbeVitalsBox(player, kHpSlot)) {
                *reinterpret_cast<float*>(box + 0x64) = rb.snapshotHp;
                static std::int64_t lastLogMs = 0;
                if (nowMs - lastLogMs > 200) {
                    lastLogMs = nowMs;
                    std::ostringstream m;
                    m << "CHIPROLL act=3:" << curId << " hp " << hp << " -> "
                      << rb.snapshotHp;
                    Log(m.str());
                }
            }
        }
    } else {
        // Outside the reaction family: refresh the snapshot every frame, so
        // the moment a guard lands the snapshot is the pre-hit value.
        const float hp = ReadHpCur(player);
        if (hp > 0.0f) {
            rb.snapshotHp = hp;
            rb.snapshotMs = nowMs;
            rb.snapshotValid = true;
        }
    }
}

void LoadChipGuardConfig() {
    #ifdef VALOR_PROBE
    std::ifstream f("nativePC/plugins/ValorProbe.chipguard.txt");
#else
    std::ifstream f("nativePC/plugins/ValorGauge.chipguard.txt");
#endif
    if (!f) {
        return;
    }
    std::string line;
    int force = -1;
    int trace = -1;
    int rollback = -1;
    int lethalveto = -1;
    int guardId = -1;
    while (std::getline(f, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, eq);
        const int value = atoi(line.c_str() + eq + 1);
        if (key == "force") {
            force = value;
        } else if (key == "trace") {
            trace = value;
        } else if (key == "rollback") {
            rollback = value;
        } else if (key == "lethalveto") {
            lethalveto = value;
        } else if (key == "guardId") {
            guardId = value;
        }
    }
    if (force >= 0) {
        g_chipForce.store(force, std::memory_order_relaxed);
    }
    if (trace >= 0) {
        g_chipTrace.store(trace, std::memory_order_relaxed);
    }
    if (rollback >= 0) {
        g_rbEnabled.store(rollback, std::memory_order_relaxed);
    }
    if (lethalveto >= 0) {
        g_lethalVeto.store(lethalveto, std::memory_order_relaxed);
    }
    if (guardId > 0) {
        g_chipGuardId.store(guardId, std::memory_order_relaxed);
    }
    std::ostringstream m;
    m << "CHIP config: trace=" << g_chipTrace.load()
      << " force=" << g_chipForce.load()
      << " rollback=" << g_rbEnabled.load()
      << " lethalveto=" << g_lethalVeto.load()
      << " guardId=" << g_chipGuardId.load();
    Log(m.str());
}

#ifdef VALOR_PROBE
void PollChipGuardHotkey() {
    static bool wasDown = false;
    const bool down = (GetAsyncKeyState(VK_F7) & 0x8000) != 0 &&
                      (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    if (down && !wasDown) {
        LoadChipGuardConfig();
        Log("CHIP config reloaded (Shift+F7)");
    }
    wasDown = down;
}
#endif // VALOR_PROBE
