"""Fix miner ranking denominators + always-on recording (marker-based)."""
import io

p = r"E:\MHW-arts\ValorGauge\src\dllmain.cpp"
t = io.open(p, encoding="utf-8").read()

# 1. Replace the analysis block between markers
start_marker = "    // Correlation-ranked report:"
end_marker = '    Log("MINER ANALYSIS END");'
si = t.index(start_marker)
ei = t.index(end_marker, si) + len(end_marker)

new_analysis = """    // Correlation-ranked report: keys whose change rate across ALL phase-2
    // runs is higher than across ALL phase-1 runs, strongest first.
    struct KeyInfo {
        int changed[3] = {};
        int minFrame[3] = {};
    };
    int phaseTotal[3] = {};
    for (const auto& run : g_minerRuns) {
        if (run.phase >= 1 && run.phase <= 2) {
            ++phaseTotal[run.phase];
        }
    }
    std::map<std::uint32_t, KeyInfo> kinfo;
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
            auto& ki = kinfo[key];
            ++ki.changed[run.phase];
            if (ki.minFrame[run.phase] == 0 || frame < ki.minFrame[run.phase]) {
                ki.minFrame[run.phase] = frame;
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
    Log("MINER ANALYSIS END");"""

t = t[:si] + new_analysis + t[ei:]

# 2. Replace MinerHandleF9 body (marker-based)
fs = t.index("void MinerHandleF9() {")
fe = t.index("}", t.index("MinerDumpAnalysis();", fs)) + 1
new_fn = """void MinerHandleF9() {
    // Recording is ALWAYS on once the controller is locked; F5 tags upcoming
    // runs with the next phase and dumps a ranked snapshot of everything so
    // far. Nothing to start, nothing to stop.
    ++g_minerPhase;
    MinerDumpAnalysis();
    std::ostringstream m;
    m << "MINER: now tagging runs as phase " << g_minerPhase
      << " (previous phases kept in the report)";
    Log(m.str());
}"""
t = t[:fs] + new_fn + t[fe:]

# 3. auto-start inside MinerCaptureFrame
old3 = """void MinerCaptureFrame() {
    if (!g_minerActive) {
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

    bool is86 = false;"""
new3 = """void MinerCaptureFrame() {
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
        return;
    }
    if (!g_minerActive) {
        g_minerActive = true;
        g_minerPhase = 1;
        Log("MINER auto-started: recording every 见切 as phase 1. "
            "F5 marks a new phase + dumps a snapshot.");
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;

    bool is86 = false;"""
assert old3 in t, "capture frame block not found"
t = t.replace(old3, new3, 1)

# 4. cap runs to last 16
old4 = """    if (!is86 && g_minerInRun) {
        MinerEndRun();
        return;
    }"""
new4 = """    if (!is86 && g_minerInRun) {
        MinerEndRun();
        while (g_minerRuns.size() > 16) {
            g_minerRuns.erase(g_minerRuns.begin());
        }
        return;
    }"""
assert old4 in t, "run end block not found"
t = t.replace(old4, new4, 1)

io.open(p, "w", encoding="utf-8", newline="\n").write(t)
print("ok")
