// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L3744-L3845, L5270-L5443, L8745-L8818. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

// --- SHEATHCLSFY 探针: set-3 候选行匹配器 FUN_14026b830 ----------------------
// 诊断翻滚勇気納刀 (SESSIONS 10-31(15)): 记录 翻滚/気刃斬1 子表被咨询时
// 组合匹配的结果, 回答 分类器是否轮到翻滚/组合23是否命中/返回什么。
// addresses.txt `sheathClsfyProbe=1` 启用; 仅在结果变化时记行, 上限 300 行。
using SheathClsfyFn = std::int64_t (*)(void*, void*, std::uint32_t*,
                                       const std::int32_t*, std::uint32_t,
                                       std::uint8_t);
SheathClsfyFn g_originalSheathClsfy = nullptr;
std::atomic<bool> g_sheathClsfyProbe{false};
std::atomic<int> g_sheathClsfyLines{0};

struct SheathSubEntry {
    std::uintptr_t rva;
    int keyA;
    int keyB;
};

constexpr SheathSubEntry kSheathSubs[] = {
    {0x3F4EA50, 18, 19},  // 前滚共享子表
    {0x3F4EC30, 20, -1},
    {0x3F4ED70, 21, -1},
    {0x3F4EEB0, 22, -1},
    {0x3F4F100, 11, -1},  // 気刃斬1 基线 (能切)
};

std::int64_t __fastcall SheathClsfyHook(void* ctx, void* p2,
                                        std::uint32_t* outIdx,
                                        const std::int32_t* rows,
                                        std::uint32_t count,
                                        std::uint8_t flag) {
    const std::int64_t result =
        g_originalSheathClsfy(ctx, p2, outIdx, rows, count, flag);
    if (!g_sheathClsfyProbe.load(std::memory_order_relaxed) || rows == nullptr) {
        return result;
    }
    const std::uintptr_t base = ModuleBaseAddr();
    if (base == 0) {
        return result;
    }
    const std::uintptr_t rowsVa = reinterpret_cast<std::uintptr_t>(rows);
    const SheathSubEntry* sub = nullptr;
    std::size_t subIdx = 0;
    for (std::size_t i = 0; i < std::size(kSheathSubs); ++i) {
        if (rowsVa == base + kSheathSubs[i].rva) {
            sub = &kSheathSubs[i];
            subIdx = i;
            break;
        }
    }
    if (sub == nullptr) {
        return result;
    }
    struct LastState {
        std::int64_t result;
        std::uint32_t idx;
    };
    static LastState last[std::size(kSheathSubs)] = {};
    const std::uint32_t firstIdx = outIdx != nullptr ? *outIdx : 0;
    LastState& st = last[subIdx];
    if (st.result == result && st.idx == firstIdx) {
        return result;
    }
    st.result = result;
    st.idx = firstIdx;
    if (g_sheathClsfyLines.fetch_add(1, std::memory_order_relaxed) > 300) {
        return result;
    }
    std::ostringstream m;
    m << "SHEATHCLSFY key=" << sub->keyA;
    if (sub->keyB >= 0) {
        m << '/' << sub->keyB;
    }
    m << " count=" << count << " flag=" << static_cast<int>(flag) << " -> ";
    if (result != 0) {
        const std::int32_t* row = reinterpret_cast<const std::int32_t*>(result);
        m << "row=(" << row[0] << ',' << row[1] << ',' << row[2] << ','
          << row[3] << ',' << row[4] << ',' << row[5] << ")";
    } else {
        m << "NOMATCH";
    }
    m << " firstIdx=" << firstIdx;
    Log(m.str().c_str());
    return result;
}

bool InstallSheathClsfyProbeHook() {
    g_sheathClsfyProbe.store(OverrideOrZero("sheathClsfyProbe") != 0,
                             std::memory_order_relaxed);
    if (!g_sheathClsfyProbe.load(std::memory_order_relaxed)) {
        return false;
    }
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkSheathClsfy]),
                      reinterpret_cast<void*>(&SheathClsfyHook),
                      reinterpret_cast<void**>(&g_originalSheathClsfy)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkSheathClsfy])) != MH_OK) {
        Log("SHEATHCLSFY probe hook failed");
        g_sheathClsfyProbe.store(false, std::memory_order_relaxed);
        return false;
    }
    Log("SHEATHCLSFY probe hook installed (sheathClsfyProbe=1)");
    return true;
}

#ifndef VALOR_PROBE
// --- 翻滚切勇気納刀 (ROLLSHEATHE poll, 2026-10-31) ---------------------------
// 数据侧(TIML/FSM/分类器表)全灭后改走输入检测: 组合23检查器 = 游戏自己定义的
// "特殊納刀输入" (键盘/手柄同链, 零配置), 挂在分类器 mgrCtx+0xD0 槽表的
// [23*8] 处, vtable+0x30 无参求值, 返回 -1=未按。mgrCtx 由 sheathCap 捕获。
// 窗口 = 当前动作 ∈ 翻滚{18,19,20,21,22} 且 motion 帧 >= 阈值(可配置,
// 默认 = TIML 调参近似值), 命中即经 DoAction 发 {3,98} 勇気納刀。
struct RollSheatheCfg {
    std::atomic<int> enabled{1};
    std::atomic<int> inputMode{0};  // 0=side 1=internal 2=any
    std::map<std::int32_t, float> thresholds{
        {18, 28.0f}, {19, 28.0f}, {20, 34.0f}, {21, 34.0f}, {22, 72.0f},
    };
};
RollSheatheCfg g_rollSheathe;
std::atomic<std::uintptr_t> g_sheathMgrCtx{0};

void LoadRollSheatheConfig() {
    std::ifstream input("nativePC/plugins/ValorGauge.roll-sheathe.txt");
    if (!input) {
        Log("ROLLSHEATHE: no ValorGauge.roll-sheathe.txt - defaults (enable=1 "
            "f18/19=28 f20/21=34 f22=72)");
        return;
    }
    std::string line;
    while (std::getline(input, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const auto key = line.substr(0, eq);
        const auto value = line.substr(eq + 1);
        if (key == "enable") g_rollSheathe.enabled.store(std::atoi(value.c_str()) != 0);
        else if (key == "input") {
            g_rollSheathe.inputMode.store(
                value == "internal" ? 1 : (value == "any" ? 2 : 0),
                std::memory_order_relaxed);
        }
        else {
            try { g_rollSheathe.thresholds[std::stoi(key)] = std::stof(value); } catch (...) {}
        }
    }
    std::ostringstream m;
    m << "ROLLSHEATHE: config loaded (enable=" << g_rollSheathe.enabled.load()
      << " input=" << g_rollSheathe.inputMode.load()
      << " thresholds=";
    for (auto& [id, f] : g_rollSheathe.thresholds) m << id << ':' << f << ' ';
    m << ')';
    Log(m.str().c_str());
}

using SheathCapFn = void (*)(void*, std::int64_t, void*, std::uint32_t);
SheathCapFn g_originalSheathCap = nullptr;

// 多实体各自有分类器 mgrCtx (日志实证 4+ 个交替), 必须按 actionObj 认领玩家的:
// actionObj = *(owner+0xD90)+0x61C8 与 mod controller(player+0x61C8) 同源同偏移。
struct SheathCtxEntry {
    std::uintptr_t mgr;
    std::uintptr_t actionObj;
};
static std::mutex g_sheathCtxMutex;
static SheathCtxEntry g_sheathCtx[8] = {};

void __fastcall SheathCapHook(void* mgrCtx, std::int64_t actionObj, void* p3,
                              std::uint32_t p4) {
    {
        std::scoped_lock lock(g_sheathCtxMutex);
        const auto m = reinterpret_cast<std::uintptr_t>(mgrCtx);
        const auto ao = static_cast<std::uintptr_t>(actionObj);
        SheathCtxEntry* slot = nullptr;
        for (auto& e : g_sheathCtx) {
            if (e.mgr == m) { slot = &e; break; }
            if (e.mgr == 0 && slot == nullptr) slot = &e;
        }
        if (slot == nullptr) slot = &g_sheathCtx[0];   // 满则覆盖最老
        slot->mgr = m;
        slot->actionObj = ao;
    }
    g_originalSheathCap(mgrCtx, actionObj, p3, p4);
}

bool InstallSheathCapHook() {
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkSheathCap]),
                      reinterpret_cast<void*>(&SheathCapHook),
                      reinterpret_cast<void**>(&g_originalSheathCap)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkSheathCap])) != MH_OK) {
        Log("ROLLSHEATHE: sheathCap capture hook failed (mgrCtx unavailable, poll idle)");
        return false;
    }
    Log("ROLLSHEATHE: sheathCap capture hook installed");
    return true;
}

// --- COMBOPROBE v2 (2026-11-02): 组合23 内部输入态观测 ------------------------
// v1 全量扫描教训: 首帧整个检查器数组都算"新对象", 500 行额度一次吃光, 按键
// 窗口零数据。v2 聚焦: 基线一行(c23 检查器字段 + 键位重映射表值) + 逐帧读
// 组合23 逻辑输入态位集, 变化才记 (SHEATHIN23)。
// 本轮静态逆解码(检查器类 vtable 0x143371ea0 → evaluate FUN_1417bfa90):
//   检查器 = mgrCtx type1[23] (mgrCtx+0xD0 槽), +0x28=逻辑输入 id,
//   +0x20=输入上下文; 上下文位集按逻辑 id 直接索引 (FUN_14122fdb0 press /
//   FUN_14122fe60 hold 的读法):
//     +0x780=当前按住  +0x7C0/+0x820=缓冲  +0x7E0=本帧按下沿
//   键位重映射表 RVA 0x3EEABF0: 逻辑动作(<0x89)→物理键, 运行时被键位配置改写
//   (vanilla 表: 逻辑23→物理9); "R+A 还是 R+B" 的答案 = 这张表的运行时值。
//   万能查询 FUN_1419a30d0(玩家上下文, 输入上下文, id, press, mode) 外围有
//   玩家门禁(+0x7658→+0xa4/a5/a6) —— 翻滚中组合23 从不被游戏询问的可能原因,
//   所以轮询直读位集, 不调查询函数。
// addresses.txt `comboProbe=1` 启用; SHEATHEVAL(ba00 钩子)沿用, 共享行数上限。
std::atomic<bool> g_comboProbe{false};
std::atomic<int> g_comboProbeLines{0};

// SheathCapHook 认领表: player 实体 → 分类器 mgrCtx (probe/poll 共用)
static std::uintptr_t FindPlayerMgrCtx(std::uintptr_t player) {
    std::scoped_lock lock(g_sheathCtxMutex);
    for (const auto& e : g_sheathCtx) {
        if (e.mgr != 0 && e.actionObj == player + 0x61C8) return e.mgr;
    }
    return 0;
}

// 组合23 检查器速览: {vtable, +8 旗标, +0x20 输入上下文, +0x28 逻辑id, +0x30 模式}
struct Sheath23View {
    bool valid = false;
    std::uintptr_t vtbl = 0;
    std::uint8_t flag8 = 0;
    std::uint8_t mode30 = 0;
    std::uint32_t inputId = 0;
    const std::uint8_t* state = nullptr;  // 输入上下文基址 (位集 +0x780..+0x820)
};

static bool ReadSheath23(std::uintptr_t mgr, Sheath23View& v) {
    v = Sheath23View{};
    const std::uintptr_t slot = mgr + 0xD0;  // type1 {指针数组,数量} 对
    if (!IsReadable(reinterpret_cast<const void*>(slot), 0x10)) return false;
    void* const* arr = *reinterpret_cast<void* const**>(slot);
    const std::uint32_t count =
        *reinterpret_cast<const volatile std::uint32_t*>(slot + 8);
    if (arr == nullptr || count <= 23) return false;
    void* chk = arr[23];
    if (chk == nullptr || !IsReadable(chk, 0x40)) return false;
    const auto* c = static_cast<const std::uint8_t*>(chk);
    v.vtbl = *reinterpret_cast<std::uintptr_t*>(chk);
    v.flag8 = *reinterpret_cast<const std::uint8_t*>(c + 8);
    v.inputId = *reinterpret_cast<const volatile std::uint32_t*>(c + 0x28);
    v.mode30 = *reinterpret_cast<const std::uint8_t*>(c + 0x30);
    void* ctx = *reinterpret_cast<void* const*>(c + 0x20);
    if (ctx == nullptr || !IsReadable(ctx, 0x830)) return false;
    v.state = static_cast<const std::uint8_t*>(ctx);
    v.valid = true;
    return true;
}

// 位集读: +0x780=按住, +0x7C0=缓冲1, +0x7E0=按下沿, +0x820=缓冲2 (逻辑 id 位索引)
static std::uint32_t SheathStateNibble(const Sheath23View& v) {
    if (!IsReadable(v.state, 0x830)) return 0;
    const std::uintptr_t word = (v.inputId >> 5) * 4;
    const std::uint32_t bit = 1u << (v.inputId & 31);
    const auto test = [&](std::uintptr_t off) {
        return (*reinterpret_cast<const volatile std::uint32_t*>(v.state + off + word) &
                bit) != 0;
    };
    return (test(0x780) ? 1u : 0u) | (test(0x7C0) ? 2u : 0u) |
           (test(0x7E0) ? 4u : 0u) | (test(0x820) ? 8u : 0u);
}

// 组合23 按住位 (内部输入源, 绑定无关)。供 PollRollSheathe 用。
static bool InternalSheathHeld(std::uintptr_t mgr) {
    if (mgr == 0) return false;
    Sheath23View v;
    if (!ReadSheath23(mgr, v) || !v.valid) return false;
    if (!IsReadable(v.state, 0x830)) return false;
    const std::uintptr_t word = (v.inputId >> 5) * 4;
    const std::uint32_t bit = 1u << (v.inputId & 31);
    return (*reinterpret_cast<const volatile std::uint32_t*>(v.state + 0x780 + word) &
            bit) != 0;
}

void PollSheathComboProbe(const void* entity) {
    if (!g_comboProbe.load(std::memory_order_relaxed)) return;
    if (g_comboProbeLines.load(std::memory_order_relaxed) > 400) return;
    const std::uintptr_t player = reinterpret_cast<std::uintptr_t>(entity);
    const std::uintptr_t mgr = FindPlayerMgrCtx(player);
    if (mgr == 0) return;
    static std::uintptr_t baselineMgr = 0;
    static Sheath23View view;
    if (baselineMgr != mgr) {
        if (!ReadSheath23(mgr, view)) return;
        baselineMgr = mgr;
        std::int32_t remapPhys = -2;
        const std::uintptr_t base = ModuleBaseAddr();
        if (base != 0 && view.inputId < 0x89 &&
            IsReadable(reinterpret_cast<const void*>(
                           base + 0x3EEABF0 + view.inputId * 4), 4)) {
            remapPhys = *reinterpret_cast<const volatile std::int32_t*>(
                base + 0x3EEABF0 + view.inputId * 4);
        }
        std::ostringstream m;
        m << "SHEATHIN23: mgr=0x" << std::hex << mgr << " checker vtbl=0x"
          << view.vtbl << " flag=0x" << static_cast<int>(view.flag8)
          << " inputId=" << std::dec << view.inputId
          << " mode30=" << static_cast<int>(view.mode30)
          << " remapPhys=" << remapPhys << " ctx=0x" << std::hex
          << reinterpret_cast<std::uintptr_t>(view.state);
        Log(m.str().c_str());
    }
    if (!view.valid) return;
    const std::uint32_t nib = SheathStateNibble(view);
    static std::uint32_t lastNib = 0xFFFFFFFFu;
    if (nib == lastNib) return;
    lastNib = nib;
    if (g_comboProbeLines.fetch_add(1, std::memory_order_relaxed) > 400) return;
    std::ostringstream m;
    m << "SHEATHIN23 h=" << (nib & 1) << " buf7c0=" << ((nib >> 1) & 1)
      << " edge=" << ((nib >> 2) & 1) << " buf820=" << ((nib >> 3) & 1);
    Log(m.str().c_str());
}

// ba00 钩子: (mgrCtx, stateCtx, row, flag) -> int32 (-1 = 组合不可用/未中)
using SheathEvalProbeFn =
    std::int64_t (*)(void*, void*, const std::int32_t*, std::uint32_t);
SheathEvalProbeFn g_originalSheathEvalProbe = nullptr;

struct SheathEvalLast {
    std::int32_t result;
    std::uint32_t flag;
    bool valid;
};

std::int64_t __fastcall SheathEvalProbeHook(void* mgrCtx, void* stateCtx,
                                            const std::int32_t* row,
                                            std::uint32_t flag) {
    const std::int64_t raw =
        g_originalSheathEvalProbe(mgrCtx, stateCtx, row, flag);
    const std::int32_t result = static_cast<std::int32_t>(raw & 0xFFFFFFFFu);
    if (!g_comboProbe.load(std::memory_order_relaxed) || row == nullptr) {
        return raw;
    }
    if (g_comboProbeLines.load(std::memory_order_relaxed) > 500) return raw;
    const void* ctrl = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        ctrl = g_lockedPlayerController;
    }
    if (ctrl != nullptr) {
        bool mine = false;
        {
            std::scoped_lock lock(g_sheathCtxMutex);
            for (const auto& e : g_sheathCtx) {
                if (e.mgr == reinterpret_cast<std::uintptr_t>(mgrCtx) &&
                    e.actionObj == reinterpret_cast<std::uintptr_t>(ctrl)) {
                    mine = true;
                    break;
                }
            }
        }
        if (!mine) return raw;
    }
    static SheathEvalLast last[512] = {};
    const std::int32_t combo = row[1];
    if (combo < 0 || combo >= 512) return raw;
    if (!((combo == 23) || (result != -1))) return raw;
    SheathEvalLast& l = last[combo];
    if (l.valid && l.result == result && l.flag == flag) return raw;
    l = {result, flag, true};
    if (g_comboProbeLines.fetch_add(1, std::memory_order_relaxed) > 500) return raw;
    std::ostringstream m;
    m << "SHEATHEVAL c" << combo << " row=(" << row[0] << ',' << row[1] << ','
      << row[2] << ',' << row[3] << ',' << row[4] << ',' << row[5]
      << ") fin=" << flag << " -> " << result;
    Log(m.str().c_str());
    return raw;
}

bool InstallSheathEvalProbe() {
    g_comboProbe.store(OverrideOrZero("comboProbe") != 0,
                       std::memory_order_relaxed);
    if (!g_comboProbe.load(std::memory_order_relaxed)) return false;
    if (MH_CreateHook(reinterpret_cast<void*>(g_hookAddr[HkSheathEval]),
                      reinterpret_cast<void*>(&SheathEvalProbeHook),
                      reinterpret_cast<void**>(&g_originalSheathEvalProbe)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(g_hookAddr[HkSheathEval])) != MH_OK) {
        Log("COMBOPROBE: sheathEval hook failed (passive scan only)");
    } else {
        Log("COMBOPROBE: sheathEval hook installed (comboProbe=1)");
    }
    Log("COMBOPROBE: SHEATHIN23 watcher active (baseline + input-state "
        "transitions, cap 400 lines)");
    return true;
}

using SheathEvalFn = std::uint32_t (*)(void*);

// 求值异常详情: SEH filter 记录异常码+地址 (前 5 次)
static std::atomic<int> g_sheathEvalFaults{0};
static long SheathEvalFilter(void* checker, EXCEPTION_POINTERS* ep) {
    const int n = g_sheathEvalFaults.fetch_add(1, std::memory_order_relaxed);
    if (n < 5 && ep && ep->ExceptionRecord) {
        std::ostringstream m;
        m << "ROLLSHEATHE: evaluate fault #" << n << " code=0x" << std::hex
          << ep->ExceptionRecord->ExceptionCode << " addr=0x"
          << reinterpret_cast<std::uintptr_t>(ep->ExceptionRecord->ExceptionAddress)
          << " checker=0x" << reinterpret_cast<std::uintptr_t>(checker);
        Log(m.str().c_str());
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

// 求值隔离函数: SEH 兜底, 求值崩溃(未武装态内部状态未初始化等)降级为未按
static std::uint32_t __stdcall SheathEvalSafe(void* checker) {
    __try {
        const auto evalFn = *reinterpret_cast<SheathEvalFn*>(static_cast<void**>(checker) + 6);
        if (evalFn == nullptr) return 0xFFFFFFFFu;
        return evalFn(checker);
    } __except (SheathEvalFilter(checker, GetExceptionInformation())) {
        return 0xFFFFFFFFu;
    }
}

// 每 tick 轮询 (PropIntHook = 游戏更新线程)。返回 -1 之外 = 组合23成立。
void PollRollSheathe(const void* entity) {
    if (!g_rollSheathe.enabled.load(std::memory_order_relaxed)) return;
    if (g_insideDoAction.load(std::memory_order_acquire)) return;
    void* ctrl = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        ctrl = g_lockedPlayerController;
    }
    if (!ctrl) return;
    if (!ModWeaponActive()) return;
    const auto* player = static_cast<const std::uint8_t*>(ctrl) - 0x61C8;
    if (entity != static_cast<const void*>(player)) return;

    const std::int32_t set =
        *reinterpret_cast<const volatile std::int32_t*>(static_cast<const std::uint8_t*>(ctrl) + 0xAC);
    const std::int32_t id =
        *reinterpret_cast<const volatile std::int32_t*>(static_cast<const std::uint8_t*>(ctrl) + 0xB0);
    if (set != 3) return;
    const auto thIt = g_rollSheathe.thresholds.find(id);
    if (thIt == g_rollSheathe.thresholds.end()) return;

    // 输入源 (roll-sheathe.txt `input=`): side=鼠标侧键(过渡版) /
    // internal=游戏组合23逻辑输入态(+0x780 按住位, 绑定无关) / any=两者并集
    // (内部源 A/B 验证期)。读法 = FindPlayerMgrCtx → mgrCtx type1[23] 检查器
    // → +0x20 输入上下文 → +0x780 位集 [inputId] 位 (COMBOPROBE 节注释)。
    bool held = false;
    char src = '-';
    const int inputMode = g_rollSheathe.inputMode.load(std::memory_order_relaxed);
    if (inputMode >= 1 &&
        InternalSheathHeld(FindPlayerMgrCtx(reinterpret_cast<std::uintptr_t>(player)))) {
        held = true;
        src = 'i';
    }
    if (!held && inputMode != 1) {
        const short x1 = GetAsyncKeyState(VK_XBUTTON1);
        const short x2 = GetAsyncKeyState(VK_XBUTTON2);
        if ((x1 & 0x8000) != 0 || (x2 & 0x8000) != 0) {
            held = true;
            src = 's';
        }
    }
    if (!held) return;

    // motion 帧 (anim+0x10C)
    if (!IsReadable(player + 0x468, sizeof(void*))) return;
    void* anim = *reinterpret_cast<void* const*>(player + 0x468);
    if (anim == nullptr || !IsReadable(static_cast<std::uint8_t*>(anim) + 0x10C, 4)) return;
    const float mot =
        *reinterpret_cast<volatile float*>(static_cast<std::uint8_t*>(anim) + 0x10C);
    if (mot < thIt->second) return;

    // 一次翻滚一发: 帧回卷 = 新一次翻滚
    static bool fired = false;
    static float lastMot = -1.0f;
    if (mot < lastMot - 5.0f) fired = false;
    lastMot = mot;
    if (fired) return;

    ActionInfo force{};
    force.actionSet = 3;
    force.actionId = 98;
    const bool ok = g_originalDoAction(ctrl, &force) != 0;
    std::ostringstream m;
    m << "SHEATHCUT[" << src << "]: roll " << id << " f" << mot
      << " -> DoAction(3:98) " << (ok ? "accepted" : "rejected");
    Log(m.str().c_str());
    if (ok) {
        fired = true;
        std::scoped_lock lock(g_actionMutex);
        ActionInfo forced{};
        forced.actionSet = 3;
        forced.actionId = 98;
        g_lastLongSwordAction = forced;
    }
}

#endif // !VALOR_PROBE
// LS set-3 请求分类器表: 勇気納刀切入(vanilla 特殊納刀)的每动作候选行。
// 带 (1,23,1,0,3,98) 行的 51 键=全部可切纳刀动作(含 key11=気刃斬1/LMT104);
// 翻滚 18-22 没有 → TIML/FSM 无法打通(SESSIONS 10-31(14))。此处在进程内把
// 同一行写进翻滚子表的死行/休眠行, 须在 FSM 管理器把静态表拷进 mgrCtx 之前执行。
// 行布局 {type,输入组合id,匹配a,匹配b,set,目标id}×0x18; count 由游戏运行时回填,
// 不动。key18/19 共享子表, row13 原是 (1,408,0,0,-1,-1) 占位行(靠 -1 目标连禁
// row14), 故 row14 同步自禁用以保持 vanilla 刻意关掉的スリF。
struct RollSheatheRowPatch {
    std::uintptr_t rva;
    std::int32_t oldRow[6];
    std::int32_t newRow[6];
};

void PatchRollSheatheRows() {
    // addresses.txt 里 rollSheathePatch=0 可整体停用(缺省开)
    if (g_addrOverrides.count("rollSheathePatch") != 0 &&
        OverrideOrZero("rollSheathePatch") == 0) {
        Log("ROLLSHEATHE: disabled via addresses.txt (rollSheathePatch=0)");
        return;
    }
    constexpr std::uintptr_t kImageBase = 0x140000000;
    constexpr RollSheatheRowPatch kPatches[] = {
        {0x3F4EB88, {1, 408, 0, 0, -1, -1}, {1, 23, 1, 0, 3, 98}},   // key18/19 r13
        {0x3F4EBA0, {1, 288, 1, 0, 3, 66}, {1, 288, 0, 0, -1, -1}},  // key18/19 r14 自禁用
        {0x3F4ED38, {1, 278, 0, 1, 1, 532}, {1, 23, 1, 0, 3, 98}},   // key20 r11
        {0x3F4EE78, {1, 278, 0, 1, 1, 532}, {1, 23, 1, 0, 3, 98}},   // key21 r11
        {0x3F4EFB8, {1, 278, 0, 1, 1, 532}, {1, 23, 1, 0, 3, 98}},   // key22 r11
    };
    // 锚点: key11(気刃斬1) 的原版纳刀行, 字节不符=游戏更新/布局漂移, 整体放弃
    const std::uintptr_t base = ModuleBaseAddr();
    if (base == 0) {
        Log("ROLLSHEATHE: module base unavailable - skipped");
        return;
    }
    const auto anchor = base + (0x3F4F1F0);
    const std::int32_t kAnchorExpected[6] = {1, 23, 1, 0, 3, 98};
    if (!IsReadable(reinterpret_cast<const void*>(anchor), sizeof(kAnchorExpected)) ||
        std::memcmp(reinterpret_cast<const void*>(anchor), kAnchorExpected,
                    sizeof(kAnchorExpected)) != 0) {
        Log("ROLLSHEATHE: anchor mismatch (game update?) - skipped");
        return;
    }
    int patched = 0;
    for (const auto& p : kPatches) {
        const auto addr = base + p.rva;
        if (!IsReadable(reinterpret_cast<const void*>(addr), sizeof(p.oldRow))) {
            std::ostringstream m;
            m << "ROLLSHEATHE: row @0x" << std::hex << p.rva << " unreadable - skipped";
            Log(m.str().c_str());
            continue;
        }
        if (std::memcmp(reinterpret_cast<const void*>(addr), p.oldRow,
                        sizeof(p.oldRow)) != 0) {
            std::ostringstream m;
            m << "ROLLSHEATHE: row @0x" << std::hex << p.rva
              << " unexpected bytes - skipped";
            Log(m.str().c_str());
            continue;
        }
        std::memcpy(reinterpret_cast<void*>(addr), p.newRow, sizeof(p.newRow));
        if (std::memcmp(reinterpret_cast<const void*>(addr), p.newRow,
                        sizeof(p.newRow)) != 0) {
            std::ostringstream m;
            m << "ROLLSHEATHE: row @0x" << std::hex << p.rva << " readback FAILED";
            Log(m.str().c_str());
            continue;
        }
        ++patched;
    }
    std::ostringstream m;
    m << "ROLLSHEATHE: " << patched << "/" << std::dec << std::size(kPatches)
      << " classifier rows patched (roll 18-22 sheathe candidate)";
    Log(m.str().c_str());
}
