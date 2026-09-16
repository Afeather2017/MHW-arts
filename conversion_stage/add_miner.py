"""Add the F9 clang miner to ValorGauge dllmain.cpp."""
import io

path = r"E:\MHW-arts\ValorGauge\src\dllmain.cpp"
t = io.open(path, encoding="utf-8").read()

anchor = "bool InstallActionHook() {"

block = r'''// --- F9 clang miner: paired-scenario state mining -----------------------------
// F9 #1: start mining (phase 1). Each 见切 (3:86 entry) records one run of
// per-frame memory deltas over weapon/mgr/pool/FSM regions plus HP.
// F9 #2: advance to phase 2 (do the other scenario, e.g. real clangs).
// F9 #3: stop and dump the analysis: offsets whose change pattern is
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
bool g_f9WasDown = false;
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

void MinerCaptureFrame() {
    if (!g_minerActive || !g_minerInRun) {
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
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    ++g_minerFrameInRun;
    g_minerCur.frames = g_minerFrameInRun;
    g_minerCur.hpEnd = MinerReadHealth(player);

    for (std::size_t ri = 0; ri < g_minerRegions.size(); ++ri) {
        auto& r = g_minerRegions[ri];
        if (!IsReadable(r.base, r.size)) {
            continue;
        }
        const auto* cur = reinterpret_cast<const std::uint32_t*>(r.base);
        const auto* prev = reinterpret_cast<const std::uint32_t*>(r.prev.data());
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
    // End the run when the action leaves 见切.
    bool still86 = false;
    {
        std::scoped_lock lock(g_actionMutex);
        still86 = g_lastLongSwordAction && g_lastLongSwordAction->actionSet == 3 &&
                  g_lastLongSwordAction->actionId == 86;
    }
    if (!still86) {
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
    const char* regionNames[4] = {"weapon", "mgr", "pool", "playerFsm"};
    int reported = 0;
    for (const auto& [key, st] : stats) {
        const int region = static_cast<int>((key >> 24) & 3);
        const auto offset = key & 0x00FFFFFF;
        const bool p2only = st.changed[2] > 0 && st.changed[1] == 0;
        const bool p1only = st.changed[1] > 0 && st.changed[2] == 0;
        if (!p2only && !p1only) {
            continue;
        }
        if (reported++ >= 120) {
            break;
        }
        std::ostringstream m;
        m << "MINER CANDIDATE " << (p2only ? "PHASE2-ONLY " : "PHASE1-ONLY ")
          << regionNames[region] << "+0x" << std::hex << std::uppercase << offset
          << std::dec << " changed p1=" << st.changed[1] << " p2=" << st.changed[2]
          << " firstFrame p1=" << st.minFrame[1] << " p2=" << st.minFrame[2];
        Log(m.str());
    }
    Log("MINER ANALYSIS END");
}

void MinerHandleF9() {
    if (!g_minerActive) {
        void* controller = nullptr;
        {
            std::scoped_lock lock(g_actionMutex);
            controller = g_lockedPlayerController;
        }
        if (!controller) {
            Log("MINER: do a Long Sword action first");
            return;
        }
        g_minerActive = true;
        g_minerPhase = 1;
        g_minerRuns.clear();
        g_minerInRun = false;
        Log("MINER started: PHASE 1 (do scenario 1 见切s). F9 again = phase 2.");
        return;
    }
    if (g_minerPhase == 1) {
        g_minerPhase = 2;
        Log("MINER: PHASE 2 (do scenario 2 见切s). F9 again = stop + analyze.");
        return;
    }
    g_minerActive = false;
    g_minerInRun = false;
    Log("MINER stopped, analyzing...");
    MinerDumpAnalysis();
}

'''

# 1. splice the block before the force-action anchor
assert anchor in t and "MINER ANALYSIS END" not in t
t = t.replace(anchor, block + anchor, 1)

# 2. wire into DrawGauge
old = "    // Counter window: a spirit-key press while the window is open fires 刚1."
new = """    MinerCaptureFrame();
    const bool f9Down = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
    if (f9Down && !g_f9WasDown) {
        MinerHandleF9();
    }
    g_f9WasDown = f9Down;

    // Counter window: a spirit-key press while the window is open fires 刚1."""
assert old in t
t = t.replace(old, new, 1)

# 3. map include
if "#include <map>" not in t:
    t = t.replace("#include <optional>", "#include <map>\n#include <optional>", 1)

io.open(path, "w", encoding="utf-8", newline="\n").write(t)
print("miner added")
