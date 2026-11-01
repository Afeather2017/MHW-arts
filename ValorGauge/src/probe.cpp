// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L49-L51, L60-L63, L69-L69, L368-L368, L530-L557, L2020-L2027, L2064-L2100, L2128-L2132, L3453-L3513, L6348-L6527, L6715-L6814, L6818-L6995, L7444-L7469. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

Aee050Fn g_originalAee050 = nullptr;
std::atomic_uint32_t g_poolLogCount = 0;
std::atomic<std::uintptr_t> g_lastPlayerPool{0};

std::atomic_bool g_stateLoggerEnabled = true;
std::atomic_uintptr_t g_stateAddress = 0;
std::optional<std::uint32_t> g_lastState;
std::chrono::steady_clock::time_point g_nextStatePoll{};

std::atomic_uint32_t g_actionDiagnosticCount = 0;

std::atomic_uint32_t g_clangProbeLogCount{0};

void LoadStateAddress() {
#ifdef VALOR_PROBE
    std::ifstream input("nativePC\\plugins\\ValorGauge.state-address.txt");
    std::string value;
    if (!input || !std::getline(input, value)) {
        g_stateAddress = 0;
        g_lastState.reset();
        Log("State logger waiting: create ValorGauge.state-address.txt with a hexadecimal address");
        return;
    }

    const auto parsed = ParseAddress(value);
    if (!parsed || *parsed == 0) {
        g_stateAddress = 0;
        g_lastState.reset();
        Log("State logger address is invalid; expected hexadecimal, for example 0x7FF612345678");
        return;
    }

    g_stateAddress = *parsed;
    g_lastState.reset();
    std::ostringstream message;
    message << "State logger watching 0x" << std::hex << std::uppercase << *parsed;
    Log(message.str());
#else
    (void)0; // state logger is probe-only
#endif
}

// Hook the damage-layer defense query (FUN_140aee050, RVA 0xAEE050). Every
// arbitration hands us the pool pointer actually in use; record the one that
// belongs to the local player so DIFF watches the real thing.
// --- 见切 hit-path diagnostics -----------------------------------------------
// During 见切 (3:86): log every pool query and every damage-calc entry so a
// whiff / clang / direct-eat comparison reveals the exact negation signal.
std::atomic_uint32_t g_fsdLogCount{0};
std::atomic_uint32_t g_rollDispatchLog{0};

#ifdef VALOR_PROBE
float __fastcall Aee050Hook(void* pool, int reactionType) {
    const float result = g_originalAee050 ? g_originalAee050(pool, reactionType) : 0.0f;
    auto* p = static_cast<std::uint8_t*>(pool);
    if (ActionIsForesightSlash() && g_fsdLogCount.load() < 96) {
        ++g_fsdLogCount;
        std::ostringstream m;
        m << "FSDQ type=" << std::dec << reactionType << " ret=" << result
          << std::hex << std::uppercase << " caller=0x"
          << reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - g_moduleBase.load()
          << " pool=0x" << reinterpret_cast<std::uintptr_t>(pool);
        Log(m.str());
    }
    if (p && IsReadable(p, 0x180) && g_poolLogCount.load() < 24) {
        void* playerPtr = nullptr;
        {
            std::scoped_lock lock(g_actionMutex);
            if (g_lockedPlayerController) {
                playerPtr = static_cast<std::uint8_t*>(g_lockedPlayerController) - 0x61C8;
            }
        }
        if (playerPtr && *reinterpret_cast<void* const*>(p + 8) == playerPtr) {
            g_lastPlayerPool = reinterpret_cast<std::uintptr_t>(pool);
            if (g_poolLogCount.fetch_add(1) < 24) {
                std::ostringstream m;
                m << "POOLQ pool=0x" << std::hex << std::uppercase
                  << reinterpret_cast<std::uintptr_t>(pool) << " type=" << std::dec
                  << reactionType << " ret=" << result << std::hex << " [94]=0x"
                  << *reinterpret_cast<const std::uint32_t*>(p + 0x94) << " [98]=0x"
                  << *reinterpret_cast<const std::uint32_t*>(p + 0x98) << " [9C]=0x"
                  << *reinterpret_cast<const std::uint32_t*>(p + 0x9C) << " [A0]=0x"
                  << *reinterpret_cast<const std::uint32_t*>(p + 0xA0);
                Log(m.str());
            }
        }
    }
    return result;
}


long long __fastcall DmgCalcHook(long long a1, long long a2, std::uint32_t* a3) {
    g_damageThisDispatch = true;
    return g_originalDmgCalc(a1, a2, a3);
}

HitDispatchFn g_originalHitDispatch = nullptr;
std::atomic_uint32_t g_clangCount{0};

std::int32_t* __fastcall HitDispatchHook(std::int64_t* a1, std::int64_t* a2) {
    g_damageThisDispatch = false;
    // The dispatcher resolves the interaction type at entry via vtable+8;
    // type 6 (== case 0) is the damage path. Mirror that read so a damage-type
    // interaction that never reached the damage calc = a negated hit.
    long long interactType = -1;
    if (a2 && IsReadable(a2, sizeof(void*))) {
        void* obj = *reinterpret_cast<void**>(a2);
        if (obj && IsReadable(obj, 0x10)) {
            void* vtbl = *reinterpret_cast<void**>(obj);
            if (IsReadable(reinterpret_cast<char*>(vtbl) + 8, sizeof(void*))) {
                                using TypeGetterFn = long long(__fastcall*)(std::int64_t*);
                auto fn = *reinterpret_cast<TypeGetterFn*>(
                    reinterpret_cast<char*>(vtbl) + 8);
                if (IsReadable(reinterpret_cast<const void*>(fn), 1)) {
                    interactType = fn(a2);
                }
                (void)0;
            }
        }
    }
    auto* result = g_originalHitDispatch(a1, a2);
    const auto code = reinterpret_cast<std::uintptr_t>(result);
    // Roll-mining: log EVERY damage-class dispatch during the roll. A type-6
    // dispatch with no damage = the hitbox overlapped the player but the
    // i-frame window ate it - the only place a successful through-roll could
    // be visible. Pure whiffs never reach the dispatcher as type 6.
    if (interactType == 6 && ActionIsRoll() && g_rollDispatchLog.load() < 400) {
        ++g_rollDispatchLog;
        std::ostringstream m;
        m << "ROLLDISPATCH type=6" << std::hex << std::uppercase << " code=0x"
          << code << " dmg=" << (g_damageThisDispatch ? 1 : 0);
        Log(m.str());
    }
    const bool attackRejected = interactType == 6 && code != 0xD &&
                                !g_damageThisDispatch;
    const bool rollRejected = attackRejected && ActionIsRoll();
    if ((attackRejected && ActionIsForesightSlash()) || rollRejected) {
        // Superseded trigger path (bit15/bit250 own the mod triggers now):
        // probe keeps observation only - latch + capped log.
        if (!g_clangLatch) {
            g_clangLatch = true;
        }
        if (g_fsdLogCount.load() < 4096) {
            ++g_fsdLogCount;
            const auto clangN = g_clangCount.fetch_add(1) + 1;
            if (clangN <= 16) {
                std::ostringstream m;
                m << "HITDISPATCH #" << std::dec << clangN << ": rejected during "
                  << (rollRejected ? "roll(3:18)" : "见切")
                  << std::hex << std::uppercase << " (type=" << interactType
                  << " code=0x" << code << ")";
                Log(m.str());
            }
        }
    }
    return result;
}

// --- RXN watcher: reaction manager + configurable FSM property polling -------
// mgr = *(player+0x7D20); mgr+0x7C0 arms at move start (NOT a clang detector).
// Property chain (decompilation-verified): component = *(player+0x126D8);
// key = *(int32*)(component+0x84+id*24); registry = base+0x5012048 (count
// +0xB8, rows *(+0xC8)[key]); counters = *(int16*)(row+0x24+index*2).
// Watched ids come from nativePC\plugins\ValorGauge.prop-watch.txt (default
// "222 631"): 222 = foresight block (idx1 family flag / idx2 success counter),
// 631 = 見切り回避成功後の攻撃ヒット (success + follow-up hit latch).
std::uint64_t g_rxnFrame = 0;
std::uint32_t g_rxnStandby = 0;
std::uint8_t g_rxnGate = 0;
float g_rxnNohit = 0.0f;
bool g_rxnValid = false;
std::uint32_t g_rxnRowMissLog = 0;
std::uint8_t* g_rxnDiagPlayer = nullptr;
std::uint64_t g_rxnDiagNextFrame = 0;
std::uint32_t g_watchIds[kMaxWatchIds] = {222, 631};
int g_watchCount = 2;
std::int16_t g_watchPrev[kMaxWatchIds][3] = {};
std::int16_t g_watchCur[kMaxWatchIds][3] = {};
std::uint32_t g_watchKeys[kMaxWatchIds] = {};
std::int16_t* g_watchCounters[kMaxWatchIds] = {};
// Prop trigger: when this watched id/index INCREASES during 见切, open the
// counter window (the vanilla success counter 222 idx2 = a hit was negated).
// Config nativePC\plugins\ValorGauge.prop-trigger.txt: "<id> <index>",
// id 0 disables.
std::uint32_t g_triggerId = 222;
std::uint32_t g_triggerIndex = 2;

std::int16_t* ResolvePropCounters(std::uint8_t* player, std::uint32_t id,
                                  std::uint32_t* keyOut) {
    if (!player || g_moduleBase.load() == 0) {
        return nullptr;
    }
    auto* componentSlot = player + 0x126D8;
    if (!IsReadable(componentSlot, sizeof(void*))) {
        return nullptr;
    }
    auto* component = *reinterpret_cast<std::uint8_t**>(componentSlot);
    if (!component) {
        return nullptr;
    }
    auto* keyAddress = component + 0x84 + static_cast<std::size_t>(id) * 24;
    if (!IsReadable(keyAddress, sizeof(std::int32_t))) {
        return nullptr;
    }
    const auto key = *reinterpret_cast<const std::int32_t*>(keyAddress);
    if (keyOut) {
        *keyOut = static_cast<std::uint32_t>(key);
    }
    auto* registry = reinterpret_cast<std::uint8_t*>(g_addrPropRegistry);
    if (!IsReadable(registry, 0xD0)) {
        return nullptr;
    }
    const auto count = *reinterpret_cast<const std::int32_t*>(registry + 0xB8);
    if (key < 0 || key >= count) {
        return nullptr;
    }
    auto** rowTable = *reinterpret_cast<std::uint8_t***>(registry + 0xC8);
    if (!IsReadable(rowTable, (static_cast<std::size_t>(key) + 1) * sizeof(void*))) {
        return nullptr;
    }
    auto* row = rowTable[key];
    if (!row || !IsReadable(row, 0x30)) {
        return nullptr;
    }
    return reinterpret_cast<std::int16_t*>(row + 0x24);
}

// Full property-key table dump (ids 0..799): run once when the controller
// locks. Diffing the dump before vs after adding an XFsm condition reveals
// which property id that condition bound (its key goes 0 -> nonzero).
void LogKeyTableDump(std::uint8_t* player) {
    auto* componentSlot = player + 0x126D8;
    if (!IsReadable(componentSlot, sizeof(void*))) {
        Log("KEYDUMP: component slot unreadable");
        return;
    }
    auto* comp = *reinterpret_cast<std::uint8_t**>(componentSlot);
    if (!comp || g_moduleBase.load() == 0) {
        Log("KEYDUMP: component unavailable");
        return;
    }
    auto* registry = reinterpret_cast<std::uint8_t*>(g_addrPropRegistry);
    if (!IsReadable(registry, 0xD0)) {
        Log("KEYDUMP: registry unreadable");
        return;
    }
    Log("KEYDUMP BEGIN (key table, ids 0..799; format id=key)");
    for (std::uint32_t base = 0; base < 800; base += 40) {
        std::ostringstream m;
        m << "KEYDUMP";
        for (std::uint32_t id = base; id < base + 40 && id < 800; ++id) {
            auto* keyAddress = comp + 0x84 + static_cast<std::size_t>(id) * 24;
            int key = -1;
            if (IsReadable(keyAddress, sizeof(std::int32_t))) {
                key = *reinterpret_cast<const std::int32_t*>(keyAddress);
            }
            m << ' ' << id << '=' << key;
        }
        Log(m.str());
    }
    Log("KEYDUMP END");
}

// One line describing every link of the manager/property chain. Fired when a
// watched row stays unresolved (including key==0 = property unbound).
void LogChainDiagnostics(std::uint8_t* player, std::uint32_t id) {
    std::ostringstream m;
    m << std::hex << std::uppercase << "RXN DIAG id=" << std::dec << id << " player=0x"
      << reinterpret_cast<std::uintptr_t>(player);
    auto* mgrSlot = player + 0x7D20;
    if (!IsReadable(mgrSlot, sizeof(void*))) {
        m << " mgrSlot=unreadable";
    } else {
        auto* mgr = *reinterpret_cast<std::uint8_t**>(mgrSlot);
        m << " mgr=0x" << reinterpret_cast<std::uintptr_t>(mgr);
        if (mgr && IsReadable(mgr, 0x10)) {
            m << " backlink="
              << (*reinterpret_cast<void* const*>(mgr + 8) == player ? "ok" : "MISMATCH");
            m << " 7C0raw=0x" << *reinterpret_cast<const std::uint32_t*>(mgr + 0x7C0);
        }
    }
    auto* compSlot = player + 0x126D8;
    if (!IsReadable(compSlot, sizeof(void*))) {
        m << " compSlot=unreadable";
        Log(m.str());
        return;
    }
    auto* comp = *reinterpret_cast<std::uint8_t**>(compSlot);
    m << " comp=0x" << reinterpret_cast<std::uintptr_t>(comp);
    if (!comp) {
        Log(m.str());
        return;
    }
    m << " keys[";
    for (int wi = 0; wi < g_watchCount; ++wi) {
        auto* keyAddress = comp + 0x84 + static_cast<std::size_t>(g_watchIds[wi]) * 24;
        if (!IsReadable(keyAddress, sizeof(std::int32_t))) {
            m << " ?";
            continue;
        }
        m << " " << g_watchIds[wi] << "="
          << std::dec << *reinterpret_cast<const std::int32_t*>(keyAddress) << std::hex;
    }
    m << " ]";
    auto* registry = reinterpret_cast<std::uint8_t*>(g_addrPropRegistry);
    if (!IsReadable(registry, 0xD0)) {
        m << " registry=unreadable base=0x" << std::hex << std::uppercase
          << g_moduleBase.load();
        Log(m.str());
        return;
    }
    const auto regCount = *reinterpret_cast<const std::int32_t*>(registry + 0xB8);
    auto* rowTable = *reinterpret_cast<std::uint8_t***>(registry + 0xC8);
    m << std::hex << std::uppercase << " reg=0x" << reinterpret_cast<std::uintptr_t>(registry)
      << " rawB8=0x" << *reinterpret_cast<const std::uint32_t*>(registry + 0xB8)
      << " rawBC=0x" << *reinterpret_cast<const std::uint32_t*>(registry + 0xBC)
      << " rowTable=0x" << reinterpret_cast<std::uintptr_t>(rowTable);
    for (int wi = 0; wi < g_watchCount; ++wi) {
        const auto key = *reinterpret_cast<const std::int32_t*>(
            comp + 0x84 + static_cast<std::size_t>(g_watchIds[wi]) * 24);
        m << std::dec << " id" << g_watchIds[wi] << "(key=" << key << "):";
        if (key < 0 || key >= regCount || !IsReadable(rowTable, (key + 1) * 8)) {
            m << "oob";
            continue;
        }
        auto* row = rowTable[key];
        m << std::hex << std::uppercase << "row=0x" << reinterpret_cast<std::uintptr_t>(row);
        if (row && IsReadable(row, 0x30)) {
            m << " vals=[" << std::dec << *reinterpret_cast<const std::int16_t*>(row + 0x24)
              << ',' << *reinterpret_cast<const std::int16_t*>(row + 0x26)
              << ',' << *reinterpret_cast<const std::int16_t*>(row + 0x28) << ']';
        } else {
            m << "unreadable";
        }
    }
    Log(m.str());
}

// POOLDIFF: per-frame reaction-pool snapshot diff. Hypothesis: the deflect
// ("blue i-frame") windows of 见切/居合 are reaction-pool timers (same family
// as the no-hit timer pool+0xE4 = query type 8); the arbitration picks
// guard/deflect/damage by which timer is running. Log every pool field that
// RISES above 0.5 (a defense window opening) - the offset that spikes on a
// 见切 deflect is the deflect timer.
std::uint8_t g_poolDiffPrev[0x180] = {};
std::uint8_t g_poolDiffPrevMgr[0x30] = {}; // mgr +0x760..0x790 (reaction slots)
bool g_poolDiffPrimed = false;
std::atomic_uint32_t g_poolDiffLogs{0};

void PollPoolDiff() {
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
        return;
    }
    auto* player = static_cast<std::uint8_t*>(controller) - 0x61C8;
    if (!IsReadable(player + 0x7D20, sizeof(void*))) {
        return;
    }
    auto* mgr = *reinterpret_cast<std::uint8_t**>(player + 0x7D20);
    if (!mgr || !IsReadable(mgr + 0x28, sizeof(void*))) {
        return;
    }
    auto* pool = *reinterpret_cast<std::uint8_t**>(mgr + 0x28);
    if (!pool || !IsReadable(pool, sizeof(g_poolDiffPrev))) {
        return;
    }
    if (!g_poolDiffPrimed) {
        std::memcpy(g_poolDiffPrev, pool, sizeof(g_poolDiffPrev));
        if (IsReadable(mgr + 0x760, sizeof(g_poolDiffPrevMgr))) {
            std::memcpy(g_poolDiffPrevMgr, mgr + 0x760, sizeof(g_poolDiffPrevMgr));
        }
        g_poolDiffPrimed = true;
        return;
    }
    auto logRise = [&](const char* tag, std::size_t off, float oldVal, float newVal) {
        if (g_poolDiffLogs.load() >= 600) {
            return;
        }
        ++g_poolDiffLogs;
        std::ostringstream m;
        m << "POOLDIFF " << tag << " +0x" << std::hex << off << std::dec
          << ": " << oldVal << " -> " << newVal;
        std::uint32_t newRaw;
        std::memcpy(&newRaw, pool + off, 4);
        m << " (raw 0x" << std::hex << newRaw << ")" << std::dec;
        {
            std::scoped_lock lock(g_actionMutex);
            if (g_lastLongSwordAction) {
                m << " act=" << g_lastLongSwordAction->actionSet << ':'
                  << g_lastLongSwordAction->actionId;
            }
        }
        Log(m.str());
    };
    for (std::size_t off = 0; off + 4 <= sizeof(g_poolDiffPrev); off += 4) {
        float newVal;
        std::memcpy(&newVal, pool + off, 4);
        float oldVal;
        std::memcpy(&oldVal, g_poolDiffPrev + off, 4);
        // Defense windows on LS are short (0.2-0.5 s) - a 0.5 threshold
        // missed them entirely in the first mining round. Any rise past
        // 0.05 from at-or-below it, or a jump > 0.1, is a window event.
        if (newVal > 0.05f && oldVal <= 0.05f) {
            logRise("pool", off, oldVal, newVal);
        } else if (newVal - oldVal > 0.1f && newVal > 0.1f) {
            logRise("pooljump", off, oldVal, newVal);
        }
    }
    std::memcpy(g_poolDiffPrev, pool, sizeof(g_poolDiffPrev));
    // MGR classification region (+0x760-0x790): the guard/deflect reaction
    // slots (+0x769 u16 = 0x100/0x101 lives here) - raw-change detection.
    if (IsReadable(mgr + 0x760, sizeof(g_poolDiffPrevMgr))) {
        for (std::size_t off = 0; off + 4 <= sizeof(g_poolDiffPrevMgr); off += 4) {
            std::uint32_t newRaw, oldRaw;
            std::memcpy(&newRaw, mgr + 0x760 + off, 4);
            std::memcpy(&oldRaw, g_poolDiffPrevMgr + off, 4);
            if (newRaw != oldRaw && g_poolDiffLogs.load() < 600) {
                ++g_poolDiffLogs;
                std::ostringstream m;
                m << "POOLDIFF mgr +0x" << std::hex << (0x760 + off) << ": 0x"
                  << oldRaw << " -> 0x" << newRaw << std::dec;
                {
                    std::scoped_lock lock(g_actionMutex);
                    if (g_lastLongSwordAction) {
                        m << " act=" << g_lastLongSwordAction->actionSet << ':'
                          << g_lastLongSwordAction->actionId;
                    }
                }
                Log(m.str());
            }
        }
        std::memcpy(g_poolDiffPrevMgr, mgr + 0x760, sizeof(g_poolDiffPrevMgr));
    }
}

void PollReactionWatch() {
    ++g_rxnFrame;
    void* controller = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        controller = g_lockedPlayerController;
    }
    if (!controller) {
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
    std::uint8_t gate7626 = 0;
    if (IsReadable(player + 0x7626, 1)) {
        gate7626 = player[0x7626];
    }

    // Poll every watched property id.
    for (int wi = 0; wi < g_watchCount; ++wi) {
        auto* counters = ResolvePropCounters(player, g_watchIds[wi], &g_watchKeys[wi]);
        if (g_watchKeys[wi] == 0) {
            counters = nullptr; // key==0: property unbound (no FSM reference)
        }
        g_watchCounters[wi] = counters;
        for (int i = 0; i < 3; ++i) {
            g_watchCur[wi][i] = counters ? counters[i] : 0;
        }
    }

    // Mined-candidate probes: active only during special evasions. These
    // offsets flipped in clang runs but never in whiff runs (F5 miner, 9 runs).
    static std::uint32_t lpW2398 = 0, lpW23A0 = 0, lpB220 = 0, lpB224 = 0,
                         lpB228 = 0, lpB240 = 0, lpB24C = 0, lpB260 = 0,
                         lpB264 = 0, lpB268 = 0, lp8954 = 0, lp8984 = 0,
                         lp89B4 = 0, lp89E4 = 0;
    bool inEvasionNow = false;
    {
        std::scoped_lock lock(g_actionMutex);
        inEvasionNow = g_lastLongSwordAction &&
                       g_lastLongSwordAction->actionSet == 3 &&
                       (g_lastLongSwordAction->actionId == 86 ||
                        g_lastLongSwordAction->actionId == 101 ||
                        g_lastLongSwordAction->actionId == 102 ||
                        g_lastLongSwordAction->actionId == 18);
    }
    if (inEvasionNow && g_clangProbeLogCount.load() < 600) {
        auto* weaponSlot = player + 0x76B0;
        std::uint8_t* weapon = nullptr;
        if (IsReadable(weaponSlot, sizeof(void*))) {
            weapon = *reinterpret_cast<std::uint8_t**>(weaponSlot);
        }
        struct MProbe {
            std::uint8_t* addr;
            std::uint32_t* last;
            const char* tag;
        };
        MProbe mprobes[] = {
            {weapon ? weapon + 0x2398 : nullptr, &lpW2398, "w2398"},
            {weapon ? weapon + 0x23A0 : nullptr, &lpW23A0, "w23A0"},
            {player + 0xB220, &lpB220, "pB220"},
            {player + 0xB224, &lpB224, "pB224"},
            {player + 0xB228, &lpB228, "pB228"},
            {player + 0xB240, &lpB240, "pB240"},
            {player + 0xB24C, &lpB24C, "pB24C"},
            {player + 0xB260, &lpB260, "pB260"},
            {player + 0xB264, &lpB264, "pB264"},
            {player + 0xB268, &lpB268, "pB268"},
            {player + 0x8954, &lp8954, "ev8954"},
            {player + 0x8984, &lp8984, "ev8984"},
            {player + 0x89B4, &lp89B4, "ev89B4"},
            {player + 0x89E4, &lp89E4, "ev89E4"},
        };
        std::ostringstream changed;
        for (auto& mp : mprobes) {
            if (!mp.addr || !IsReadable(mp.addr, sizeof(std::uint32_t))) {
                continue;
            }
            const auto v = *reinterpret_cast<const std::uint32_t*>(mp.addr);
            if (v != *mp.last) {
                changed << ' ' << mp.tag << '=' << v;
                *mp.last = v;
            }
        }
        if (!changed.str().empty()) {
            ++g_clangProbeLogCount;
            Log("EPROBE" + changed.str());
        }
    }
    // (iai bit250 / deflect-flash triggers live in the mod build's
    //  PollModTriggers; the probe only observes the raw values above.)

    if (!g_rxnValid) {
        g_rxnStandby = standby;
        g_rxnNohit = nohit;
        g_rxnGate = gate7626;
        for (int wi = 0; wi < g_watchCount; ++wi) {
            for (int i = 0; i < 3; ++i) {
                g_watchPrev[wi][i] = g_watchCur[wi][i];
            }
        }
        g_rxnValid = true;
        for (int wi = 0; wi < g_watchCount; ++wi) {
            if (g_watchCounters[wi]) {
                std::ostringstream m;
                m << "RXN: prop " << g_watchIds[wi] << " resolved key="
                  << g_watchKeys[wi] << " counters=0x" << std::hex << std::uppercase
                  << reinterpret_cast<std::uintptr_t>(g_watchCounters[wi]) << std::dec
                  << " initial=[" << g_watchCur[wi][0] << ',' << g_watchCur[wi][1]
                  << ',' << g_watchCur[wi][2] << ']';
                Log(m.str());
            } else if (wi == 0 && g_rxnRowMissLog < 3) {
                ++g_rxnRowMissLog;
                LogChainDiagnostics(player, g_watchIds[wi]);
            }
        }
        return;
    }

    const bool windowEdge = (nohit == 0.0f) != (g_rxnNohit == 0.0f);
    bool propsChanged = false;
    for (int wi = 0; wi < g_watchCount && !propsChanged; ++wi) {
        for (int i = 0; i < 3; ++i) {
            propsChanged = propsChanged || g_watchCur[wi][i] != g_watchPrev[wi][i];
        }
    }
    const bool changed = standby != g_rxnStandby || windowEdge ||
                         gate7626 != g_rxnGate || propsChanged;
    if (!changed) {
        g_rxnNohit = nohit;
        return;
    }

    ActionInfo action{};
    bool haveAction = false;
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lastLongSwordAction) {
            action = *g_lastLongSwordAction;
            haveAction = true;
        }
    }
    std::ostringstream line;
    line << "RXN f=" << std::dec << g_rxnFrame << " act=";
    if (haveAction) {
        line << action.actionSet << ':' << action.actionId;
    } else {
        line << "--";
    }
    line << " 7C0=" << standby << " gate=" << static_cast<int>(gate7626) << std::fixed
         << std::setprecision(2) << " nohit=" << nohit;
    for (int wi = 0; wi < g_watchCount; ++wi) {
        line << " p" << g_watchIds[wi] << "=[" << g_watchCur[wi][0] << ','
             << g_watchCur[wi][1] << ',' << g_watchCur[wi][2] << ']';
    }
    Log(line.str());

    g_rxnStandby = standby;
    g_rxnNohit = nohit;
    g_rxnGate = gate7626;
    for (int wi = 0; wi < g_watchCount; ++wi) {
        for (int i = 0; i < 3; ++i) {
            g_watchPrev[wi][i] = g_watchCur[wi][i];
        }
    }
}

void PollStateLogger() {
    const auto now = std::chrono::steady_clock::now();
    if (!g_stateLoggerEnabled || now < g_nextStatePoll) {
        return;
    }
    g_nextStatePoll = now + std::chrono::milliseconds(16);

    const auto address = g_stateAddress.load();
    if (address == 0 || !IsReadable(reinterpret_cast<const void*>(address), sizeof(std::uint32_t))) {
        return;
    }

    const auto state = *reinterpret_cast<const volatile std::uint32_t*>(address);
    if (!g_lastState) {
        g_lastState = state;
        std::ostringstream message;
        message << "State initial: " << state << " (0x" << std::hex << std::uppercase << state << ')';
        Log(message.str());
    } else if (*g_lastState != state) {
        std::ostringstream message;
        message << "State changed: " << *g_lastState << " -> " << state
                << " (0x" << std::hex << std::uppercase << *g_lastState << " -> 0x" << state << ')';
        Log(message.str());
        g_lastState = state;
    }
}

#endif // VALOR_PROBE
