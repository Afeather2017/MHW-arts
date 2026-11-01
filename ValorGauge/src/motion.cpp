// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L310-L325, L326-L329, L339-L342, L344-L366, L3605-L3611, L3620-L3621, L3623-L3740, L4028-L4036, L4057-L4075, L4077-L4297, L4438-L4531. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

// Frame seek: jump the action-instance frame counter (instance+8) to a chosen
// start frame right after a forced action begins. <0 = off; the 4th token of
// force-action.txt. FRAMETICK diagnostics arm on every accepted force so an
// ordinary F6 run confirms the field semantics before trusting the write.
std::atomic_bool g_frameWatchArmed{false};
std::atomic_bool g_seekArmed{false};
std::int32_t g_frameWatchSet = 3;
std::int32_t g_frameWatchId = 0;
std::int32_t g_frameWatchTicksLeft = 0;
std::int32_t g_frameTickLogsLeft = 0;
bool g_frameWatchSeen = false;
float g_frameLastLogged = -1.0f;
float g_frameLastDt = -1.0f;
bool g_seekFired = false;
std::int32_t g_seekWrites = 0;
float g_seekFrame = -1.0f;

// Clock miner v2: while a forced action runs, scan candidate regions for
// floats whose per-tick delta is a CONSTANT step (any magnitude >= 0.005).
// v1 only matched +-dt and missed nothing because the motion clock likely
// counts in TIML frames (+2.0/tick at the 30Hz logic rate) or lives deeper.

ClkRegion g_clkRegions[7];
std::int32_t g_clkTicks = 0;
bool g_clkReported = false;
float g_clkLastFrame = -1.0f;

void ClkResetRegions() {
    static const struct {
        const char* name;
        std::uint32_t size;
    } defs[] = {
        {"core", 0xCD0},        {"anim468", 0x400},  {"anim470", 0x200},
        {"weapon", 0x2450},     {"playerA", 0x500},  {"playerTail", 0x200},
        {"inst", 0x60},
    };
    for (int i = 0; i < 7; ++i) {
        auto& r = g_clkRegions[i];
        r.name = defs[i].name;
        r.size = defs[i].size;
        r.prev.assign(r.size / 4, 0.0f);
        r.step.assign(r.size / 4, 0.0f);
        r.valid.assign(r.size / 4, 0);
        r.hits.assign(r.size / 4, 0);
        r.lastBase = nullptr;
    }
    g_clkTicks = 0;
    g_clkReported = false;
    g_clkLastFrame = -1.0f;
}


// MOTIONCOND (mod): conditional motion swap for one action (valor design:
// qi-blade-3 action 68 -> LMT 142 with W held, 141 without, vanilla 108
// unless the spirit gauge is red). While the watched action is fresh
// (frame < 0.2 s) the instance motion id (+0x30) is rewritten to
// entry|0xC000 and the motion restarts via player vtable+0x288 (the same
// 3-arg call the native mid-motion swap helpers use).

std::vector<MotionBind> g_motionBinds;


#ifndef VALOR_PROBE
// SHEATHEMOTION: patch the 见切 (86) action-class factory motion id so the
// vanilla foresight flow plays the custom sheathe motion. The deflect/arm/
// FSM machinery is code-side (per-action-class) and stays 100% vanilla -
// only the skeleton changes. Class table: LS set-3 (RVA 0x2EF5C30, 113
// entries, stride 0x20, {u32 id, u32 set, void* classPtr, pad}); the class
// factories (class vtable+8/+0x10) embed the motion id immediate
// entry|0xC000 (86 -> 0xC08C, direct hit verified 2026-09-30).
// Config nativePC/plugins/ValorGauge.motion-swap.txt "<fromEntry> <toEntry>"
// (missing = off; e.g. "140 306").
void ApplySheatheMotionPatch() {
    std::ifstream input("nativePC/plugins/ValorGauge.motion-swap.txt");
    std::string token;
    unsigned long fromEntry = 0, toEntry = 0;
    bool patchMotion = false;
    if (!(input >> token)) {
        Log("Sheathe motion swap: no ValorGauge.motion-swap.txt - off");
        return;
    }
    if (token != "cond") {
        // factory-patch form: "<fromEntry> <toEntry>" first line; "bind ..."
        // lines (and cond-only files) skip the factory patch.
        fromEntry = std::strtoul(token.c_str(), nullptr, 10);
        if ((input >> toEntry) && fromEntry < 0x200 && toEntry < 0x200 &&
            fromEntry != toEntry) {
            patchMotion = true;
        } else {
            Log("Sheathe motion swap: bad first pair - factory patch off");
        }
        input >> token; // optional "cond" keyword
    }
    while (input >> token) {
        if (token != "bind") {
            continue;
        }
        unsigned long actionId = 0, entry = 0;
        if (input >> actionId >> entry && actionId < 0x200 && entry < 0x200) {
            MotionBind bind;
            bind.actionId = static_cast<std::uint32_t>(actionId);
            bind.entry = static_cast<std::uint32_t>(entry);
            g_motionBinds.push_back(bind);
        }
    }
    if (!patchMotion && g_motionBinds.empty()) {
        Log("Sheathe motion swap: nothing configured - off");
        return;
    }
    const auto base = g_moduleBase.load(std::memory_order_relaxed);
    int patchCount = 0;
    if (patchMotion) {
        const std::uint32_t fromMotion =
            (static_cast<std::uint32_t>(fromEntry) & 0x1FF) | 0xC000;
        const std::uint32_t toMotion =
            (static_cast<std::uint32_t>(toEntry) & 0x1FF) | 0xC000;
        const auto* table = reinterpret_cast<const std::uint8_t*>(base + 0x2EF5C30);
        if (!IsReadable(table, 113 * 0x20)) {
            Log("Sheathe motion swap: class table unreadable");
            return;
        }
        for (int i = 0; i < 113; ++i) {
            const auto* entry = table + i * 0x20;
            std::uint32_t id = 0;
            std::memcpy(&id, entry, 4);
            void* classPtr = nullptr;
            std::memcpy(&classPtr, entry + 8, sizeof(void*));
            if (!classPtr || !IsReadable(classPtr, sizeof(void*))) {
                continue;
            }
            void* vtbl = *reinterpret_cast<void**>(classPtr);
            if (!vtbl || !IsReadable(reinterpret_cast<const std::uint8_t*>(vtbl) + 0x18,
                                     sizeof(void*) * 2)) {
                continue;
            }
            for (int fi = 1; fi <= 2; ++fi) {
                void* factory = nullptr;
                std::memcpy(&factory,
                            reinterpret_cast<const std::uint8_t*>(vtbl) + fi * 8,
                            sizeof(void*));
                if (!factory || !IsReadable(factory, 0x140)) {
                    continue;
                }
                for (std::size_t off = 0; off + 4 <= 0x140; off += 4) {
                    std::uint32_t imm = 0;
                    std::memcpy(&imm,
                                reinterpret_cast<const std::uint8_t*>(factory) + off, 4);
                    if (imm != fromMotion) {
                        continue;
                    }
                    void* target = reinterpret_cast<std::uint8_t*>(factory) + off;
                    DWORD oldProtect = 0;
                    if (!VirtualProtect(target, 4, PAGE_EXECUTE_READWRITE,
                                        &oldProtect)) {
                        Log("Sheathe motion swap: VirtualProtect failed");
                        continue;
                    }
                    std::memcpy(target, &toMotion, 4);
                    DWORD restored = 0;
                    VirtualProtect(target, 4, oldProtect, &restored);
                    FlushInstructionCache(GetCurrentProcess(), target, 4);
                    ++patchCount;
                    std::ostringstream m;
                    m << "SHEATHEMOTION: class id=" << id << " factory" << fi
                      << " +0x" << std::hex << off << ": motion 0x" << fromMotion
                      << " -> 0x" << toMotion << std::dec;
                    Log(m.str());
                }
            }
        }
    }
    std::ostringstream m;
    m << "Sheathe motion swap: " << patchCount << " immediate(s) patched";
    if (patchMotion) {
        m << " (" << fromEntry << " -> " << toEntry << ")";
    }
    for (const auto& bind : g_motionBinds) {
        m << "; bind " << bind.actionId << "->" << bind.entry;
    }
    Log(m.str());
}

// --- Frame seek / frame watch ------------------------------------------------
// Action instances are preallocated per (set,id) under the action controller
// at player+0x61C8: instArray = *(ctrl + 0x68 + set*0x10), instance =
// *(instArray + id*8). instance+8 (float) is the ACTION FRAME: zeroed at
// begin (FUN_14026a1e0), advanced by the per-frame dt (instance+0xC) in the
// controller updaters (FUN_14026a820 / FUN_141f78600, "frame += dt"). Motion
// sampling, TIML event windows and the auto-end threshold (ctrl+0x6A8) all
// key off the controller-visible timeline this counter drives. Writing it
// right after begin = "play this action from frame N" (TIML events between 0

void ArmFrameSeekDiagnostics() {
    {
        std::scoped_lock lock(g_actionMutex);
        g_frameWatchSet = static_cast<std::int32_t>(g_forceSet);
        g_frameWatchId = static_cast<std::int32_t>(g_forceId);
    }
    g_frameWatchTicksLeft = 2000;
    g_frameTickLogsLeft = 24;
    g_frameWatchSeen = false;
    g_frameLastLogged = -1.0f;
    g_frameLastDt = -1.0f;
    g_seekFired = false;
    g_seekWrites = 0;
    ClkResetRegions();
    g_frameWatchArmed.store(true, std::memory_order_release);
    if (g_seekFrame >= 0.0f) {
        g_seekArmed.store(true, std::memory_order_release);
    }
}

// Runs on the property-getter hook (game thread, every tick). Watches the
// forced action's frame counter; when seek is configured and the action has
// actually begun (controller phase 2 = begin ran, frame restarted from 0),
// writes the start frame once.
// Track floats whose per-tick delta stays at a constant step: clocks (+),
// countdowns (-), frame counters. A region whose base pointer changes (object
// swap at action start) restarts its tracking. Offsets whose step wobbles get
// their hit counter reset.
void MineClocks(ClkRegion& r, const void* base) {
    if (!base || !IsReadable(base, r.size)) {
        r.lastBase = nullptr;
        return;
    }
    if (r.lastBase != base) {
        r.lastBase = base;
        std::fill(r.valid.begin(), r.valid.end(), std::uint8_t{0});
    }
    const std::uint32_t n = r.size / 4;
    auto* p = static_cast<const float*>(base);
    for (std::uint32_t i = 0; i < n; ++i) {
        const float v = p[i];
        if (!r.valid[i]) {
            r.valid[i] = 1;
            r.prev[i] = v;
            r.step[i] = 0.0f;
            continue;
        }
        const float d = v - r.prev[i];
        r.prev[i] = v;
        if (d == 0.0f) {
            continue;
        }
        const float ref = r.step[i];
        if (ref == 0.0f) {
            if (v == 0.0f || (d < 0.005f && d > -0.005f)) {
                continue; // noise floor: skip sub-tick drift (gauge decay etc.)
            }
            r.step[i] = d;
            r.hits[i] = 1;
            continue;
        }
        float tol = ref * 0.12f;
        if (tol < 0.0f) {
            tol = -tol;
        }
        if (tol < 0.002f) {
            tol = 0.002f;
        }
        if (d <= ref + tol && d >= ref - tol) {
            if (r.hits[i] < 255) {
                ++r.hits[i];
            }
        } else {
            r.step[i] = 0.0f;
            r.hits[i] = 0;
        }
    }
}

void TryFrameSeek(const void* entity) {
    if (!g_frameWatchArmed.load(std::memory_order_acquire)) {
        return;
    }
    void* ctrl = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        ctrl = g_lockedPlayerController;
    }
    if (!ctrl || !ModWeaponActive()) {
        return;
    }
    const void* player = static_cast<const std::uint8_t*>(ctrl) - 0x61C8;
    if (entity != player) {
        return;
    }
    auto* bytes = static_cast<std::uint8_t*>(ctrl);
    const std::int32_t phase = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xA8);
    const std::int32_t set = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xAC);
    const std::int32_t id = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xB0);

    if (g_frameWatchTicksLeft > 0) {
        --g_frameWatchTicksLeft;
    } else {
        g_frameWatchArmed.store(false, std::memory_order_release);
        Log(g_frameWatchSeen ? "FRAMEWATCH done"
                             : "FRAMEWATCH timeout (action never became current)");
        return;
    }
    if (set != g_frameWatchSet || id != g_frameWatchId) {
        if (g_seekArmed.load(std::memory_order_relaxed) && g_seekFired) {
            g_seekArmed.store(false, std::memory_order_release);
            Log("FRAMESEEK abandoned (action changed before settling)");
        }
        return;
    }
    g_frameWatchSeen = true;
    void* inst = ResolveActionInstance(ctrl, set, id);
    if (!inst) {
        return;
    }
    auto* instBytes = static_cast<std::uint8_t*>(inst);
    const float frame = *reinterpret_cast<volatile float*>(instBytes + 8);
    const float dt = *reinterpret_cast<volatile float*>(instBytes + 0xC);
    // The property getter fires ~10x per game frame; only log when the value
    // actually moves so the 24-line budget covers the whole action instead of
    // two frames. 2026-09-26 first run: dt = 1/30 -> the frame field counts
    // SECONDS (30Hz logic tick); TIML frames are 60/s, so TIML n = n/60 sec.
    auto* playerBytes = static_cast<std::uint8_t*>(ctrl) - 0x61C8;
    float motTime = -1.0f;
    void* anim = nullptr;
    if (IsReadable(playerBytes + 0x468, sizeof(void*))) {
        anim = *reinterpret_cast<void* const*>(playerBytes + 0x468);
        if (anim && IsReadable(static_cast<std::uint8_t*>(anim) + 0x10C, 4)) {
            motTime = *reinterpret_cast<volatile float*>(static_cast<std::uint8_t*>(anim) + 0x10C);
        }
    }
    if (g_frameTickLogsLeft > 0 && (frame != g_frameLastLogged || dt != g_frameLastDt)) {
        --g_frameTickLogsLeft;
        g_frameLastLogged = frame;
        g_frameLastDt = dt;
        const float endAt = *reinterpret_cast<volatile float*>(bytes + 0x6A8);
        std::ostringstream m;
        m << "FRAMETICK act=" << set << ':' << id << " phase=" << phase
          << " frame=" << frame << " dt=" << dt << " endAt=" << endAt
          << " mot=" << motTime;
        Log(m.str());
    }
    // Clock mining (until reported): resolve every candidate region each tick,
    // but only ONE mining pass per logic tick - property ticks fire ~10x per
    // game frame, and v2's report fired at "tick 28" after only ~3 frames, so
    // nothing had enough consistent-step hits to clear the threshold.
    if (!g_clkReported && g_clkTicks < 40 && frame != g_clkLastFrame) {
        g_clkLastFrame = frame;
        ++g_clkTicks;
        auto* playerBytes2 = static_cast<std::uint8_t*>(ctrl) - 0x61C8;
        const auto deref = [&](std::uintptr_t where) -> const void* {
            if (!IsReadable(reinterpret_cast<const void*>(where), sizeof(void*))) {
                return nullptr;
            }
            return *reinterpret_cast<void* const*>(where);
        };
        void* core = nullptr;
        if (IsReadable(instBytes + 0x10, sizeof(void*))) {
            core = *reinterpret_cast<void* const*>(instBytes + 0x10);
        }
        MineClocks(g_clkRegions[0], core);
        MineClocks(g_clkRegions[1], deref(reinterpret_cast<std::uintptr_t>(playerBytes2) + 0x468));
        MineClocks(g_clkRegions[2], deref(reinterpret_cast<std::uintptr_t>(playerBytes2) + 0x470));
        MineClocks(g_clkRegions[3], deref(reinterpret_cast<std::uintptr_t>(playerBytes2) + 0x76B0));
        MineClocks(g_clkRegions[4], playerBytes2 + 0x400);
        MineClocks(g_clkRegions[5], playerBytes2 + 0x1D600);
        MineClocks(g_clkRegions[6], instBytes);
        if (g_clkTicks == 28) {
            g_clkReported = true;
            for (int ri = 0; ri < 7; ++ri) {
                const auto& r = g_clkRegions[ri];
                if (!r.lastBase) {
                    continue;
                }
                std::ostringstream m;
                m << "CLK " << r.name << " base=0x" << std::hex
                  << reinterpret_cast<std::uintptr_t>(r.lastBase) << std::dec << " :";
                int shown = 0;
                for (std::uint32_t i = 0; i < r.size / 4 && shown < 14; ++i) {
                    if (r.hits[i] >= 14) {
                        m << (shown++ ? "," : "") << "+0x" << std::hex << i * 4 << std::dec
                          << " step=" << r.step[i] << " (" << static_cast<int>(r.hits[i])
                          << ')';
                    }
                }
                if (shown) {
                    Log(m.str());
                }
            }
        }
    }
    if (g_seekArmed.load(std::memory_order_relaxed)) {
        // v3: drive BOTH timelines. instance+8 is logic-side only (v2 test:
        // pinning it moved nothing on screen); the skeleton samples the MOTION
        // clock mot = *(float*)(*(player+0x468)+0x10C), which counts TIML
        // frames (+2.0/tick at the 30Hz logic rate, 60/s overall) and resets
        // to 0 on every motion (re)start. Seek target: config seconds -> x60
        // TIML frames. Same fresh-begin detection + pin-until-caught-up as the
        // frame write, symmetric for both fields.
        const float motTarget = g_seekFrame * 60.0f;
        const bool motOk = anim && motTime >= 0.0f;
        const bool framePast = g_seekFired && frame > g_seekFrame + dt * 0.5f;
        const bool motPast = !motOk || (g_seekFired && motTime > motTarget + 0.5f);
        if (framePast && motPast) {
            g_seekArmed.store(false, std::memory_order_release);
            std::ostringstream m;
            m << "FRAMESEEK settled act=" << set << ':' << id << " frame=" << frame
              << " mot=" << motTime << " (target " << g_seekFrame << "s, "
              << g_seekWrites << " writes)";
            Log(m.str());
        } else if (!g_seekFired || frame < g_seekFrame ||
                   (motOk && motTime < motTarget)) {
            if (g_seekWrites > 240) {
                g_seekArmed.store(false, std::memory_order_release);
                Log("FRAMESEEK gave up: action never reached the target");
            } else {
                if (!g_seekFired || frame < g_seekFrame) {
                    *reinterpret_cast<volatile float*>(instBytes + 8) = g_seekFrame;
                }
                if (motOk && (!g_seekFired || motTime < motTarget)) {
                    *reinterpret_cast<volatile float*>(static_cast<std::uint8_t*>(anim) +
                                                       0x10C) = motTarget;
                }
                ++g_seekWrites;
                if (!g_seekFired) {
                    g_seekFired = true;
                    std::ostringstream m;
                    m << "FRAMESEEK act=" << set << ':' << id << " frame " << frame
                      << " -> " << g_seekFrame << " mot " << motTime << " -> "
                      << motTarget << " (dt=" << dt << ')';
                    Log(m.str());
                }
            }
        }
    }
}

// MOTIONBIND per-tick: see MotionBind. Runs on the game thread inside the
// property-getter hook (~10x/frame), same cadence as FRAMESEEK. Each bound
// action plays its bound LMT entry unconditionally (the FSM/routing decides
// WHICH action fires; the DLL only guarantees the action<->LMT pairing).
void PollMotionBind(const void* entity) {
    if (g_motionBinds.empty()) {
        return;
    }
    void* ctrl = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        ctrl = g_lockedPlayerController;
    }
    if (!ctrl || !ModWeaponActive()) {
        return;
    }
    if (entity != static_cast<const void*>(
                       static_cast<const std::uint8_t*>(ctrl) - 0x61C8)) {
        return;
    }
    auto* bytes = static_cast<std::uint8_t*>(ctrl);
    const std::int32_t set = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xAC);
    const std::int32_t id = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xB0);
    if (set != 3) {
        return;
    }
    for (auto& bind : g_motionBinds) {
        if (static_cast<std::uint32_t>(id) != bind.actionId) {
            if (++bind.missTicks > 3) {
                bind.swapped = false;
                bind.lastActId = 0;
                bind.missTicks = 0;
            }
            continue;
        }
        bind.missTicks = 0;
        if (bind.swapped) {
            continue;
        }
        if (bind.lastActId != static_cast<std::uint32_t>(id)) {
            bind.lastActId = static_cast<std::uint32_t>(id);
            bind.swapped = false;
        }
        void* inst = ResolveActionInstance(ctrl, set, id);
        if (!inst) {
            continue;
        }
        const float frame =
            *reinterpret_cast<volatile float*>(static_cast<std::uint8_t*>(inst) + 8);
        // A backwards frame jump = the action re-began (chained re-cast):
        // re-arm so the re-cast binds again. Our own restart sets prevFrame
        // = -1 so it does not re-trigger itself.
        if (frame + 0.5f < bind.prevFrame) {
            bind.swapped = false;
        }
        bind.prevFrame = frame;
        if (bind.swapped) {
            continue;
        }
        if (!(frame < 0.2f)) {
            bind.swapped = true; // missed the fresh window, keep vanilla
            std::ostringstream m;
            m << "MOTIONBIND act=3:" << id << " missed fresh window (frame "
              << frame << ") - keeping vanilla motion";
            Log(m.str());
            continue;
        }
        const std::uint32_t motionId = (bind.entry & 0x1FF) | 0xC000;
        *reinterpret_cast<volatile std::uint32_t*>(
            static_cast<std::uint8_t*>(inst) + 0x30) = motionId;
        bool restarted = false;
        auto* playerPtr = bytes - 0x61C8;
        if (IsReadable(playerPtr, sizeof(void*))) {
            auto* vtbl = *reinterpret_cast<void**>(playerPtr);
            auto* slot = reinterpret_cast<std::uint8_t*>(vtbl) + 0x288;
            if (IsReadable(slot, sizeof(void*))) {
                void* fn = *reinterpret_cast<void**>(slot);
                if (fn && IsReadable(fn, 1)) {
                    reinterpret_cast<void(__fastcall*)(void*, std::uint32_t,
                                                       std::uint32_t)>(fn)(
                        playerPtr, motionId, 0);
                    restarted = true;
                }
            }
        }
        bind.swapped = true;
        bind.prevFrame = -1.0f; // the restart below resets the frame
        std::ostringstream m;
        m << "MOTIONBIND act=3:" << id << " -> motion " << bind.entry
          << " (0x" << std::hex << motionId << ")" << std::dec
          << (restarted ? " restarted" : " <no restart>");
        Log(m.str());
    }
}

#endif // !VALOR_PROBE
