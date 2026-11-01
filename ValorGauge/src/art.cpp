// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L76-L91, L97-L103, L105-L110, L707-L753, L1721-L1766, L6152-L6258. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

// --- 战技槽 / art gauge (2026-10-26, user design) ----------------------------
// MHXX hunter-arts resource as a DLL-EXTERNAL field: no game memory holds it,
// just g_artGauge (0..100) shown as overlay text "ART n%" (a drawn bar is a
// later milestone). Charging: CONNECTED hits (same gate as gainhit/timerhit -
// whiffs never charge) of the actions configured in ValorGauge.art.txt.
// Release: the R+△ combo (KBM Ctrl+LMB) reaches the game as the 3:87 request
// (気刃突刺); while the gauge is below 100 the DoAction hook swallows that
// request outright (the 10-25 session "纯应用层方案"), so it never becomes
// current, never buffers, plays no vanilla move and can enter no FSM node on
// the request path. At 100 the request passes; when the game accepts it the
// gauge drains (consume 1) - the art move itself is the user's FSM work, and
// it MUST be wired via the request path (zero-in-edge node, ActionNo = the
// gated id): a button-property link edge evaluates the raw keys and would
// bypass this gate entirely.
// Config: nativePC/plugins/ValorGauge.art.txt (missing file = system off;
// Shift+F6 reloads). No new game addresses - pure logic on existing hooks.

std::atomic<float> g_artGauge{0.0f};      // the 战技条 itself, 0..100
std::atomic_bool g_artEnabled{false};     // config file present
std::atomic_int g_artConsumeOnPass{1};    // drain on the released request
std::atomic_int32_t g_artGateSet{3};
std::vector<std::int32_t> g_artGateIds;   // requests swallowed below 100
std::vector<ArtChargeRow> g_artChargeRows;
std::mutex g_artMutex;                    // guards the two vectors above

// Damage-based art charging (2026-10-29, art.txt "charge-dmg <dmg> [cap]"):
// when <dmg> > 0 the flat row amount is scaled by damage/<dmg>, capped per
// hit. 0 = off (flat rows only). Calibrate <dmg> to the damage a hit must
// have to charge exactly the configured row amount.
std::atomic<float> g_artDmgScale{0.0f};
std::atomic<float> g_artDmgCap{15.0f};

// DrawGauge(Present)-thread only. Last poll's combat-zone facts, for the
// wipe edges below; -1 = stage never read yet (no edge until first read).
bool g_artLastInRealQuest = false;
std::int32_t g_artLastStageId = -1;

// ART quest gate (2026-10-27): the 战技 gauge is combat-zone-scoped state.
// Three wipe triggers, so a part-charged bar never crosses a session boundary:
// - entering a REAL quest (quest id crosses 0 -> positive). The 探索->任务
//   same-map case changes neither the stage id (both are 101..108) nor
//   necessarily the InQuest bit, only the id - this edge is the one that
//   catches it (user bug report 2026-10-27).
// - zone change (探索A -> 探索B, hub -> anywhere). Real zones only: a -1
//   read (loading screen) arms nothing until a valid id returns.
// - being outside a combat zone at all (hub / rooms / main menu).
// Quest-id and stage reads failing wholesale (post-update RVA drift) never
// edges - the gate just degrades to the combat-zone test. The DoActionHook
// gate keeps swallowing the art combo regardless of all this.
void PollArtQuestGate() {
    if (!g_artEnabled.load(std::memory_order_relaxed)) {
        return;
    }

    std::int32_t questId = 0;
    std::int32_t questState = 0;
    const bool inRealQuest =
        ReadQuestInfo(&questId, &questState) && questState == 2 && questId > 0;
    const std::int32_t stage = ReadStageId();

    const bool questEdge = inRealQuest && !g_artLastInRealQuest;
    const bool zoneEdge =
        stage >= 0 && g_artLastStageId >= 0 && stage != g_artLastStageId;
    g_artLastInRealQuest = inRealQuest;
    g_artLastStageId = stage;

    if ((questEdge || zoneEdge) &&
        g_artGauge.exchange(0.0f, std::memory_order_relaxed) > 0.0f) {
        std::ostringstream m;
        m << "ART gauge wiped ("
          << (questEdge ? "quest start" : "zone change") << ')';
        Log(m.str());
    }

    if (!InCombatZone() &&
        g_artGauge.exchange(0.0f, std::memory_order_relaxed) > 0.0f) {
        Log("ART gauge wiped (not in combat zone)");
    }
}

// --- 战技槽 helpers -----------------------------------------------------------
bool ArtGateMatches(std::int32_t set, std::int32_t id) {
    if (set != g_artGateSet.load(std::memory_order_relaxed)) {
        return false;
    }
    std::scoped_lock lock(g_artMutex);
    return std::find(g_artGateIds.begin(), g_artGateIds.end(), id) !=
           g_artGateIds.end();
}

// Weapon gate WITHOUT the session lock, so the very first combo press of a
// scene is already gated (iron rule 11: local player + weapon gate). The
// local-player side of that rule comes from IsLocalPlayerActionController.
bool ArtWeaponGate(void* controller) {
    if (!controller) {
        return false;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (!IsReadable(player + 0x76B0, sizeof(void*))) {
        return false;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(player + 0x76B0);
    return weapon && IsReadable(weapon + 0x9F8, sizeof(std::uint32_t)) &&
           *reinterpret_cast<const volatile std::uint32_t*>(weapon + 0x9F8) ==
               g_modWeaponType.load(std::memory_order_relaxed);
}

// Exact charge row wins over the wildcard; 0 = no row (or non-positive).
float ArtChargePercentFor(std::int32_t set, std::int32_t id) {
    float wildcard = 0.0f;
    {
        std::scoped_lock lock(g_artMutex);
        for (const auto& row : g_artChargeRows) {
            if (row.set != set) {
                continue;
            }
            if (row.id == id && id != 0) {
                return row.percent;
            }
            if (row.id == 0 && wildcard == 0.0f) {
                wildcard = row.percent;
            }
        }
    }
    return wildcard;
}

#ifndef VALOR_PROBE
// --- 战技槽 config -------------------------------------------------------------
// nativePC/plugins/ValorGauge.art.txt (missing file = system fully off):
//   gate <set> <id> [id...]      - requests swallowed while gauge < 100
//                                  (default 3 87 = the R+△ / Ctrl+LMB combo)
//   charge <set> <id> <percent>  - +percent per CONNECTED hit of the action
//                                  (id 0 = wildcard, exact rows win). Repeatable.
//   charge-dmg <dmg> [cap]       - damage-based scaling (2026-10-29): the
//                                  matched row amount is applied as
//                                  amount*damage/<dmg>, capped at <cap> per
//                                  hit (default 15). <dmg> = the damage a hit
//                                  needs to charge exactly the row amount.
//                                  0-damage hits charge nothing. Delete the
//                                  line = flat mode. Shift+F6 reloads.
//   consume <0|1>                - drain to 0 when the released request is
//                                  accepted (default 1). Shift+F6 reloads.
void LoadArtConfig() {
    std::vector<std::int32_t> gateIds;
    std::int32_t gateSet = 3;
    std::vector<ArtChargeRow> rows;
    int consume = 1;
    g_artDmgScale.store(0.0f, std::memory_order_relaxed);  // reload = opt-out
    g_artDmgCap.store(15.0f, std::memory_order_relaxed);
    const bool enabled =
        std::filesystem::exists("nativePC/plugins/ValorGauge.art.txt");
    std::ifstream input("nativePC/plugins/ValorGauge.art.txt");
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
        if (kind == "gate") {
            std::int32_t set = 0;
            if (!(ss >> set)) {
                continue;
            }
            gateSet = set;
            std::int32_t id = 0;
            while (ss >> id) {
                gateIds.push_back(id);
            }
        } else if (kind == "charge") {
            ArtChargeRow row;
            if (ss >> row.set >> row.id >> row.percent) {
                rows.push_back(row);
            }
        } else if (kind == "charge-dmg") {
            // Damage-based scaling: <damagePerScale> [capPercent]. When set,
            // the matched row amount is scaled by damage/<damagePerScale>
            // (capped) instead of applying flat. Delete the line = flat mode.
            float scale = 0.0f;
            if (ss >> scale) {
                float cap = 15.0f;
                if (ss >> cap) {
                    g_artDmgCap.store(cap, std::memory_order_relaxed);
                }
                g_artDmgScale.store(scale, std::memory_order_relaxed);
            }
        } else if (kind == "consume") {
            int value = 0;
            if (ss >> value) {
                consume = value != 0;
            }
        }
    }
    if (gateIds.empty()) {
        gateIds.push_back(87);
    }
    {
        std::scoped_lock lock(g_artMutex);
        g_artGateIds = std::move(gateIds);
        g_artChargeRows = std::move(rows);
    }
    g_artGateSet.store(gateSet, std::memory_order_relaxed);
    g_artConsumeOnPass.store(consume, std::memory_order_relaxed);
    g_artEnabled.store(enabled, std::memory_order_relaxed);
    std::ostringstream m;
    m << "ART gauge " << (enabled ? "ON" : "OFF (no ValorGauge.art.txt)")
      << ": gate " << gateSet << ":{";
    {
        std::scoped_lock lock(g_artMutex);
        for (std::size_t i = 0; i < g_artGateIds.size(); ++i) {
            if (i) {
                m << ",";
            }
            m << g_artGateIds[i];
        }
    }
    m << "} swallowed below 100%, charge";
    {
        std::scoped_lock lock(g_artMutex);
        if (g_artChargeRows.empty()) {
            m << " = none";
        } else {
            for (const auto& r : g_artChargeRows) {
                m << " " << r.set << ':' << r.id << " +" << r.percent << "%";
            }
        }
    }
    m << ", consume=" << consume << " (Shift+F6 reloads)";
    Log(m.str());
}

#endif // !VALOR_PROBE
