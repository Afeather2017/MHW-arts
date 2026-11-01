// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L5652-L5832, L5834-L6144. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

#ifndef VALOR_PROBE
// --- Vitals slots ------------------------------------------------------------
// HP/stamina box pointer slots on the player object. Config
// nativePC/plugins/ValorGauge.vitals.txt = "hp <slotHex>" / "stam <slotHex>"
// (defaults 7630/7670). Readers: stamina control, hp-lock, HPWATCH.
//
// USER-VERIFIED 2026-10-05 (HPWATCH + VIT trace): the local player's HP box
// IS *(player+0x7630): +0x60=max, +0x64=cur; RecoverableHealth at box+0x2DD4;
// pending-heal array at *(box+0x30)+0xEBB0 (4 entries x0x40: Heal@0x10,
// MaxHeal@0x18, Stage@0x2C); Stamina cur/max at box+0x12C/0x130. Damage
// writes route through the low-level setter (FUN_141216940) from unknown
// callers (seen: RVA +0x1218E18), NOT through FUN_141f518e0.
std::atomic<std::size_t> g_vitalsHpSlot{0x7630};
std::atomic<std::size_t> g_vitalsStamSlot{0x7670};

std::uint8_t* VitalsBox(std::uint8_t* player, std::size_t slot) {
    if (!IsReadable(player + slot, sizeof(void*))) {
        return nullptr;
    }
    auto* box = *reinterpret_cast<std::uint8_t**>(player + slot);
    if (box && IsReadable(box + 0x60, 8)) {
        return box;
    }
    return nullptr;
}

std::uint8_t* WeaponOfPlayer(std::uint8_t* player) {
    if (!IsReadable(player + 0x76B0, sizeof(void*))) {
        return nullptr;
    }
    return *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
}

void LoadVitalsConfig() {
    std::ifstream input("nativePC/plugins/ValorGauge.vitals.txt");
    std::string hpTok, stamTok;
    std::string line;
    bool any = false;
    while (std::getline(input, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        std::istringstream ss(line);
        std::string key;
        if (!(ss >> key)) {
            continue;
        }
        std::string val;
        if (!(ss >> val)) {
            continue;
        }
        const auto parsed = static_cast<std::size_t>(
            std::strtoull(val.c_str(), nullptr, 0));
        if (key == "hp") {
            g_vitalsHpSlot = parsed;
            any = true;
        } else if (key == "stam") {
            g_vitalsStamSlot = parsed;
            any = true;
        }
    }
    std::ostringstream m;
    m << "Vitals slots: hp=+0x" << std::hex << g_vitalsHpSlot.load()
      << " stam=+0x" << g_vitalsStamSlot.load() << std::dec
      << (any ? " (custom)" : " (defaults, HP box verified 2026-10-05)");
    Log(m.str());
}

// --- HP lock (address verification tool) -------------------------------------
// ValorGauge.hp-lock.txt = "<value>"; >0 forces BOTH box fields (max+cur) to
// value every frame. Set 200: if the HUD shows/keeps 200, the vitals box
// address is right. 0 or missing file = off.
std::atomic<float> g_hpLockValue{0.0f};

void LoadHpLockConfig() {
    float v = 0.0f;
    std::ifstream input("nativePC/plugins/ValorGauge.hp-lock.txt");
    if (input >> v) {
        g_hpLockValue = (v > 0.0f && v <= 500.0f) ? v : 0.0f;
    } else {
        g_hpLockValue = 0.0f;
    }
    std::ostringstream m;
    m << "HP lock: " << g_hpLockValue.load()
      << (g_hpLockValue.load() > 0.0f ? " (forced every frame - TEST TOOL)"
                                      : " (off)");
    Log(m.str());
}

void PollHpLock() {
    const float v = g_hpLockValue.load(std::memory_order_relaxed);
    if (v <= 0.0f) {
        return;
    }
    void* controller = g_lockedPlayerController;
    if (!controller) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (auto* box = VitalsBox(player, g_vitalsHpSlot.load())) {
        *reinterpret_cast<float*>(box + 0x60) = v;  // max
        *reinterpret_cast<float*>(box + 0x64) = v;  // cur
    }
}

// --- HPWATCH: one-second candidate dump for the real HP field ---------------
// Conflicting static reads (HunterPie says the *(player+0x7630) container IS
// the HUD structure: MaxHealth@0x60/Health@0x64/Stamina@0x12C/Recoverable@
// 0x2DD4/healing array *(c+0x30)+0xEBB0; the user's hp-lock test on the same
// box moved SHARPNESS). Ground truth = which logged number tracks the HUD
// when a hit lands. Cap-limited so one session answers it.
std::atomic<std::uint32_t> g_hpWatchCount{0};
std::uint32_t g_hpWatchFrame = 0;

void PollHpWatch() {
    if (++g_hpWatchFrame % 90 != 0) {
        return;
    }
    if (g_hpWatchCount.load(std::memory_order_relaxed) >= 400) {
        return;
    }
    void* controller = g_lockedPlayerController;
    if (!controller) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    std::ostringstream m;
    m << "HPWATCH";
    // HunterPie HUD structure = the *(player+0x7630) container itself.
    if (IsReadable(player + 0x7630, sizeof(void*))) {
        auto* c = *reinterpret_cast<std::uint8_t**>(player + 0x7630);
        if (c) {
            if (IsReadable(c + 0x64, sizeof(float))) {
                m << " hpBox=["
                  << *reinterpret_cast<const float*>(c + 0x60) << ","
                  << *reinterpret_cast<const float*>(c + 0x64) << "]";
            }
            if (IsReadable(c + 0x130, sizeof(float))) {
                m << " hudStam=["
                  << *reinterpret_cast<const float*>(c + 0x12C) << ","
                  << *reinterpret_cast<const float*>(c + 0x130) << "]";
            }
            if (IsReadable(c + 0x2DD4, sizeof(float))) {
                m << " rec="
                  << *reinterpret_cast<const float*>(c + 0x2DD4);
            }
            if (IsReadable(c + 0x30, sizeof(void*))) {
                auto* arr = *reinterpret_cast<std::uint8_t**>(c + 0x30);
                if (arr && IsReadable(arr + 0xEBB0, 4 * 0x40)) {
                    m << " heal=";
                    for (int i = 0; i < 4; ++i) {
                        auto* e = arr + 0xEBB0 + i * 0x40;
                        m << *reinterpret_cast<const float*>(e + 0x10) << "/"
                          << *reinterpret_cast<const float*>(e + 0x18) << "s"
                          << *reinterpret_cast<const std::int32_t*>(e + 0x2C)
                          << (i < 3 ? "+" : "");
                    }
                }
            }
        } else {
            m << " hpBox=<null>";
        }
    }
    // Legacy candidates for comparison.
    if (auto* stamBox = VitalsBox(player, 0x7670)) {
        m << " box7670=[" << *reinterpret_cast<const float*>(stamBox + 0x60)
          << "," << *reinterpret_cast<const float*>(stamBox + 0x64) << "]";
    }
    if (auto* weapon = WeaponOfPlayer(player)) {
        if (IsReadable(weapon + 0x2100, 4)) {
            m << " sharp=" << *reinterpret_cast<const int*>(weapon + 0x20F8)
              << "/" << *reinterpret_cast<const int*>(weapon + 0x20FC);
        }
    }
    if (auto* box = VitalsBox(player, g_vitalsHpSlot.load())) {
        m << " cfgHp=[" << *reinterpret_cast<const float*>(box + 0x60)
          << "," << *reinterpret_cast<const float*>(box + 0x64) << "]";
    }
    g_hpWatchCount.fetch_add(1, std::memory_order_relaxed);
    Log(m.str());
}

// --- Stamina control (drain / cost / emptyExit) ------------------------------
// Real stamina = hpBox+0x12C (cur) / +0x130 (max) - HPWATCH-proven to track
// the HUD bar (drains on sprint, refills on idle). The engine's per-frame
// updater (FUN_141C4A260) reaches it through vtable+0xC68/+0xC70 virtuals, so
// we do not hook the writer; we read/write the field directly (gauge-lock
// pattern). Fatigue anchors for later: player+0x208EC (threshold float),
// +0x20914 (exhaustion flag byte).
// Config nativePC/plugins/ValorGauge.stamina.txt:
//   drain <ids> <perSec>            continuous drain while the action runs
//   cost <ids> <amount>             one-time cost on action accept (见切)
//   emptyExit <ids> <toSet> <toId>  force a buffered {set,id} switch once the
//                                   bar empties during those actions
std::vector<std::uint32_t> g_stamDrainIds;
float g_stamDrainPerSec = 0.0f;
std::vector<std::uint32_t> g_stamCostIds;
float g_stamCostAmount = 0.0f;
std::vector<std::uint32_t> g_stamExitIds;
std::uint32_t g_stamExitSet = 0;
std::uint32_t g_stamExitId = 0;
// Two-phase exit: phase 1 = sheathe switch, phase 2 = the panting state
// (user-provided 1:324). Stage machine survives action accepts until done
// or a 3s timeout fires.
std::uint32_t g_stamExitThenSet = 0;
std::uint32_t g_stamExitThenId = 0;
int g_stamExitStage = 0;
std::chrono::steady_clock::time_point g_stamExitStageTime{};
bool g_stamExitFired = false;
std::uint32_t g_stamDrainActId = 0;
bool g_stamDrainArmed = false;
float g_stamDrainBase = 0.0f;
std::chrono::steady_clock::time_point g_stamDrainStart{};
std::atomic<std::uint32_t> g_stamLogCount{0};

float* StaminaCurPtr(std::uint8_t* player) {
    auto* box = VitalsBox(player, g_vitalsHpSlot.load());
    if (!box || !IsWritable(box + 0x12C, 4) || !IsReadable(box + 0x130, 4)) {
        return nullptr;
    }
    return reinterpret_cast<float*>(box + 0x12C);
}

void StaminaOnActionAccepted(std::uint32_t actionId) {
    g_stamExitFired = false;
    // Any accepted action change disarms the absolute-drain snapshot so
    // re-entering the move re-snapshots from the CURRENT bar (stale
    // snapshots emptied the bar instantly on re-entry).
    g_stamDrainArmed = false;
    if (g_stamCostIds.empty() || g_stamCostAmount <= 0.0f) {
        return;
    }
    if (std::find(g_stamCostIds.begin(), g_stamCostIds.end(), actionId) ==
        g_stamCostIds.end()) {
        return;
    }
    void* controller = g_lockedPlayerController;
    if (!controller) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (float* cur = StaminaCurPtr(player)) {
        const float before = *cur;
        *cur = std::max(0.0f, before - g_stamCostAmount);
        const auto n = g_stamLogCount.fetch_add(1, std::memory_order_relaxed);
        if (n < 60) {
            std::ostringstream m;
            m << "STAMCOST act=3:" << actionId << " " << before << " -> "
              << *cur << " (-" << g_stamCostAmount << ")";
            Log(m.str());
        }
    }
}

void PollStaminaControl() {
    if ((g_stamDrainIds.empty() && g_stamExitIds.empty())) {
        return;
    }
    void* controller = g_lockedPlayerController;
    if (!controller || !ModWeaponActive()) {
        return;
    }
    ActionInfo cur;
    {
        std::scoped_lock lock(g_actionMutex);
        if (!g_lastLongSwordAction) {
            return;
        }
        cur = *g_lastLongSwordAction;
    }
    const std::uint32_t curSet = static_cast<std::uint32_t>(cur.actionSet);
    const std::uint32_t curId = static_cast<std::uint32_t>(cur.actionId);
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    float* curPtr = StaminaCurPtr(player);
    if (!curPtr) {
        return;
    }
    const auto nowClock = std::chrono::steady_clock::now();

    // --- drain (set-3 watched actions): absolute-set suppression ----------
    if (curSet == 3) {
        const bool draining =
            !g_stamDrainIds.empty() &&
            std::find(g_stamDrainIds.begin(), g_stamDrainIds.end(),
                      curId) != g_stamDrainIds.end();
        if (draining) {
            // Snapshot on entry, then pull the bar DOWN to
            // (snapshot - rate*elapsed) each poll - never raises, wins
            // regardless of engine write order (only sawtooths within a
            // frame). The snapshot disarms on every accepted action change
            // (StaminaOnActionAccepted), so re-entering the move always
            // starts from the CURRENT bar - the old version kept the stale
            // snapshot and instantly emptied the bar on re-entry.
            if (!g_stamDrainArmed || g_stamDrainActId != curId) {
                g_stamDrainArmed = true;
                g_stamDrainActId = curId;
                g_stamDrainBase = *curPtr;
                g_stamDrainStart = nowClock;
            }
            if (g_stamDrainPerSec > 0.0f && *curPtr > 0.0f) {
                const float before = *curPtr;
                const float elapsed = std::chrono::duration<float>(
                                          nowClock - g_stamDrainStart).count();
                const float target = std::max(
                    0.0f, g_stamDrainBase - g_stamDrainPerSec * elapsed);
                if (*curPtr > target) {
                    *curPtr = target;
                }
                static std::int32_t lastLoggedFloor = -999;
                const std::int32_t floored =
                    static_cast<std::int32_t>(*curPtr);
                if (floored != lastLoggedFloor &&
                    g_stamLogCount.load(std::memory_order_relaxed) < 60) {
                    lastLoggedFloor = floored;
                    g_stamLogCount.fetch_add(1, std::memory_order_relaxed);
                    std::ostringstream m;
                    m << "STAMDRAIN act=3:" << curId << " " << before
                      << " -> " << *curPtr << " (" << g_stamDrainPerSec
                      << "/s, abs)";
                    Log(m.str());
                }
            }
        } else {
            g_stamDrainArmed = false;
        }
    } else {
        g_stamDrainArmed = false;
    }

    // --- empty exit, two-phase: sheathe first, then the panting state -----
    // User design: 1:324 = the panting (fatigue) idle; emptying during the
    // stance plays the sheathe, then immediately drops into 1:324.
    const bool exitWatched =
        !g_stamExitIds.empty() &&
        std::find(g_stamExitIds.begin(), g_stamExitIds.end(), curId) !=
            g_stamExitIds.end() &&
        curSet == 3;
    const auto matches = [&](std::uint32_t set, std::uint32_t id) {
        return curSet == set && curId == id;
    };
    auto queueSwitch = [&](std::uint32_t set, std::uint32_t id) {
        auto* ctrl = static_cast<std::uint8_t*>(controller);
        if (IsWritable(ctrl + 0xC0, 4) && IsWritable(ctrl + 0xBC, 4)) {
            *reinterpret_cast<std::int32_t*>(ctrl + 0xBC) =
                static_cast<std::int32_t>(set);
            *reinterpret_cast<std::int32_t*>(ctrl + 0xC0) =
                static_cast<std::int32_t>(id);
            return true;
        }
        return false;
    };
    const bool phase2Configured = g_stamExitThenSet != 0 || g_stamExitThenId != 0;
    // Stage timeouts guard against a queued slot that never consumes.
    if (g_stamExitStage > 0 &&
        std::chrono::duration<float>(nowClock - g_stamExitStageTime).count() >
            3.0f) {
        Log("STAMEXIT stage timeout - reset");
        g_stamExitStage = 0;
        g_stamExitFired = false;
    }
    switch (g_stamExitStage) {
        case 1:
            if (matches(g_stamExitSet, g_stamExitId)) {
                if (phase2Configured) {
                    if (queueSwitch(g_stamExitThenSet, g_stamExitThenId)) {
                        Log("STAMEXIT phase2 queued: panting state");
                        g_stamExitStage = 2;
                        g_stamExitStageTime = nowClock;
                    }
                } else {
                    g_stamExitStage = 0;
                    g_stamExitFired = false;
                }
            } else if (!exitWatched && *curPtr > 0.0f) {
                // Player left the watched action some other way.
                g_stamExitStage = 0;
                g_stamExitFired = false;
            }
            break;
        case 2:
            if (matches(g_stamExitThenSet, g_stamExitThenId)) {
                Log("STAMEXIT panting state reached");
                g_stamExitStage = 0;
                g_stamExitFired = false;
            }
            break;
        default:
            if (*curPtr <= 0.0f && exitWatched && !g_stamExitFired) {
                g_stamExitFired = true;
                if (queueSwitch(g_stamExitSet, g_stamExitId)) {
                    Log("STAMEXIT stamina empty - queued sheathe switch");
                    g_stamExitStage = 1;
                    g_stamExitStageTime = nowClock;
                }
            }
            break;
    }
}

void LoadStaminaConfig() {
    g_stamDrainIds.clear();
    g_stamDrainPerSec = 0.0f;
    g_stamCostIds.clear();
    g_stamCostAmount = 0.0f;
    g_stamExitIds.clear();
    g_stamExitSet = 0;
    g_stamExitId = 0;
    g_stamExitThenSet = 0;
    g_stamExitThenId = 0;
    g_stamExitStage = 0;
    g_stamExitFired = false;
    std::ifstream input("nativePC/plugins/ValorGauge.stamina.txt");
    if (!input) {
        Log("Stamina: no ValorGauge.stamina.txt - stamina control OFF");
        return;
    }
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
        if (kind == "drain") {
            std::string ids;
            float perSec = 0.0f;
            if (ss >> ids >> perSec && ParseIdList(ids, g_stamDrainIds)) {
                g_stamDrainPerSec = std::max(0.0f, std::min(200.0f, perSec));
            } else {
                Log("Stamina: malformed drain line - skipped");
            }
        } else if (kind == "cost") {
            std::string ids;
            float amount = 0.0f;
            if (ss >> ids >> amount && ParseIdList(ids, g_stamCostIds)) {
                g_stamCostAmount = std::max(0.0f, std::min(500.0f, amount));
            } else {
                Log("Stamina: malformed cost line - skipped");
            }
        } else if (kind == "emptyExit") {
            std::string ids;
            unsigned long toSet = 0;
            unsigned long toId = 0;
            unsigned long thenSet = 0;
            unsigned long thenId = 0;
            if (ss >> ids >> toSet >> toId && ParseIdList(ids, g_stamExitIds)) {
                g_stamExitSet = static_cast<std::uint32_t>(toSet);
                g_stamExitId = static_cast<std::uint32_t>(toId);
                ss >> thenSet >> thenId;  // optional phase 2
                g_stamExitThenSet = static_cast<std::uint32_t>(thenSet);
                g_stamExitThenId = static_cast<std::uint32_t>(thenId);
            } else {
                Log("Stamina: malformed emptyExit line - skipped");
            }
        }
    }
    std::ostringstream m;
    m << "Stamina control: drain=";
    if (g_stamDrainIds.empty()) {
        m << "(none)";
    } else {
        for (std::size_t i = 0; i < g_stamDrainIds.size(); ++i) {
            if (i) m << ',';
            m << g_stamDrainIds[i];
        }
        m << '@' << g_stamDrainPerSec << "/s";
    }
    m << " cost=";
    if (g_stamCostIds.empty()) {
        m << "(none)";
    } else {
        for (std::size_t i = 0; i < g_stamCostIds.size(); ++i) {
            if (i) m << ',';
            m << g_stamCostIds[i];
        }
        m << '-' << g_stamCostAmount;
    }
    m << " emptyExit=";
    if (g_stamExitIds.empty()) {
        m << "(none)";
    } else {
        for (std::size_t i = 0; i < g_stamExitIds.size(); ++i) {
            if (i) m << ',';
            m << g_stamExitIds[i];
        }
        m << "->" << g_stamExitSet << ':' << g_stamExitId;
    }
    Log(m.str());
}

#endif // !VALOR_PROBE
