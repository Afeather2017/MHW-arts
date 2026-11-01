// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L623-L633, L896-L902, L911-L1036, L1038-L1038, L1049-L1092, L1094-L1147, L1149-L1192, L1194-L1235. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

// Quest-state singleton slot (HunterPie map, build 421810): *(base+RVA) ->
// quest struct: int32 id at +0x4C (0 = no real quest: explorations and the
// training area run through the quest framework with an empty id - HunterPie's
// isQuestInvalid = Id <= 0 exists exactly for those), state int at +0x54
// (2 = InQuest). Feeds ReadQuestInfo(); NOT hook-resolved - a plain data
// pointer, so only the addresses.txt override backs it up.
std::uintptr_t g_addrQuestStateMgr = 0x500ED30;
// Zone singleton slot (HunterPie ZONE_OFFSET): *(base+RVA) -> int32 stage id
// at +0xAED0. Feeds InCombatZone()'s whitelist for combat areas that are not
// quests (训练场) or whose quest-state bit does not read InQuest (探索).
std::uintptr_t g_addrZoneMgr = 0x500ECA0;

// --- Update-resilient address map --------------------------------------------
// A game patch moves code. Hook targets resolve by byte signature first
// (wildcards cover rel32 branches and rip-relative displacements), then by
// the recorded RVA of the last known build, then by a manual override file
// nativePC/plugins/ValorGauge.addresses.txt ("name=0xRVA" lines). Signatures
// are generated and verified offline against the unpacked dump exe:
//   python Reverse/sig_check.py --emit && python Reverse/sig_check.py --verify

struct HookSig {
    std::uint8_t pattern[48];
    char mask[49];
    std::uint32_t fallbackRva;
};

constexpr HookSig kHookSigs[HkCount] = {
    // condEval = FUN_14118BDA0 condition-tree node evaluator
    {{0x4C, 0x8B, 0xDC, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x70,
      0x41, 0x83, 0x78, 0x10, 0x00, 0x44, 0x8B, 0xE1, 0x4D, 0x8B, 0x78, 0x08, 0x4D, 0x8B, 0xE8,
      0x48, 0x8B, 0xFA, 0x49, 0x8B, 0xCF, 0x49, 0x8B, 0x07, 0x0F, 0x86, 0x7F, 0x01, 0x00, 0x00,
      0x49, 0x89, 0x5B},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxx", 0x118BDA0},
    // poolQuery = FUN_140AEE050 defense/no-hit query
    {{0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x8D, 0x42, 0xFF, 0x0F, 0x29,
      0x74, 0x24, 0x20, 0x8B, 0xFA, 0x48, 0x8B, 0xD9, 0x0F, 0x57, 0xF6, 0x0F, 0x57, 0xC0, 0x83,
      0xF8, 0x05, 0x0F, 0x87, 0x4A, 0x01, 0x00, 0x00, 0x48, 0x8D, 0x0D, 0x83, 0x1F, 0x51, 0xFF,
      0x44, 0x8B, 0x84},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxx????xxx", 0xAEE050},
    // dmgCalc = FUN_141F66DD0 damage calculation entry
    {{0x48, 0x8B, 0xC4, 0x48, 0x89, 0x50, 0x10, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55,
      0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0x68, 0x98, 0x48, 0x81, 0xEC, 0x28, 0x01, 0x00, 0x00,
      0x48, 0x8B, 0xBA, 0xB8, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xF1, 0x0F, 0x29, 0x70, 0xA8, 0x49,
      0x8B, 0xD8, 0x0F},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 0x1F66DD0},
    // hitDispatch = FUN_141F71960 hit dispatcher
    {{0x40, 0x53, 0x57, 0x48, 0x83, 0xEC, 0x48, 0x48, 0x8B, 0xFA, 0x48, 0x8B, 0xD9, 0xE8, 0x4E,
      0xF8, 0xC8, 0xFF, 0x48, 0x8B, 0x07, 0x48, 0x8B, 0xCF, 0xFF, 0x50, 0x08, 0x83, 0xC0, 0xFA,
      0x83, 0xF8, 0x6A, 0x0F, 0x87, 0xF2, 0x08, 0x00, 0x00, 0x48, 0x89, 0x74, 0x24, 0x70, 0x48,
      0x8D, 0x15, 0x6D},
     "xxxxxxxxxxxxxx????xxxxxxxxxxxxxxxxx????xxxxxxxxx", 0x1F71960},
    // propInt = FUN_141922190 FSM property int getter
    {{0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x41, 0x8B, 0xF8, 0x41, 0x8B,
      0xD9, 0x48, 0x85, 0xC9, 0x74, 0x34, 0xF6, 0x41, 0x60, 0x01, 0x74, 0x2E, 0x48, 0x8B, 0x89,
      0xD8, 0x26, 0x01, 0x00, 0xE8, 0x39, 0x5F, 0x9D, 0xFF, 0x85, 0xC0, 0x74, 0x1E, 0x48, 0x8B,
      0x0D, 0x86, 0xFE},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxxxx", 0x1922190},
    // propBool = FUN_1419221F0 FSM property bool getter
    {{0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x41, 0x8B, 0xD8, 0x48, 0x85, 0xC9, 0x74, 0x34, 0xF6,
      0x41, 0x60, 0x01, 0x74, 0x2E, 0x48, 0x8B, 0x89, 0xD8, 0x26, 0x01, 0x00, 0xE8, 0xE0, 0x5E,
      0x9D, 0xFF, 0x85, 0xC0, 0x74, 0x1E, 0x48, 0x8B, 0x0D, 0x2D, 0xFE, 0x6E, 0x03, 0x8B, 0xD0,
      0xE8, 0xDE, 0x04},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxx????xxxxx", 0x19221F0},
    // propFloat = FUN_141922240 FSM property float getter
    {{0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x0F, 0x29, 0x74, 0x24, 0x20, 0x0F, 0x28, 0xF3, 0x41,
      0x8B, 0xD8, 0x48, 0x85, 0xC9, 0x74, 0x43, 0xF6, 0x41, 0x60, 0x01, 0x74, 0x3D, 0x48, 0x8B,
      0x89, 0xD8, 0x26, 0x01, 0x00, 0xE8, 0x88, 0x5E, 0x9D, 0xFF, 0x85, 0xC0, 0x74, 0x2D, 0x48,
      0x8B, 0x0D, 0xD5},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxxxxxx", 0x1922240},
    // gaugeAdd = FUN_142122410 the ONLY float writer of weapon+0x2368 (spirit
    // gauge): every per-move gain/cost funnels through it. The 0x14211EF90 twin
    // accumulates +0x236C instead; the call displacement bytes (DF 0F) keep the
    // signature pinned to the +0x2368 variant.
    {{0x40, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x0F, 0x29, 0x74, 0x24, 0x30, 0x48, 0x8B, 0xD9, 0x0F,
      0x29, 0x7C, 0x24, 0x20, 0x0F, 0x57, 0xF6, 0x0F, 0x28, 0xF9, 0x0F, 0x2F, 0xFE, 0x76, 0x21,
      0x48, 0x8B, 0x81, 0xB0, 0x09, 0x00, 0x00, 0x48, 0x8B, 0x88, 0x20, 0x7D, 0x00, 0x00, 0xE8,
      0xDF, 0x0F, 0x0A},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 0x2122410},
    // fxNotify = FUN_140ADD670 player-emitter effect notify (body auras). The
    // 0x201DCB0 twin shares 32 bytes; the post-call movss/lea tail (offsets
    // 36-46) discriminates. Wildcards: call rel32 + rip-relative disp32s.
    {{0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x81, 0xEC, 0x30,
      0x01, 0x00, 0x00, 0x48, 0x8B, 0xF1, 0x41, 0x8B, 0xD8, 0x48, 0x8D, 0x4C, 0x24, 0x40, 0x8B,
      0xFA, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xF3, 0x0F, 0x10, 0x05, 0x00, 0x00, 0x00, 0x00, 0x48,
      0x8D, 0x05},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxx????xxx", 0xADD670},
    // dealDamage = FUN_141cc5f80 = HunterPie's FUN_DEAL_DAMAGE (map 421810,
    // dump is the same build): the applied-damage entry, fired once per
    // damage instance with arg1 = victim em (monster id int32 @+0x12280 -
    // the id compare sits right in the prologue) and arg2 = int damage.
    // Prologue carries no rel32/rip-relative in 48 bytes - all-exact mask.
    {{0x48, 0x83, 0xEC, 0x68, 0x83, 0xB9, 0x80, 0x22, 0x01, 0x00, 0x0F, 0x75, 0x59, 0x8B, 0x84, 0x24,
      0xB0, 0x00, 0x00, 0x00, 0xC6, 0x44, 0x24, 0x50, 0x00, 0xC7, 0x44, 0x24, 0x48, 0x00, 0x00, 0x00,
      0x00, 0x89, 0x44, 0x24, 0x40, 0x0F, 0xB6, 0x84, 0x24, 0xA8, 0x00, 0x00, 0x00, 0x88, 0x44, 0x24},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 0x1CC5F80},
    // dmgFormula = FUN_1402bc4a0 = the player damage formula (writes the
    // damage float pair FUN_1402c3030 applies). arg2 (rdx) = the attack
    // context struct built by FUN_1402c1050. Prologue has no rel32/rip-rel
    // in 48 bytes - all-exact mask.
    {{0x48, 0x8B, 0xC4, 0x44, 0x89, 0x48, 0x20, 0x44, 0x89, 0x40, 0x18, 0x55, 0x53, 0x56, 0x57, 0x41,
      0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0x68, 0xB8, 0x48, 0x81, 0xEC, 0x08, 0x01,
      0x00, 0x00, 0x48, 0x8B, 0x7D, 0x78, 0x48, 0x8B, 0xF2, 0x0F, 0x29, 0x70, 0xA8, 0x4C, 0x8B, 0xF9},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 0x2BC4A0},
    // atkCtxBuilder = FUN_1402c1050 = the attack-context builder. Its
    // param_1+0x10 is the attacker's STATS object (X) - the fixed-damage
    // getters (0x141216910/920) read float@X+0x64 / float@X+0x60, and X is
    // a dedicated stats component the earlier player/weapon/w03p scans never
    // covered. Dumping X is where the combined attack (user ref 1366/1399)
    // should finally show up.
    {{0x48, 0x8B, 0xC4, 0x55, 0x57, 0x48, 0x8D, 0xA8, 0x58, 0xFE, 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0x98,
      0x02, 0x00, 0x00, 0x48, 0x89, 0x58, 0x10, 0x49, 0x8B, 0xF8, 0x48, 0x89, 0x70, 0x18, 0x4C, 0x89,
      0x60, 0x20, 0x4C, 0x89, 0x68, 0xE8, 0x4C, 0x8B, 0xEA, 0x4C, 0x89, 0x70, 0xE0, 0x4C, 0x8B, 0xF1},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 0x2C1050},
    // sheathClsfy = FUN_14026b830 = set-3 candidate-row matcher (per-action
    // request subtables; the (1,23,...,3,98) row is the special-sheathe
    // cut-in). 48-byte prologue has no rel32/rip-rel - all-exact mask.
    {{0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18,
      0x48, 0x89, 0x7C, 0x24, 0x20, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x30,
      0x8B, 0x5C, 0x24, 0x70, 0x45, 0x33, 0xD2, 0x44, 0x89, 0x54, 0x24, 0x70, 0x49, 0x8B, 0xF9,
      0x4D, 0x8B, 0xE0},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 0x26B830},
    // sheathCap = FUN_14026c0f0 = set-3 classifier entry ({set,key} pulled from
    // the current-action object; param_1 = classifier mgrCtx). Captured once at
    // runtime so the roll-sheathe poll can reach the combo-23 checker. No
    // rel32/rip-rel in the 48-byte prologue.
    {{0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57, 0x48, 0x83, 0xEC, 0x20,
      0x8B, 0x82, 0xBC, 0x00, 0x00, 0x00, 0x41, 0x8B, 0xD9, 0x49, 0x8B, 0xF8, 0x48, 0x8B, 0xF1,
      0x83, 0xF8, 0xFF, 0x74, 0x15, 0x83, 0xBA, 0xC0, 0x00, 0x00, 0x00, 0xFF, 0x74, 0x0C, 0x89,
      0x44, 0x24, 0x38},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 0x26C0F0},
    // sheathEval = FUN_14026ba00 = classifier checker evaluator. Reads the
    // checker object at mgrCtx[(row.type+0xC)*0x10][row.comboId], flag byte
    // at +8 (bit1 set = combo unavailable this frame), then calls the
    // checker's vtable+0x30 evaluate. Hooked to log what the game asks per
    // combo and what the evaluate returns. 48-byte prologue all-exact.
    {{0x40, 0x53, 0x55, 0x56, 0x41, 0x56, 0x48, 0x83, 0xEC, 0x68, 0x49, 0x63, 0x00, 0x48, 0x8B, 0xD9,
      0x4D, 0x63, 0x50, 0x04, 0x48, 0x83, 0xC0, 0x0C, 0x48, 0x03, 0xC0, 0x4C, 0x8B, 0xF2, 0x41, 0x0F,
      0xB6, 0xE9, 0x49, 0x8B, 0xF0, 0x48, 0x8B, 0x04, 0xC1, 0x4A, 0x8B, 0x0C, 0xD0, 0x0F, 0xB6, 0x51},
     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 0x26BA00},
    // chipThreshold = FUN_1411c10c0 = the chip-damage threshold getter
    // (prop-int id 0x94 thunk). 15 exact bytes incl. the MOV EDX,0x94
    // immediate + JMP rel32; unique hit verified against the dump exe.
    {{0x48, 0x8B, 0x49, 0x08, 0x45, 0x33, 0xC9, 0x45, 0x33, 0xC0, 0xBA, 0x94, 0x00, 0x00, 0x00,
      0xE9},
     "xxxxxxxxxxxxxxxx????", 0x11C10C0},
    // vitSet = FUN_141216940 = the low-level vitals box setter (leaf):
    //   flag!=0: cur = clamp(max(value,1), max)   flag==0 && value<1: cur = 0
    // The (flag==0 && value<1) shape is the engine's LETHAL write - the real
    // damage caller (RVA 0x1218E18 in FUN_141217e50) funnels through here.
    // 63-byte all-exact except rel8/disp32; unique hit verified on the dump.
    {{0x45, 0x84, 0xC0, 0x74, 0x00, 0xF3, 0x0F, 0x5F, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x57,
      0xC0, 0x0F, 0x2F, 0xC8, 0x72, 0x00, 0xF3, 0x0F, 0x10, 0x41, 0x60, 0xF3, 0x0F, 0x5D, 0xC1,
      0xF3, 0x0F, 0x11, 0x41, 0x64, 0xC3},
     "xxxx?xxxx????xxxxxxx?xxxxxxxxxxxxxxx", 0x1216940},
};

const char* const kHookNames[HkCount] = {
    "condEval", "poolQuery", "dmgCalc", "hitDispatch",
    "propInt", "propBool", "propFloat",
    "gaugeAdd", "fxNotify",
    "dealDamage", "dmgFormula", "atkCtxBuilder",
    "sheathClsfy", "sheathCap", "sheathEval",
    "chipThreshold", "vitSet",
};

std::uintptr_t g_hookAddr[HkCount] = {};
std::uintptr_t g_addrAudioSingleton = 0x51C458; // audio manager singleton slot
std::uintptr_t g_addrPropRegistry = 0x5012048;  // FSM property registry (count +0xB8, rows +0xC8)
std::uintptr_t g_addrPropLookup = 0x462700;     // registry row lookup FUN_140462700
std::uintptr_t g_addrPropName = 0x1979770;      // property id -> name
std::uintptr_t g_addrDoAction = 0;              // set by InstallActionHook

std::map<std::string, std::uintptr_t> g_addrOverrides;

void LoadAddressOverrides() {
    std::ifstream input("nativePC/plugins/ValorGauge.addresses.txt");
    if (!input) {
        return;
    }
    std::string line;
    int count = 0;
    while (std::getline(input, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const auto trim = [](std::string s) {
            while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
                s.erase(s.begin());
            }
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
                s.pop_back();
            }
            return s;
        };
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        if (key.empty() || value.empty()) {
            continue;
        }
        g_addrOverrides[key] = std::strtoull(value.c_str(), nullptr, 0);
        ++count;
    }
    if (count > 0) {
        std::ostringstream m;
        m << "MEMORYMAP: " << count << " address override(s) loaded from ValorGauge.addresses.txt";
        Log(m.str());
    }
}

std::uintptr_t OverrideOrZero(const char* name) {
    const auto it = g_addrOverrides.find(name);
    return it == g_addrOverrides.end() ? 0 : it->second;
}

// Resolve one hook target: override > unique signature > last-known RVA.
std::uintptr_t ResolveHookTarget(HookId id) {
    const HookSig& sig = kHookSigs[id];
    const std::uintptr_t base = ModuleBaseAddr();
    if (const std::uintptr_t overrideRva = OverrideOrZero(kHookNames[id])) {
        std::ostringstream m;
        m << "MEMORYMAP: " << kHookNames[id] << " via addresses.txt override";
        Log(m.str());
        return base + overrideRva;
    }
    auto* hit = FindPattern(sig.pattern, sig.mask, sizeof(sig.pattern));
    if (hit) {
        // Reject ambiguous patterns: a second match means the signature is too
        // weak to trust after an update.
        const auto start = reinterpret_cast<const std::uint8_t*>(base);
        const std::size_t next = static_cast<std::size_t>(hit - start) + 1;
        bool ambiguous = false;
        if (const std::uintptr_t moduleBase = ModuleBaseAddr()) {
            const HMODULE game = GetModuleHandleW(nullptr);
            MODULEINFO module{};
            if (game && GetModuleInformation(GetCurrentProcess(), game, &module, sizeof(module))) {
                const auto scanStart = static_cast<const std::uint8_t*>(module.lpBaseOfDll);
                const auto imageSize = static_cast<std::size_t>(module.SizeOfImage);
                for (std::size_t i = next; i + sizeof(sig.pattern) <= imageSize; ++i) {
                    bool matches = true;
                    for (std::size_t j = 0; j < sizeof(sig.pattern); ++j) {
                        if (sig.mask[j] == 'x' && scanStart[i + j] != sig.pattern[j]) {
                            matches = false;
                            break;
                        }
                    }
                    if (matches) {
                        ambiguous = true;
                        break;
                    }
                }
            }
        }
        if (ambiguous) {
            std::ostringstream m;
            m << "MEMORYMAP: " << kHookNames[id]
              << " signature matched more than once - using recorded RVA instead";
            Log(m.str());
        } else {
            return reinterpret_cast<std::uintptr_t>(hit);
        }
    }
    std::ostringstream m;
    m << "MEMORYMAP: " << kHookNames[id] << std::hex
      << " signature miss - falling back to RVA 0x" << sig.fallbackRva
      << " (game update? verify with Reverse/sig_check.py)";
    Log(m.str());
    return base + sig.fallbackRva;
}

// --- PROPTRACE: 属性求值轨迹 (诊断翻滚切納刀的居合斬り属性源) -----------------
// addresses.txt `propTrace=1` 启用。只记本地玩家、结果非零、且该 (kind,id,index)
// 发生 0→非0 跳变的行, 上限 120 行。成功切納刀时出现的 id = 居合斬り真值源。
std::atomic<bool> g_propTrace{false};     // InitMemoryMap 置位
std::atomic<int> g_propTraceLines{0};

void InitMemoryMap() {
    LoadAddressOverrides();
    g_propTrace.store(OverrideOrZero("propTrace") != 0, std::memory_order_relaxed);
    for (int i = 0; i < HkCount; ++i) {
        g_hookAddr[i] = ResolveHookTarget(static_cast<HookId>(i));
    }
    const std::uintptr_t base = ModuleBaseAddr();
    if (const std::uintptr_t v = OverrideOrZero("audioSingleton")) {
        g_addrAudioSingleton = base + v;
    } else {
        g_addrAudioSingleton = base + g_addrAudioSingleton;
    }
    if (const std::uintptr_t v = OverrideOrZero("propRegistry")) {
        g_addrPropRegistry = base + v;
    } else {
        g_addrPropRegistry = base + g_addrPropRegistry;
    }
    if (const std::uintptr_t v = OverrideOrZero("propLookup")) {
        g_addrPropLookup = base + v;
    } else {
        g_addrPropLookup = base + g_addrPropLookup;
    }
    if (const std::uintptr_t v = OverrideOrZero("propName")) {
        g_addrPropName = base + v;
    } else {
        g_addrPropName = base + g_addrPropName;
    }
    if (const std::uintptr_t v = OverrideOrZero("questStateMgr")) {
        g_addrQuestStateMgr = base + v;
    } else {
        g_addrQuestStateMgr = base + g_addrQuestStateMgr;
    }
    if (const std::uintptr_t v = OverrideOrZero("zoneMgr")) {
        g_addrZoneMgr = base + v;
    } else {
        g_addrZoneMgr = base + g_addrZoneMgr;
    }
}

// One-time sanity report: every resolved target must be readable, and the two
// global data structures must still look like themselves. On failure the log
// names the exact anchor to re-derive (see Reverse/ADDRESS_RECOVERY.md).
void ValidateMemoryMap() {
    std::ostringstream d;
    d << "MEMORYMAP doAction "
      << (IsReadable(reinterpret_cast<void*>(g_addrDoAction), 16) ? "ok" : "BAD");
    Log(d.str());
    std::ostringstream m;
    m << "MEMORYMAP hooks:";
    for (int i = 0; i < HkCount; ++i) {
        m << ' ' << kHookNames[i] << '='
          << (IsReadable(reinterpret_cast<void*>(g_hookAddr[i]), 16) ? "ok" : "BAD");
    }
    Log(m.str());

    const char* registryStatus = "BAD";
    if (IsReadable(reinterpret_cast<void*>(g_addrPropRegistry), 0xD0)) {
        const auto count =
            *reinterpret_cast<const std::uint32_t*>(g_addrPropRegistry + 0xB8);
        const auto rowTable =
            *reinterpret_cast<void* const*>(g_addrPropRegistry + 0xC8);
        if (count >= 200 && count <= 2000000 && IsReadable(rowTable, sizeof(void*))) {
            registryStatus = "ok";
        }
        std::ostringstream r;
        r << "MEMORYMAP propRegistry count=" << count << " rowTable="
          << (IsReadable(rowTable, sizeof(void*)) ? "ok" : "BAD") << " -> " << registryStatus;
        Log(r.str());
    } else {
        Log("MEMORYMAP propRegistry UNREADABLE - base+0x5012048 moved?");
    }

    const char* audioStatus = "BAD";
    if (IsReadable(reinterpret_cast<void*>(g_addrAudioSingleton), sizeof(void*))) {
        const auto singleton = *reinterpret_cast<void* const*>(g_addrAudioSingleton);
        audioStatus = singleton ? "ok" : "null";
    }
    std::ostringstream a;
    a << "MEMORYMAP audioSingleton " << audioStatus;
    Log(a.str());
}
