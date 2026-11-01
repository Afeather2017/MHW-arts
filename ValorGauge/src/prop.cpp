// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L3940-L3947, L3957-L3995, L3998-L4024, L5172-L5236, L5239-L5267, L5447-L5648. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

PropGetIntFn g_originalPropInt = nullptr;
PropGetBoolFn g_originalPropBool = nullptr;
PropGetFloatFn g_originalPropFloat = nullptr;

// --- F11 FSM property-access histogram --------------------------------------
// FUN_141922190 / FUN_1419221f0 / FUN_141922240 are the FSM condition-tree
// property getters: (entity, id, index[, default]) -> int16 counter stored at
// row+0x24+index*2 of the entity's property row (the float variant scales the
// counter by a fixed frame constant). Whatever the game consults to decide
// "success dodge" (the clang + blue flash) must be read here only in captures
// where a hit actually lands inside the defense window. F11 dumps the
// accumulated (entity, id, index) histogram with caller RVAs, then resets it.

bool g_f11WasDown = false;

struct PropCallKey {
    const void* entity;
    std::uint32_t id;
    std::uint32_t index;
    std::uint32_t kind; // 0 = int, 1 = bool, 2 = float
    bool operator==(const PropCallKey& other) const {
        return entity == other.entity && id == other.id &&
               index == other.index && kind == other.kind;
    }
};

struct PropCallKeyHash {
    std::size_t operator()(const PropCallKey& key) const noexcept {
        std::uint64_t h = reinterpret_cast<std::uint64_t>(key.entity);
        h ^= h >> 33;
        h *= 0xFF51AFD7ED558CCDull;
        h ^= (h >> 33) ^ (static_cast<std::uint64_t>(key.id) << 1);
        h *= 0xC4CEB9FE1A85EC53ull;
        h ^= (h >> 33) ^ (static_cast<std::uint64_t>(key.index) << 17) ^
             static_cast<std::uint64_t>(key.kind);
        return static_cast<std::size_t>(h);
    }
};

struct PropCallStats {
    std::uint32_t count = 0;
    std::uintptr_t callers[6] = {};
    std::uint32_t callerCount = 0;
    // Highest return value seen (bit-cast for the float variant). Reveals
    // whether a condition property ever reads true during a capture.
    std::int32_t maxRet = 0;
};

std::mutex g_propMutex;
std::unordered_map<PropCallKey, PropCallStats, PropCallKeyHash> g_propCalls;
bool g_propOverflow = false;
std::atomic_bool g_propRegistryDumped{false};

#ifdef VALOR_PROBE
void RecordPropCall(const void* entity, std::uint32_t id, std::uint32_t index,
                    std::uint32_t kind, const void* caller, std::int32_t ret) {
    // Benign racy read: the pointer is only used to tag the player in the dump.
    const void* controller = g_lockedPlayerController;
    const void* player = controller ? static_cast<const std::uint8_t*>(controller) - 0x61C8
                                    : nullptr;
    if (entity != player && g_propCalls.size() > 20000) {
        g_propOverflow = true;
        return;
    }
    const auto callerRva = caller
        ? reinterpret_cast<std::uintptr_t>(caller) - g_moduleBase.load()
        : 0;
    std::scoped_lock lock(g_propMutex);
    auto& stats = g_propCalls[{entity, id, index, kind}];
    ++stats.count;
    if (ret > stats.maxRet) {
        stats.maxRet = ret;
    }
    bool knownCaller = false;
    for (std::uint32_t i = 0; i < stats.callerCount; ++i) {
        knownCaller = knownCaller || stats.callers[i] == callerRva;
    }
    if (!knownCaller && stats.callerCount < 6) {
        stats.callers[stats.callerCount++] = callerRva;
    }
}

#endif // VALOR_PROBE
#ifndef VALOR_PROBE
int __fastcall PropIntHook(void* entity, void* id, std::uint32_t index, int defaultValue) {
    TryQueuedForceWrite(entity);
    TryQueuedFx(entity);
    TryFrameSeek(entity);
#ifndef VALOR_PROBE
    PollMotionBind(entity);
    MotionTraceTick(entity);
    PollGaugeBeginRules(entity);
    PollTrailRules(entity);
    PollRollSheathe(entity);
    PollSheathComboProbe(entity);
    PollActionSounds(entity);
    PollDrawGang1(entity);
    PollGang1GaugeScope();
    PollGaugeWatch();
#endif
    const int result = g_originalPropInt(entity, id, index, defaultValue);
    PropTraceRecord(entity, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(id)),
                    index, 0, result);
#ifdef VALOR_PROBE
    RecordPropCall(entity, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(id)),
                   index, 0, _ReturnAddress(), result);
#endif
    return result;
}

// mod 构建的 bool 属性 getter 钩子: 居合斬り类条件为布尔型, PROPTRACE 用。
bool __fastcall PropBoolHookMod(void* entity, void* id, std::uint32_t index) {
    const bool result = g_originalPropBool(entity, id, index);
    PropTraceRecord(entity, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(id)),
                    index, 1, result ? 1 : 0);
    return result;
}

bool InstallPropBoolHookMod() {
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkPropBool]),
                      reinterpret_cast<void*>(&PropBoolHookMod),
                      reinterpret_cast<void**>(&g_originalPropBool)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkPropBool])) != MH_OK) {
        Log("PROPTRACE: prop bool hook failed");
        return false;
    }
    Log("PROPTRACE: prop bool hook installed");
    return true;
}

// The force path rides the property-int getter (game thread, every frame).
// The probe build installs this via the F11 histogram; the mod build needs it
// standalone.
bool InstallForceHook() {
    const HMODULE game = GetModuleHandleW(nullptr);
    MODULEINFO module{};
    if (!game || !GetModuleInformation(GetCurrentProcess(), game, &module, sizeof(module))) {
        Log("Force hook failed: module info unavailable");
        return false;
    }
    void* target = reinterpret_cast<void*>(g_hookAddr[HkPropInt]);
    if (MH_CreateHook(target, reinterpret_cast<void*>(&PropIntHook),
                      reinterpret_cast<void**>(&g_originalPropInt)) != MH_OK ||
        MH_EnableHook(target) != MH_OK) {
        Log("Force hook (prop int getter) failed");
        return false;
    }
    Log("Force hook (prop int getter) installed");
    return true;
}

#endif // !VALOR_PROBE
// --- PROPTRACE: 属性求值轨迹 (诊断翻滚切納刀的居合斬り属性源) -----------------
// addresses.txt `propTrace=1` 启用。只记本地玩家、结果非零、且该 (kind,id,index)
// 发生 0→非0 跳变的行, 上限 120 行。成功切納刀时出现的 id = 居合斬り真值源。
// (atomics 定义在 InitMemoryMap 前)
void PropTraceRecord(const void* entity, std::uint32_t id, std::uint32_t index,
                     std::uint32_t kind, std::int32_t ret) {
    if (!g_propTrace.load(std::memory_order_relaxed)) return;
    if (ret == 0) return;
    const void* controller = g_lockedPlayerController;
    const void* player = controller
        ? static_cast<const std::uint8_t*>(controller) - 0x61C8
        : nullptr;
    if (entity != player) return;
    static std::mutex s_mtx;
    static std::map<std::uint64_t, std::int32_t> s_last;
    const std::uint64_t key =
        (std::uint64_t(kind) << 40) | (std::uint64_t(id) << 8) | (index & 0xFF);
    {
        std::scoped_lock lock(s_mtx);
        auto it = s_last.find(key);
        if (it != s_last.end() && it->second == ret) return;
        s_last[key] = ret;
    }
    if (g_propTraceLines.fetch_add(1, std::memory_order_relaxed) > 120) return;
    std::ostringstream m;
    m << "PROPTRACE kind=" << kind << " id=0x" << std::hex << id << std::dec
      << " idx=" << index << " ret=" << ret;
    Log(m.str().c_str());
}

#ifdef VALOR_PROBE
// Probe variant of the int getter: histogram only, no force path.
int __fastcall PropIntHook(void* entity, void* id, std::uint32_t index, int defaultValue) {
    const int result = g_originalPropInt(entity, id, index, defaultValue);
    RecordPropCall(entity, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(id)),
                   index, 0, _ReturnAddress(), result);
    PropTraceRecord(entity, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(id)),
                    index, 0, result);
    return result;
}

bool __fastcall PropBoolHook(void* entity, void* id, std::uint32_t index) {
    const bool result = g_originalPropBool(entity, id, index);
    RecordPropCall(entity, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(id)),
                   index, 1, _ReturnAddress(), result ? 1 : 0);
    PropTraceRecord(entity, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(id)),
                    index, 1, result ? 1 : 0);
    return result;
}

float __fastcall PropFloatHook(void* entity, void* id, std::uint32_t index, std::uint64_t defaultBits) {
    const float result = g_originalPropFloat(entity, id, index, defaultBits);
    std::int32_t bits = 0;
    std::memcpy(&bits, &result, sizeof(bits));
    RecordPropCall(entity, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(id)),
                   index, 2, _ReturnAddress(), bits);
    return result;
}

// FUN_141979770 resolves a property id to its display name from the game's
// own name tables; SEH-wrapped because it runs before we know table state.
const char* SafePropName(std::int32_t id) {
    const auto nameFn = reinterpret_cast<PropNameFn>(g_addrPropName);
    __try {
        return nameFn(id);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

// One-time enumeration of the global property registry (object at RVA
// 0x5012048, count at +0xB8, row lookup at RVA 0x462700) so histogram ids
// can be matched against display names.
void DumpPropRegistry() {
    if (g_propRegistryDumped.exchange(true)) {
        return;
    }
    auto* table = reinterpret_cast<std::uint8_t*>(g_addrPropRegistry);
    if (!IsReadable(table, 0xC0)) {
        Log("REGISTRY unavailable: table unreadable");
        return;
    }
    const auto count = *reinterpret_cast<const std::int32_t*>(table + 0xB8);
    std::ostringstream header;
    header << "REGISTRY BEGIN count=" << std::dec << count;
    Log(header.str());
    if (count > 0) {
        const auto lookup = reinterpret_cast<PropRegistryLookupFn>(g_addrPropLookup);
        const std::int32_t limit = count > 1500 ? 1500 : count;
        for (std::int32_t i = 0; i < limit; ++i) {
            const char* name = SafePropName(i);
            void* row = lookup(table, i);
            std::ostringstream line;
            line << "REG id=" << std::dec << i << std::hex << std::uppercase
                 << " row=0x" << reinterpret_cast<std::uintptr_t>(row)
                 << " name='" << (name ? name : "?") << "'";
            Log(line.str());
        }
    }
    Log("REGISTRY END");
}

void DumpPropHistogram() {
    std::vector<std::pair<PropCallKey, PropCallStats>> entries;
    {
        std::scoped_lock lock(g_propMutex);
        entries.assign(g_propCalls.begin(), g_propCalls.end());
        g_propCalls.clear();
    }
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) {
                  if (a.first.kind != b.first.kind) return a.first.kind < b.first.kind;
                  if (a.first.id != b.first.id) return a.first.id < b.first.id;
                  if (a.first.index != b.first.index) return a.first.index < b.first.index;
                  return a.first.entity < b.first.entity;
              });

    const void* controller = g_lockedPlayerController;
    const void* player = controller ? static_cast<const std::uint8_t*>(controller) - 0x61C8
                                    : nullptr;

    std::ostringstream header;
    header << "PROPHIST BEGIN entries=" << std::dec << entries.size()
           << (g_propOverflow ? " OVERFLOW(some non-player entries dropped)" : "");
    g_propOverflow = false;
    Log(header.str());

    // Property names for the id space seen in this capture (id -> name).
    std::unordered_map<std::uint32_t, const char*> names;
    for (const auto& entry : entries) {
        if (names.size() >= 80 || names.count(entry.first.id)) {
            continue;
        }
        names.emplace(entry.first.id, SafePropName(static_cast<std::int32_t>(entry.first.id)));
    }
    for (const auto& [id, name] : names) {
        if (name && *name) {
            std::ostringstream line;
            line << "PNAME id=" << std::dec << id << " name='" << name << "'";
            Log(line.str());
        }
    }

    constexpr std::size_t kMaxLines = 4000;
    std::size_t printed = 0;
    for (const auto& [key, stats] : entries) {
        if (printed >= kMaxLines) {
            std::ostringstream truncated;
            truncated << "PROPHIST TRUNCATED remaining=" << std::dec
                      << entries.size() - printed;
            Log(truncated.str());
            break;
        }
        std::ostringstream line;
        line << "PROP kind=" << (key.kind == 0 ? "INT" : key.kind == 1 ? "BOOL" : "FLT")
             << " id=" << std::dec << key.id << std::hex << std::uppercase
             << "(0x" << key.id << ") idx=" << std::dec << key.index
             << " ent=" << (key.entity == player
                                ? "PLAYER"
                                : ([&] {
                                      std::ostringstream e;
                                      e << std::hex << std::uppercase << "0x"
                                        << reinterpret_cast<std::uintptr_t>(key.entity);
                                      return e.str();
                                  })())
             << " cnt=" << std::dec << stats.count << std::hex << std::uppercase
             << " max=0x" << stats.maxRet << " callers=[";
        for (std::uint32_t i = 0; i < stats.callerCount; ++i) {
            if (i) {
                line << ',';
            }
            line << std::hex << std::uppercase << "0x" << stats.callers[i];
        }
        line << ']';
        Log(line.str());
        ++printed;
    }
    Log("PROPHIST END");
}

void PollPropHistogram() {
    const bool f11Down = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
    if (f11Down && !g_f11WasDown) {
        DumpPropRegistry();
        DumpPropHistogram();
    }
    g_f11WasDown = f11Down;
}

bool InstallPropHistogramHooks() {
    const HMODULE game = GetModuleHandleW(nullptr);
    MODULEINFO module{};
    if (!game || !GetModuleInformation(GetCurrentProcess(), game, &module, sizeof(module))) {
        Log("Property histogram hooks failed: module info unavailable");
        return false;
    }
    g_moduleBase = reinterpret_cast<std::uintptr_t>(module.lpBaseOfDll);

    struct HookEntry {
        std::uintptr_t rva;
        void* detour;
        void** original;
        const char* label;
    };
    const HookEntry entries[] = {
        {0, reinterpret_cast<void*>(&PropIntHook),
         reinterpret_cast<void**>(&g_originalPropInt), "int"},
        {0, reinterpret_cast<void*>(&PropBoolHook),
         reinterpret_cast<void**>(&g_originalPropBool), "bool"},
        {0, reinterpret_cast<void*>(&PropFloatHook),
         reinterpret_cast<void**>(&g_originalPropFloat), "float"},
    };
    const std::uintptr_t hookAddrs[3] = {g_hookAddr[HkPropInt], g_hookAddr[HkPropBool],
                                         g_hookAddr[HkPropFloat]};
    bool allOk = true;
    for (int i = 0; i < 3; ++i) {
        const auto& entry = entries[i];
        void* target = reinterpret_cast<void*>(hookAddrs[i]);
        if (MH_CreateHook(target, entry.detour, entry.original) != MH_OK ||
            MH_EnableHook(target) != MH_OK) {
            std::ostringstream m;
            m << "Property histogram hook failed (" << entry.label << ") at 0x"
              << std::hex << std::uppercase
              << reinterpret_cast<std::uintptr_t>(target);
            Log(m.str());
            allOk = false;
        }
    }
    if (allOk) {
        Log("Property histogram hooks installed (int/bool/float), F11 dumps");
    }
    return allOk;
}

#endif // VALOR_PROBE
