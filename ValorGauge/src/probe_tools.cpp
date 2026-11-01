// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L6999-L7403, L7471-L8277. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

#ifdef VALOR_PROBE
// Optional nativePC\plugins\ValorGauge.prop-watch.txt: property ids to watch,
// up to 6, e.g. "222 631".
void LoadPropWatchConfig() {
    std::ifstream trigger("nativePC/plugins/ValorGauge.prop-trigger.txt");
    std::int32_t tid = 0;
    std::int32_t tidx = 0;
    if (trigger >> tid >> tidx && tid >= 0 && tidx >= 0) {
        g_triggerId = static_cast<std::uint32_t>(tid);
        g_triggerIndex = static_cast<std::uint32_t>(tidx);
    }
    std::ostringstream msg;
    msg << "Prop trigger configured: id=" << g_triggerId << " idx=" << g_triggerIndex
        << (g_triggerId == 0 ? " (disabled)" : "");
    Log(msg.str());
    std::ifstream input("nativePC\\plugins\\ValorGauge.prop-watch.txt");
    std::int32_t id = 0;
    int count = 0;
    while (count < kMaxWatchIds && (input >> id) && id >= 0) {
        g_watchIds[count++] = static_cast<std::uint32_t>(id);
    }
    if (count > 0) {
        g_watchCount = count;
    }
    std::ostringstream watchMsg;
    watchMsg << "Property watch list:";
    for (int wi = 0; wi < g_watchCount; ++wi) {
        watchMsg << ' ' << g_watchIds[wi];
    }
    Log(watchMsg.str());
}

// --- F6 property write test (Shift+F6) ---------------------------------------
// Writes the 0xDE-family counter row through the verified chain. Requires the
// property to be bound (key != 0), i.e. some FSM node must reference it.
std::uint32_t g_propWriteId = 222;
std::uint32_t g_propWriteIndex = 0;
std::int32_t g_propWriteValue = 1;

// Optional nativePC\plugins\ValorGauge.prop-write.txt: "<id> <index> <value>".
void LoadPropWriteConfig() {
    std::ifstream input("nativePC\\plugins\\ValorGauge.prop-write.txt");
    std::int32_t id = 0;
    std::int32_t index = 0;
    std::int32_t value = 0;
    if (input >> id >> index >> value && id >= 0 && index >= 0) {
        g_propWriteId = static_cast<std::uint32_t>(id);
        g_propWriteIndex = static_cast<std::uint32_t>(index);
        g_propWriteValue = value;
    }
    std::ostringstream msg;
    msg << "Property write test configured: Shift+F6 writes id=" << g_propWriteId
        << " idx=" << g_propWriteIndex << " value=" << g_propWriteValue;
    Log(msg.str());
}

void TryPropertyWrite() {
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
        Log("PROPWRITE: player controller not locked yet");
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    std::uint32_t key = 0;
    auto* counters = ResolvePropCounters(player, g_propWriteId, &key);
    if (counters && key == 0) {
        Log("PROPWRITE: property key slot is 0 - unbound (no FSM node references it); "
            "add an R/property condition via XFsm and reload the area first");
        return;
    }
    if (!counters) {
        Log("PROPWRITE: property row unresolved");
        return;
    }
    auto* target = counters + g_propWriteIndex;
    DWORD oldProtect = 0;
    const bool directlyWritable = IsWritable(target, sizeof(std::int16_t));
    if (!directlyWritable) {
        MEMORY_BASIC_INFORMATION mbi{};
        VirtualQuery(target, &mbi, sizeof(mbi));
        std::ostringstream m;
        m << std::hex << std::uppercase << "PROPWRITE: target=0x"
          << reinterpret_cast<std::uintptr_t>(target) << " protect=0x" << mbi.Protect
          << " state=0x" << mbi.State << std::dec << " - trying VirtualProtect";
        Log(m.str());
        if (!VirtualProtect(target, sizeof(std::int16_t), PAGE_READWRITE, &oldProtect)) {
            Log("PROPWRITE: VirtualProtect failed; write aborted");
            return;
        }
    }

    std::ostringstream before;
    before << "PROPWRITE id=" << std::dec << g_propWriteId << " idx=" << g_propWriteIndex
           << " key=" << key << std::hex << std::uppercase << " counters=0x"
           << reinterpret_cast<std::uintptr_t>(counters) << std::dec
           << " before=[" << counters[0] << ',' << counters[1] << ',' << counters[2] << ']';
    Log(before.str());

    counters[g_propWriteIndex] = static_cast<std::int16_t>(g_propWriteValue);

    if (!directlyWritable) {
        VirtualProtect(target, sizeof(std::int16_t), oldProtect, &oldProtect);
    }

    std::ostringstream after;
    after << "PROPWRITE wrote " << g_propWriteValue << (directlyWritable ? "" : " (protected)")
          << "; readback=[" << counters[0] << ',' << counters[1] << ',' << counters[2] << ']';
    Log(after.str());
}

// --- F9 clang miner: paired-scenario state mining -----------------------------
// F5 #1: start mining (phase 1). Each 见切 (3:86 entry) records one run of
// per-frame memory deltas over weapon/mgr/pool/FSM regions plus HP.
// F5 #2: advance to phase 2 (do the other scenario, e.g. real clangs).
// F5 #3: stop and dump the analysis: offsets whose change pattern is
// exclusive to one phase = candidate clang-state signals, no guessing.
struct MinerChange { std::uint32_t key; std::uint32_t val; std::uint32_t frame; };
struct MinerRun {
    int phase = 0;
    float hpStart = 0.0f;
    float hpEnd = 0.0f;
    std::uint32_t frames = 0;
    std::vector<MinerChange> changes;
};
struct MinerRegionSpec {
    std::uint8_t* base = nullptr;
    std::size_t size = 0;
    const char* name = "";
    std::vector<std::uint8_t> prev;
};
std::vector<MinerRegionSpec> g_minerRegions;
std::vector<MinerRun> g_minerRuns;
MinerRun g_minerCur;
bool g_minerInRun = false;
int g_minerPhase = 0;
bool g_minerActive = false;
std::uint32_t g_minerFrameInRun = 0;
constexpr std::size_t kMinerMaxChanges = 30000;

float MinerReadHealth(std::uint8_t* player) {
    auto* hpBox = player + 0x7630; // {player+0x7630} + 0x64 (Lua engine layout)
    if (!IsReadable(hpBox, sizeof(void*))) {
        return -1.0f;
    }
    auto* box = *reinterpret_cast<std::uint8_t**>(hpBox);
    if (!box || !IsReadable(box + 0x64, sizeof(float))) {
        return -1.0f;
    }
    return *reinterpret_cast<const float*>(box + 0x64);
}

void MinerSetupRegions(std::uint8_t* player) {
    g_minerRegions.clear();
    auto* weaponSlot = player + 0x76B0;
    if (IsReadable(weaponSlot, sizeof(void*)) && *reinterpret_cast<void**>(weaponSlot)) {
        auto* weapon = *reinterpret_cast<std::uint8_t**>(weaponSlot);
        if (IsReadable(weapon, 0x2400)) {
            g_minerRegions.push_back({weapon, 0x2400, "weapon", {}});
        }
    }
    auto* mgrSlot = player + 0x7D20;
    if (IsReadable(mgrSlot, sizeof(void*)) && *reinterpret_cast<void**>(mgrSlot)) {
        auto* mgr = *reinterpret_cast<std::uint8_t**>(mgrSlot);
        if (IsReadable(mgr, 0x800)) {
            g_minerRegions.push_back({mgr, 0x800, "mgr", {}});
            auto* pool = mgr + 0x28;
            if (IsReadable(pool, 0x180)) {
                g_minerRegions.push_back({pool, 0x180, "pool", {}});
            }
        }
    }
    // FSM fields on the player struct (controller + fsmTarget/id block).
    if (IsReadable(player + 0x6100, 0x900)) {
        g_minerRegions.push_back({player + 0x6100, 0x900, "playerFsm", {}});
    }
    // Player combat windows: event bit array (+0x8938!), flags, broad sweep.
    struct PlayerWindow { std::size_t off; std::size_t size; const char* name; };
    const PlayerWindow windows[] = {
        {0x1000, 0x400, "pFlags"},
        {0x1200, 0x7700, "pCore"},
        {0x8900, 0x180, "pEventBits"},
        {0x8A80, 0x2A80, "pTail"},
    };
    for (const auto& w : windows) {
        if (IsReadable(player + w.off, w.size)) {
            g_minerRegions.push_back({player + w.off, w.size, w.name, {}});
        }
    }
    for (auto& r : g_minerRegions) {
        r.prev.assign(r.base, r.base + r.size);
    }
}

void MinerBeginRun(std::uint8_t* player) {
    MinerSetupRegions(player);
    g_minerCur = MinerRun{};
    g_minerCur.phase = g_minerPhase;
    g_minerCur.hpStart = MinerReadHealth(player);
    g_minerFrameInRun = 0;
    g_minerInRun = true;
}

void MinerEndRun() {
    g_minerInRun = false;
    g_minerRuns.push_back(std::move(g_minerCur));
    g_minerCur = MinerRun{};
    std::ostringstream m;
    m << "MINER run " << g_minerRuns.size() << " (phase " << g_minerRuns.back().phase
      << "): " << g_minerRuns.back().frames << " frames, "
      << g_minerRuns.back().changes.size() << " changes, HP "
      << g_minerRuns.back().hpStart << " -> " << g_minerRuns.back().hpEnd;
    Log(m.str());
}

void MinerCaptureFrame() {
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (!g_minerActive) {
        g_minerActive = true;
        g_minerPhase = 1;
        Log("MINER auto-started: recording 见切/居合/翻滚(3:18) as runs. "
            "F5 marks a new phase + dumps a snapshot.");
        static bool keyDumped = false;
        if (!keyDumped) {
            keyDumped = true;
            LogKeyTableDump(player);
        }
    }

    bool inEvasion = false;
    {
        std::scoped_lock lock(g_actionMutex);
        inEvasion = g_lastLongSwordAction && g_lastLongSwordAction->actionSet == 3 &&
               (g_lastLongSwordAction->actionId == 86 ||
                g_lastLongSwordAction->actionId == 101 ||
                g_lastLongSwordAction->actionId == 102 ||
                g_lastLongSwordAction->actionId == 18);
    }
    if (inEvasion && !g_minerInRun) {
        MinerBeginRun(player);
    }
    if (!inEvasion && g_minerInRun) {
        MinerEndRun();
        while (g_minerRuns.size() > 16) {
            g_minerRuns.erase(g_minerRuns.begin());
        }
        return;
    }
    if (!g_minerInRun) {
        return;
    }
    ++g_minerFrameInRun;
    g_minerCur.frames = g_minerFrameInRun;
    g_minerCur.hpEnd = MinerReadHealth(player);

    for (std::size_t ri = 0; ri < g_minerRegions.size(); ++ri) {
        auto& r = g_minerRegions[ri];
        if (!IsReadable(r.base, r.size)) {
            continue;
        }
        const auto* cur = reinterpret_cast<const std::uint32_t*>(r.base);
        auto* prev = reinterpret_cast<std::uint32_t*>(r.prev.data());
        const auto dwordCount = r.size / sizeof(std::uint32_t);
        const auto regionTag = static_cast<std::uint32_t>(ri << 24);
        for (std::size_t d = 0; d < dwordCount; ++d) {
            if (cur[d] != prev[d]) {
                prev[d] = cur[d];
                if (g_minerCur.changes.size() < kMinerMaxChanges) {
                    const auto offset = static_cast<std::uint32_t>(d * 4);
                    g_minerCur.changes.push_back(
                        {regionTag | offset, cur[d], g_minerFrameInRun});
                }
            }
        }
    }
}

void MinerDumpAnalysis() {
    std::ostringstream header;
    header << "MINER ANALYSIS: " << g_minerRuns.size() << " runs";
    Log(header.str());
    for (std::size_t i = 0; i < g_minerRuns.size(); ++i) {
        std::ostringstream m;
        m << "MINER run " << (i + 1) << " phase=" << g_minerRuns[i].phase
          << " frames=" << g_minerRuns[i].frames << " changes="
          << g_minerRuns[i].changes.size() << std::fixed << std::setprecision(1)
          << " HP " << g_minerRuns[i].hpStart << "->" << g_minerRuns[i].hpEnd;
        Log(m.str());
    }
    if (g_minerRuns.empty()) {
        return;
    }
    // key -> per-phase stats {runsChanged, minFirstFrame}
    struct KeyStat { int changed[3] = {}; int minFrame[3] = {}; };
    std::map<std::uint32_t, KeyStat> stats;
    for (const auto& run : g_minerRuns) {
        if (run.phase < 1 || run.phase > 2) {
            continue;
        }
        std::map<std::uint32_t, std::uint32_t> firstFrame;
        for (const auto& c : run.changes) {
            if (!firstFrame.count(c.key) || c.frame < firstFrame[c.key]) {
                firstFrame[c.key] = c.frame;
            }
        }
        for (const auto& [key, frame] : firstFrame) {
            auto& st = stats[key];
            ++st.changed[run.phase];
            if (st.minFrame[run.phase] == 0 || frame < st.minFrame[run.phase]) {
                st.minFrame[run.phase] = frame;
            }
        }
    }
    // Correlation-ranked report: keys whose change rate across ALL phase-2
    // runs is higher than across ALL phase-1 runs, strongest first.
    struct KeyInfo {
        int changed[3] = {};
        int minFrame[3] = {};
    };
    // Phase 1 = whiffs, every LATER phase = clangs (users often do mixed
    // runs in phase 2 and clean clangs in 3+; all of them are clang data).
    int phaseTotal[3] = {};
    for (const auto& run : g_minerRuns) {
        if (run.phase >= 1) {
            ++phaseTotal[run.phase == 1 ? 1 : 2];
        }
    }
    std::map<std::uint32_t, KeyInfo> kinfo;
    for (const auto& run : g_minerRuns) {
        if (run.phase < 1) {
            continue;
        }
        const int slot = run.phase == 1 ? 1 : 2;
        std::map<std::uint32_t, std::uint32_t> firstFrame;
        for (const auto& c : run.changes) {
            if (!firstFrame.count(c.key) || c.frame < firstFrame[c.key]) {
                firstFrame[c.key] = c.frame;
            }
        }
        for (const auto& [key, frame] : firstFrame) {
            auto& ki = kinfo[key];
            ++ki.changed[slot];
            if (ki.minFrame[slot] == 0 || frame < ki.minFrame[slot]) {
                ki.minFrame[slot] = frame;
            }
        }
    }
    std::vector<std::pair<std::uint32_t, KeyInfo>> ranked(kinfo.begin(), kinfo.end());
    const double wTotal = phaseTotal[1] > 0 ? phaseTotal[1] : 1;
    const double cTotal = phaseTotal[2] > 0 ? phaseTotal[2] : 1;
    std::sort(ranked.begin(), ranked.end(),
              [&](const auto& a, const auto& b) {
                  const double sa = a.second.changed[2] / cTotal;
                  const double sb = b.second.changed[2] / cTotal;
                  const double wa = a.second.changed[1] / wTotal;
                  const double wb = b.second.changed[1] / wTotal;
                  return (sa - wa) > (sb - wb);
              });
    const char* regionNames[8] = {"weapon", "mgr", "pool", "playerFsm",
                                  "pFlags", "pCore", "pEventBits", "pTail"};
    int reported = 0;
    for (const auto& [key, ki] : ranked) {
        if (reported >= 60) {
            break;
        }
        const int region = static_cast<int>((key >> 24) & 7);
        const auto offset = key & 0x00FFFFFF;
        if (reported == 0) {
            std::ostringstream hdr;
            hdr << "MINER CANDIDATES (whiff runs=" << std::dec << phaseTotal[1]
                << ", clang runs=" << phaseTotal[2] << "):";
            Log(hdr.str());
        }
        ++reported;
        std::ostringstream m;
        m << "MINER CANDIDATE " << regionNames[region] << "+0x" << std::hex
          << std::uppercase << offset << std::dec << " whiff " << ki.changed[1] << '/'
          << phaseTotal[1] << " clang " << ki.changed[2] << '/' << phaseTotal[2]
          << " firstFrame w=" << ki.minFrame[1] << " c=" << ki.minFrame[2];
        Log(m.str());
    }
    Log("MINER ANALYSIS END");
}

void MinerHandleF9() {
    // Recording is ALWAYS on once the controller is locked; F5 tags upcoming
    // runs with the next phase and dumps a ranked snapshot of everything so
    // far. Nothing to start, nothing to stop.
    ++g_minerPhase;
    MinerDumpAnalysis();
    std::ostringstream m;
    m << "MINER: now tagging runs as phase " << g_minerPhase
      << " (previous phases kept in the report)";
    Log(m.str());
}

// --- F12 ATK-row scan + guard-page access trace ------------------------------
// Part-break (破坏值) question 2026-10-18: which cAttackParamPl field feeds the
// per-hit part-break accumulation and whether anything reads it at all on the
// custom row. cAttackParamPl = col ATK type-1 row object (0x270 bytes; factory
// jump table 0x143F949E0[type], class name string at 0x142F12DD0); runtime
// layout from Load FUN_14027d880 / Save FUN_14027dd20 (row 73 live-confirmed
// 2026-10-19 via ATKHEX: +0x1F0/+0x210 mirrored the deployed HitLag edit):
//   +0x00 vtable(0x142F12DE0) +0x08 Index u32 +0x0c Index2 u32
//   +0x18 Attack_Type u32 +0x20 Motion_Value +0x24 Fixed_Damage
//   +0x28 Part_Modifier +0x68 Stun +0x6c Exhaust +0x1c8 Soften
//   +0x1d0 Elem_Mod +0x1d4 Status_Mod +0x1d8 Mount +0x240 Flinch u32
//   +0x1ec HitLagSpd_a +0x1f0 HitLag_a +0x20c HitLagSpd_b +0x210 HitLag_b
// F12 arms one session: background scan of private RW memory for the vtable
// pointer, per-row dump, hex dump of the configured row, then a PAGE_GUARD
// trace over that row for 30 s (VEH logs rw/offset/rip/stack per access;
// neighbour-row faults on the same page restore the guard instead of
// disarming it - they are counted and reported in the session-over line).
// Config nativePC/plugins/ValorProbe.atktrace.txt = "<rowIndex>" (default 22).
static std::atomic<std::uintptr_t> g_atkRowTarget{0};
static std::atomic<long long> g_atkTraceDeadline{0};
static std::atomic<unsigned> g_atkTraceLines{0};
static std::atomic<unsigned> g_atkNeighborFaults{0};
static std::atomic<unsigned> g_atkNeiLines{0};
static std::atomic<bool> g_atkReprotectPending{false};
static std::atomic<bool> g_atkVehInstalled{false};
static std::atomic<bool> g_atkScanRunning{false};
static std::atomic<std::uintptr_t> g_atkGuardedRows[8] = {};
static std::atomic<unsigned> g_atkGuardedRowCount{0};
static int g_atkRowCfg = 22;
bool g_atkKeyWasDown = false;

struct AtkGuardedPage {
    void* base;
    SIZE_T size;
    DWORD oldProtect;
};
static std::vector<AtkGuardedPage> g_atkGuardedPages;
static std::mutex g_atkGuardMutex;

// (Re)arm PAGE_GUARD on every page overlapping one row. Safe to call again
// after a fault consumed the guard bit - that is exactly how the single-step
// handler restores it. A previous build skipped re-arming whenever the page
// list was non-empty, which silently turned the whole trace one-shot.
static void AtkGuardRowPages(std::uintptr_t target) {
    if (!target) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_atkGuardMutex);
    const auto pageStart = target & ~static_cast<std::uintptr_t>(0xFFF);
    const auto pageEnd = (target + 0x270 + 0xFFF) & ~static_cast<std::uintptr_t>(0xFFF);
    for (auto page = pageStart; page < pageEnd; page += 0x1000) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(page), &info, sizeof(info)) ||
            info.State != MEM_COMMIT) {
            continue;
        }
        DWORD oldProtect = 0;
        if (VirtualProtect(reinterpret_cast<void*>(page), 0x1000,
                           (info.Protect & ~PAGE_GUARD) | PAGE_GUARD, &oldProtect)) {
            bool known = false;
            for (const auto& pg : g_atkGuardedPages) {
                known |= (pg.base == reinterpret_cast<void*>(page));
            }
            if (!known) {
                g_atkGuardedPages.push_back(
                    {reinterpret_cast<void*>(page), 0x1000, oldProtect});
            }
        } else {
            static std::atomic<unsigned> guardFails{0};
            if (guardFails.fetch_add(1, std::memory_order_relaxed) < 3) {
                std::ostringstream m;
                m << "ATKTRACE: VirtualProtect(guard) FAILED page=0x" << std::hex
                  << page << " err=" << std::dec << GetLastError();
                Log(m.str());
            }
        }
    }
}

static void AtkReprotectAll() {
    std::lock_guard<std::mutex> lock(g_atkGuardMutex);
    for (auto& pg : g_atkGuardedPages) {
        DWORD dontCare = 0;
        VirtualProtect(pg.base, pg.size,
                       (pg.oldProtect & ~PAGE_GUARD) | PAGE_GUARD, &dontCare);
    }
}

static void AtkRemoveGuard() {
    std::lock_guard<std::mutex> lock(g_atkGuardMutex);
    for (const auto& page : g_atkGuardedPages) {
        DWORD dontCare = 0;
        VirtualProtect(page.base, page.size, page.oldProtect, &dontCare);
    }
    g_atkGuardedPages.clear();
    g_atkGuardedRowCount.store(0, std::memory_order_relaxed);
    for (auto& r : g_atkGuardedRows) {
        r.store(0, std::memory_order_relaxed);
    }
}

static LONG NTAPI AtkGuardVectored(PEXCEPTION_POINTERS info) {
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code != STATUS_GUARD_PAGE_VIOLATION) {
        if (code == STATUS_SINGLE_STEP &&
            g_atkReprotectPending.exchange(false, std::memory_order_relaxed)) {
            // The faulting instruction finished; put the guards back.
            info->ContextRecord->EFlags &= ~0x100;
            AtkReprotectAll();
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const int rowCount = static_cast<int>(
        g_atkGuardedRowCount.load(std::memory_order_relaxed));
    if (rowCount <= 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const auto addr = static_cast<std::uintptr_t>(
        info->ExceptionRecord->ExceptionInformation[1]);
    const auto base = g_moduleBase.load(std::memory_order_relaxed);
    for (int i = 0; i < rowCount; ++i) {
        const auto target = g_atkGuardedRows[i].load(std::memory_order_relaxed);
        if (!target) {
            continue;
        }
        const bool inRow = addr >= target && addr < target + 0x270;
        const auto page = addr & ~static_cast<std::uintptr_t>(0xFFF);
        const bool onPage = page >= (target & ~static_cast<std::uintptr_t>(0xFFF)) &&
                            page <= ((target + 0x270 - 1) & ~static_cast<std::uintptr_t>(0xFFF));
        if (!inRow && !onPage) {
            continue;
        }
        // Re-run the faulting instruction, then restore the guard on the
        // single step. Without this the OS-cleared PAGE_GUARD bit never comes
        // back and the whole session dies on the first touch.
        info->ContextRecord->EFlags |= 0x100;
        g_atkReprotectPending.store(true, std::memory_order_relaxed);

        if (inRow) {
            const unsigned line =
                g_atkTraceLines.fetch_add(1, std::memory_order_relaxed);
            if (line < 1500) {
                const char* rw =
                    info->ExceptionRecord->ExceptionInformation[0] == 0
                        ? "R"
                        : (info->ExceptionRecord->ExceptionInformation[0] == 1
                               ? "W"
                               : "X");
                void* stack[10] = {};
                USHORT frames = RtlCaptureStackBackTrace(0, 10, stack, nullptr);
                std::ostringstream m;
                m << "ATKACC i" << std::dec << i << " " << rw << " off=0x"
                  << std::hex << (addr - target) << " rip=+0x"
                  << (info->ContextRecord->Rip - base);
                int printed = 0;
                for (USHORT f = 0; f < frames && printed < 6; ++f) {
                    const auto pc = reinterpret_cast<std::uintptr_t>(stack[f]);
                    if (pc >= base && pc < base + 0x2000000) {
                        m << (printed == 0 ? " stk=" : ",") << "+0x" << (pc - base);
                        ++printed;
                    }
                }
                Log(m.str());
            }
        } else {
            // Neighbouring allocation on a guarded page: count it, and log
            // the first ones - the reader RIP is the breadcrumb to whoever
            // walks these rows at runtime.
            g_atkNeighborFaults.fetch_add(1, std::memory_order_relaxed);
            const unsigned nl = g_atkNeiLines.fetch_add(1, std::memory_order_relaxed);
            if (nl < 200) {
                const char* rw =
                    info->ExceptionRecord->ExceptionInformation[0] == 0
                        ? "R"
                        : (info->ExceptionRecord->ExceptionInformation[0] == 1
                               ? "W"
                               : "X");
                void* stack[10] = {};
                USHORT frames = RtlCaptureStackBackTrace(0, 10, stack, nullptr);
                std::ostringstream m;
                m << "ATKNEI i" << std::dec << i << " " << rw << " rel="
                  << static_cast<long long>(addr) -
                         static_cast<long long>(target)
                  << " rip=+0x" << std::hex
                  << (info->ContextRecord->Rip - base);
                int printed = 0;
                for (USHORT f = 0; f < frames && printed < 4; ++f) {
                    const auto pc = reinterpret_cast<std::uintptr_t>(stack[f]);
                    if (pc >= base && pc < base + 0x2000000) {
                        m << (printed == 0 ? " stk=" : ",") << "+0x" << (pc - base);
                        ++printed;
                    }
                }
                Log(m.str());
            }
        }
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// SEH can't live in a function with unwinding objects - keep the guard-page
// self-test reads in their own no-throw helper.
static bool AtkSelftestRead(std::uintptr_t addr, std::uint32_t* mv,
                            std::uint32_t* lag) {
    __try {
        const auto* p = reinterpret_cast<const std::uint8_t*>(addr);
        *mv = *reinterpret_cast<const std::uint32_t*>(p + 0x20);
        *lag = *reinterpret_cast<const std::uint32_t*>(p + 0x1F0);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *mv = 0xDEADBEEFu;
        *lag = 0xDEADBEEFu;
        return false;
    }
}

static void AtkScanThread(int wantIdx) {
    const auto base = g_moduleBase.load(std::memory_order_relaxed);
    if (!base) {
        Log("ATKSCAN: no module base yet");
        g_atkScanRunning.store(false, std::memory_order_relaxed);
        return;
    }
    const std::uintptr_t vtable = base + 0x2F12DE0;
    std::uint8_t needle[8];
    std::memcpy(needle, &vtable, sizeof(needle));

    // Raw scans run under SEH: the game streams resources and can free a heap
    // region between VirtualQuery and the read.
    struct Row {
        std::uintptr_t addr;
        std::uint32_t idx;
    };
    std::vector<Row> rows;

    auto scanRegion = [&](const std::uint8_t* start, std::size_t size) {
        if (size < 0x270) {
            return;
        }
        std::uintptr_t candidates[512];
        std::size_t count = 0;
        __try {
            for (std::size_t i = 0; i + 8 <= size; ++i) {
                if (start[i] != needle[0] ||
                    std::memcmp(start + i, needle, 8) != 0) {
                    continue;
                }
                if (count < 512) {
                    candidates[count] =
                        reinterpret_cast<std::uintptr_t>(start + i);
                }
                ++count;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        for (std::size_t c = 0; c < count && c < 512; ++c) {
            const auto a = candidates[c];
            std::uint32_t idx = 0xFFFFFFFFu;
            std::uint32_t type = 0xFFFFFFFFu;
            float mv = 0.0f;
            const auto* p = reinterpret_cast<const std::uint8_t*>(a);
            __try {
                idx = *reinterpret_cast<const std::uint32_t*>(p + 0x08);
                type = *reinterpret_cast<const std::uint32_t*>(p + 0x18);
                mv = *reinterpret_cast<const float*>(p + 0x20);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                continue;
            }
            if (idx < 1024 && type < 32 && std::isfinite(mv) &&
                std::fabs(mv) < 100000.0f) {
                rows.push_back({a, idx});
            }
        }
    };

    std::uintptr_t regionStart = 0;
    while (regionStart < 0x7FFFFFFEFFFF) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(regionStart), &info, sizeof(info))) {
            break;
        }
        const auto next = regionStart + info.RegionSize;
        if (info.State == MEM_COMMIT && info.Type == MEM_PRIVATE &&
            (info.Protect & PAGE_READWRITE) != 0 &&
            (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0) {
            scanRegion(reinterpret_cast<const std::uint8_t*>(info.BaseAddress),
                       info.RegionSize);
        }
        if (next <= regionStart) {
            break;
        }
        regionStart = next;
    }

    std::ostringstream summary;
    summary << "ATKSCAN: " << rows.size() << " cAttackParamPl rows (vtable +0x2F12DE0)";
    Log(summary.str());
    if (rows.empty()) {
        Log("ATKSCAN: no rows resident - load a quest first (col not parsed yet?)");
        g_atkScanRunning.store(false, std::memory_order_relaxed);
        return;
    }

    unsigned dumped = 0;
    std::vector<Row> matches;
    for (const auto& row : rows) {
        auto* p = reinterpret_cast<const std::uint8_t*>(row.addr);
        auto u32 = [p](std::size_t off) {
            return *reinterpret_cast<const std::uint32_t*>(p + off);
        };
        auto f32 = [p](std::size_t off) {
            return *reinterpret_cast<const float*>(p + off);
        };
        if (static_cast<int>(row.idx) == wantIdx) {
            matches.push_back(row);
        }
        if (dumped++ < 120) {
            std::ostringstream m;
            m << "ATKROW ptr=0x" << std::hex << row.addr << std::dec
              << " idx=" << row.idx << " type=" << u32(0x18)
              << " mv=" << f32(0x20) << " fixed=" << f32(0x24)
              << " part=" << f32(0x28) << " stun=" << f32(0x68)
              << " exh=" << f32(0x6c) << " soft=" << f32(0x1c8)
              << " mnt=" << f32(0x1d8) << " flinch=" << u32(0x240);
            Log(m.str());
        }
    }

    if (matches.empty()) {
        std::ostringstream m;
        m << "ATKSCAN: row " << wantIdx
          << " not found among resident rows (col without that row loaded?)";
        Log(m.str());
        g_atkScanRunning.store(false, std::memory_order_relaxed);
        return;
    }
    if (matches.size() > 8) {
        matches.resize(8);
    }

    {
        auto* p = reinterpret_cast<const std::uint8_t*>(matches[0].addr);
        std::ostringstream m;
        m << "ATKHEX row " << matches[0].idx << " @0x" << std::hex
          << matches[0].addr << std::dec << " (u32 view):";
        // 0x270 = the row itself; the tail up to 0xE00 covers the runtime
        // neighbours that ATKNEI showed being read at hit time (row+0xB40..).
        for (std::size_t off = 0; off < 0xE00; off += 64) {
            if (off == 0x270) {
                m << "\n  ---- beyond row: neighbour/context region ----";
            }
            m << "\n  +0x" << std::hex << off << std::dec << ":";
            for (std::size_t w = off; w < off + 64 && w < 0xE00; w += 4) {
                m << ' ' << std::hex << std::setw(8) << std::setfill('0')
                  << *reinterpret_cast<const std::uint32_t*>(p + w) << std::dec;
            }
        }
        Log(m.str());
    }

    if (!g_atkVehInstalled.exchange(true, std::memory_order_relaxed)) {
        AddVectoredExceptionHandler(1, AtkGuardVectored);
        Log("ATKTRACE: VEH installed");
    }
    g_atkTraceLines.store(0, std::memory_order_relaxed);
    g_atkNeighborFaults.store(0, std::memory_order_relaxed);
    g_atkNeiLines.store(0, std::memory_order_relaxed);
    for (std::size_t k = 0; k < matches.size(); ++k) {
        g_atkGuardedRows[k].store(matches[k].addr, std::memory_order_relaxed);
        AtkGuardRowPages(matches[k].addr);
    }
    g_atkGuardedRowCount.store(
        static_cast<unsigned>(matches.size()), std::memory_order_relaxed);
    g_atkRowTarget.store(matches[0].addr, std::memory_order_relaxed);
    g_atkTraceDeadline.store(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                .count() +
            30,
        std::memory_order_relaxed);
    {
        std::ostringstream m;
        m << "ATKTRACE: guarding row " << matches[0].idx << ": "
          << matches.size() << " instance(s) for 30 s - land hits now";
        for (std::size_t k = 0; k < matches.size(); ++k) {
            auto* p = reinterpret_cast<const std::uint8_t*>(matches[k].addr);
            const float mv = *reinterpret_cast<const float*>(p + 0x20);
            m << " [i" << k << " @0x" << std::hex << matches[k].addr << std::dec
              << " mv=" << mv << "]";
        }
        Log(m.str());
    }
    // Self-test: these two reads must each produce one ATKACC line right
    // below. No lines = the guard mechanism itself is broken, not the game.
    {
        std::uint32_t mv = 0;
        std::uint32_t lag = 0;
        const bool ok = AtkSelftestRead(matches[0].addr, &mv, &lag);
        std::ostringstream m;
        m << "ATKTRACE: selftest " << (ok ? "ok" : "FAULTED")
          << " mv=0x" << std::hex << mv << " hitlag_a=0x" << lag
          << " (expect 2 ATKACC lines above)";
        Log(m.str());
    }
    g_atkScanRunning.store(false, std::memory_order_relaxed);
}

void AtkScanArm() {
    if (g_atkScanRunning.exchange(true, std::memory_order_relaxed)) {
        Log("ATKSCAN: previous scan still running - wait for it to finish");
        return;
    }
    if (g_atkTraceDeadline.load(std::memory_order_relaxed) != 0 ||
        g_atkRowTarget.load(std::memory_order_relaxed) != 0) {
        g_atkScanRunning.store(false, std::memory_order_relaxed);
        Log("ATKTRACE: session still live - wait for 'session over' first");
        return;
    }
    std::ifstream input("nativePC/plugins/ValorProbe.atktrace.txt");
    int idx = 0;
    if (input >> idx) {
        g_atkRowCfg = idx;
    }
    g_atkRowTarget.store(0, std::memory_order_relaxed);
    g_atkTraceDeadline.store(0, std::memory_order_relaxed);
    AtkRemoveGuard();
    std::ostringstream m;
    m << "ATKSCAN: arming for row " << g_atkRowCfg
      << " (config ValorProbe.atktrace.txt)";
    Log(m.str());
    std::thread(AtkScanThread, g_atkRowCfg).detach();
}

void PollAtkTrace() {
    const auto target = g_atkRowTarget.load(std::memory_order_relaxed);
    if (!target) {
        return;
    }
    const long long deadline = g_atkTraceDeadline.load(std::memory_order_relaxed);
    if (deadline == 0) {
        return;  // scan still running
    }
    const long long now =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    if (now < deadline && g_atkTraceLines.load(std::memory_order_relaxed) < 1500) {
        return;
    }
    g_atkRowTarget.store(0, std::memory_order_relaxed);
    AtkRemoveGuard();
    std::ostringstream m;
    m << "ATKTRACE: session over, " << g_atkTraceLines.load()
      << " accesses logged (cap 1500, neighbor faults "
      << g_atkNeighborFaults.load() << ")";
    Log(m.str());
    g_atkTraceDeadline.store(0, std::memory_order_relaxed);
}

// --- F10 struct diff capture ----------------------------------------------
// Two-phase capture for locating defense-timer fields empirically:
//   baseline (5 s idle) learns which offsets move without input;
//   armed (F10 again / 30 s) logs high-signal transitions on non-noisy offsets:
//   Z = dword crossed zero (timer armed/expired), B = single-bit flag flip.
enum class DiffPhase { Idle, Baseline, Armed };

struct DiffChangeStats {
    std::uint32_t changes = 0;
    std::uint32_t zeroCrosses = 0;
    std::uint32_t bitToggles = 0;
    std::uint32_t lastOld = 0;
    std::uint32_t lastNew = 0;
};

struct DiffRegion {
    const char* name = "";
    std::uint8_t* base = nullptr;
    std::size_t size = 0;
    std::vector<std::uint8_t> prev;
    bool initialized = false;
    std::unordered_map<std::uint32_t, DiffChangeStats> baseline;
    std::unordered_set<std::uint32_t> noisy;
    std::unordered_map<std::uint32_t, DiffChangeStats> stats;
};

DiffPhase g_diffPhase = DiffPhase::Idle;
DiffRegion g_diffWeapon;
DiffRegion g_diffPlayer;
DiffRegion g_diffPool;
std::chrono::steady_clock::time_point g_diffDeadline{};
std::uint32_t g_diffFrame = 0;
std::uint32_t g_diffLineBudget = 0;
bool g_f10WasDown = false;

std::string FormatHex32(std::uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase << value;
    return out.str();
}

std::string FormatFloat32(std::uint32_t raw) {
    float value = 0.0f;
    std::memcpy(&value, &raw, sizeof(value));
    std::ostringstream out;
    out << value;
    return out.str();
}

void DiffScanRegion(DiffRegion& region, bool armed) {
    if (!region.base || region.size == 0 || !IsReadable(region.base, region.size)) {
        return;
    }
    if (!region.initialized) {
        region.prev.assign(region.base, region.base + region.size);
        region.initialized = true;
        return;
    }

    std::optional<ActionInfo> action;
    {
        std::scoped_lock lock(g_actionMutex);
        action = g_lastLongSwordAction;
    }

    auto* previous = reinterpret_cast<std::uint32_t*>(region.prev.data());
    const auto* current = reinterpret_cast<const std::uint32_t*>(region.base);
    const auto dwordCount = static_cast<std::uint32_t>(region.size / sizeof(std::uint32_t));
    auto& active = armed ? region.stats : region.baseline;
    for (std::uint32_t i = 0; i < dwordCount; ++i) {
        const auto oldValue = previous[i];
        const auto newValue = current[i];
        if (oldValue == newValue) {
            continue;
        }
        previous[i] = newValue;
        const auto offset = static_cast<std::uint32_t>(i * sizeof(std::uint32_t));
        auto& entry = active[offset];
        entry.changes += 1;
        entry.lastOld = oldValue;
        entry.lastNew = newValue;
        const bool zeroCross = oldValue == 0 || newValue == 0;
        const bool bitToggle = std::popcount(oldValue ^ newValue) == 1;
        if (zeroCross) {
            entry.zeroCrosses += 1;
        }
        if (bitToggle) {
            entry.bitToggles += 1;
        }
        if (armed && (zeroCross || bitToggle) && g_diffLineBudget > 0) {
            --g_diffLineBudget;
            std::ostringstream line;
            line << "DIFF[" << region.name << "] +0x" << std::hex << std::uppercase << offset
                 << (region.noisy.find(offset) != region.noisy.end() ? " n: "
                        : zeroCross ? " Z: " : " B: ")
                 << FormatHex32(oldValue) << "(" << FormatFloat32(oldValue) << ") -> "
                 << FormatHex32(newValue) << "(" << FormatFloat32(newValue) << ") @f"
                 << std::dec << g_diffFrame << " act=";
            if (action) {
                line << action->actionSet << ':' << action->actionId;
            } else {
                line << "--";
            }
            Log(line.str());
        }
    }
}

// The damage layer consults a per-player defense-pool object accessed as
// ([player+0x7D20] + 0x28) (see FUN_140aee690 call sites); its +8 holds the
// player pointer, timers at +0x84..0xC4, selector bools at +0x13D..0x177.
std::uint8_t* FindDefensePool(std::uint8_t* player) {
    auto* slot = player + 0x7D20;
    if (!IsReadable(slot, sizeof(void*))) {
        return nullptr;
    }
    auto* owner = *reinterpret_cast<std::uint8_t**>(slot);
    if (!owner || !IsReadable(owner, 0x10)) {
        return nullptr;
    }
    if (*reinterpret_cast<void* const*>(owner + 8) != player) {
        Log("DIFF: pool owner backlink mismatch; pool watch disabled");
        return nullptr;
    }
    auto* pool = owner + 0x28;
    if (!IsReadable(pool, 0x180)) {
        return nullptr;
    }
    return pool;
}

void StartDiffCapture() {
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
        Log("DIFF: player controller not locked yet; perform a Long Sword action first");
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    auto* weaponSlot = player + 0x76B0;
    if (!IsReadable(weaponSlot, sizeof(void*))) {
        Log("DIFF: weapon slot unreadable");
        return;
    }
    auto* weapon = *reinterpret_cast<std::uint8_t**>(weaponSlot);
    if (!weapon || !IsReadable(weapon, 0x2400)) {
        Log("DIFF: weapon struct unreadable");
        return;
    }

    g_diffWeapon = DiffRegion{};
    g_diffWeapon.name = "weapon";
    g_diffWeapon.base = weapon;
    g_diffWeapon.size = 0x2400;
    g_diffPlayer = DiffRegion{};
    g_diffPlayer.name = "player";
    g_diffPlayer.base = player;
    g_diffPlayer.size = 0x12608;
    if (!IsReadable(g_diffPlayer.base, g_diffPlayer.size)) {
        Log("DIFF: player struct tail unreadable; watching weapon only");
        g_diffPlayer.base = nullptr;
        g_diffPlayer.size = 0;
    }
    g_diffPool = DiffRegion{};
    g_diffPool.name = "pool";
    if (g_lastPlayerPool.load() != 0) {
        g_diffPool.base = reinterpret_cast<std::uint8_t*>(g_lastPlayerPool.load());
        g_diffPool.size = 0x180;
    } else if (g_diffPlayer.base) {
        auto* pool = FindDefensePool(player);
        if (pool) {
            g_diffPool.base = pool;
            g_diffPool.size = 0x180;
        }
    }
    if (!IsReadable(g_diffPool.base, g_diffPool.size)) {
        Log("DIFF: defense pool not located; watching weapon+player only");
        g_diffPool.base = nullptr;
        g_diffPool.size = 0;
    }

    g_diffPhase = DiffPhase::Baseline;
    g_diffDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    std::ostringstream message;
    message << "DIFF baseline started: weapon=0x" << std::hex << std::uppercase
            << reinterpret_cast<std::uintptr_t>(weapon) << " player=0x"
            << reinterpret_cast<std::uintptr_t>(player)
            << " pool=0x" << reinterpret_cast<std::uintptr_t>(g_diffPool.base)
            << " - stand still for 5 seconds";
    Log(message.str());
}

void PromoteDiffToArmed() {
    for (DiffRegion* region : {&g_diffWeapon, &g_diffPlayer, &g_diffPool}) {
        region->noisy.clear();
        for (const auto& [offset, entry] : region->baseline) {
            if (entry.changes > 0) {
                region->noisy.insert(offset);
            }
        }
        // Keep baseline stats: the report compares armed behavior against baseline behavior.
        region->stats.clear();
    }
    g_diffLineBudget = 20000;
    g_diffPhase = DiffPhase::Armed;
    g_diffDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    std::ostringstream message;
    message << "DIFF armed: perform the move now (F10 to finish, 30 s timeout)"
            << " noiseMask weapon=" << g_diffWeapon.noisy.size()
            << " player=" << g_diffPlayer.noisy.size();
    Log(message.str());
}

void FinishDiffCapture() {
    for (DiffRegion* region : {&g_diffWeapon, &g_diffPlayer, &g_diffPool}) {
        std::vector<std::pair<std::uint32_t, const DiffChangeStats*>> candidates;
        std::uint32_t totalChanges = 0;
        for (const auto& [offset, entry] : region->stats) {
            totalChanges += entry.changes;
            if (entry.zeroCrosses > 0 || entry.bitToggles > 0) {
                candidates.emplace_back(offset, &entry);
            }
        }
        std::sort(candidates.begin(), candidates.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

        std::ostringstream header;
        header << "DIFF REPORT [" << region->name << "] trackedOffsets=" << region->stats.size()
               << " totalChanges=" << totalChanges
               << " signalOffsets=" << candidates.size();
        if (g_diffLineBudget == 0) {
            header << " (line budget exhausted; live lines truncated)";
        }
        Log(header.str());

        constexpr std::uint32_t maxReportedOffsets = 400;
        std::uint32_t reported = 0;
        for (const auto& [offset, entry] : candidates) {
            if (reported >= maxReportedOffsets) {
                std::ostringstream trunc;
                trunc << "DIFF REPORT [" << region->name << "] ... "
                      << (candidates.size() - reported) << " more offsets truncated";
                Log(trunc.str());
                break;
            }
            ++reported;
            const bool noisy = region->noisy.find(offset) != region->noisy.end();
            const auto baselineIt = region->baseline.find(offset);
            const auto baselineChanges =
                baselineIt == region->baseline.end() ? 0u : baselineIt->second.changes;
            const auto baselineZeroCrosses =
                baselineIt == region->baseline.end() ? 0u : baselineIt->second.zeroCrosses;
            std::ostringstream line;
            line << "DIFF REPORT [" << region->name << "] +0x" << std::hex << std::uppercase
                 << offset << std::dec << " z=" << entry->zeroCrosses
                 << " b=" << entry->bitToggles << " ch=" << entry->changes
                 << " blCh=" << baselineChanges << " blZ=" << baselineZeroCrosses
                 << " last=" << FormatHex32(entry->lastOld) << "->" << FormatHex32(entry->lastNew)
                 << " (" << FormatFloat32(entry->lastOld) << "->" << FormatFloat32(entry->lastNew)
                 << ")" << (noisy ? " noisy" : "");
            Log(line.str());
        }
        region->stats.clear();
        region->baseline.clear();
        region->noisy.clear();
        region->initialized = false;
        region->prev.clear();
    }
    g_diffPhase = DiffPhase::Idle;
    Log("DIFF capture finished; press F10 for a new session");
}

bool DiffTargetsValid() {
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
        return false;
    }
    const auto* player = static_cast<const std::uint8_t*>(controller) - 0x61C8;
    const auto* weaponSlot = player + 0x76B0;
    if (!IsReadable(weaponSlot, sizeof(void*))) {
        return false;
    }
    const auto* weapon = *reinterpret_cast<std::uint8_t* const*>(weaponSlot);
    return weapon == g_diffWeapon.base && IsReadable(g_diffWeapon.base, g_diffWeapon.size);
}

void PollDiffCapture() {
    ++g_diffFrame;
    const bool f9Down = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
    const bool f9Pressed = f9Down && !g_f10WasDown;
    g_f10WasDown = f9Down;

    const auto now = std::chrono::steady_clock::now();
    switch (g_diffPhase) {
    case DiffPhase::Idle:
        if (f9Pressed) {
            StartDiffCapture();
        }
        return;
    case DiffPhase::Baseline:
        if (!DiffTargetsValid()) {
            Log("DIFF aborted: weapon pointer changed or became unreadable");
            g_diffPhase = DiffPhase::Idle;
            return;
        }
        DiffScanRegion(g_diffWeapon, false);
        DiffScanRegion(g_diffPlayer, false);
        DiffScanRegion(g_diffPool, false);
        if (f9Pressed || now >= g_diffDeadline) {
            PromoteDiffToArmed();
        }
        return;
    case DiffPhase::Armed:
        if (!DiffTargetsValid()) {
            Log("DIFF aborted: weapon pointer changed or became unreadable");
            g_diffPhase = DiffPhase::Idle;
            return;
        }
        DiffScanRegion(g_diffWeapon, true);
        DiffScanRegion(g_diffPlayer, true);
        DiffScanRegion(g_diffPool, true);
        if (f9Pressed || now >= g_diffDeadline) {
            FinishDiffCapture();
        }
        return;
    }
}

std::string DiffStatusText() {
    switch (g_diffPhase) {
    case DiffPhase::Idle:
        return "";
    case DiffPhase::Baseline:
        return "DIFF baseline: stand still";
    case DiffPhase::Armed:
        return "DIFF ARMED: do the move (F10 to finish)";
    }
    return "";
}

#endif // VALOR_PROBE
