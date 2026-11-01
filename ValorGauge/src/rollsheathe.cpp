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
        else {
            try { g_rollSheathe.thresholds[std::stoi(key)] = std::stof(value); } catch (...) {}
        }
    }
    std::ostringstream m;
    m << "ROLLSHEATHE: config loaded (enable=" << g_rollSheathe.enabled.load()
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

// --- COMBOPROBE (2026-11-02): 分类器检查器数组被动观测 + ba00 评估日志 ---------
// 动机: 特殊納刀实体键因绑定而异(user 情报 R+A/B 因人而异) → combo 走逻辑动作
// 层, 物理键不可枚举, GetAsyncKeyState 是死路。目标 = 游戏内部"特殊納刀输入态"。
// 静态已知(反编译): mgrCtx type 槽 = {指针数组,数量} 对, ptr=+0xC0+type*0x10,
// count=+0xC8+type*0x10 (FUN_14026b380 越界检查); 检查器 = ptr[comboId],
// +8 旗标字节 bit1=本帧不可用(ba00 见 1 即弃), bit0 由 ba00 按 row.a 写;
// 评估 = vtable+0x30, 候选胜出时对前方可用候选调 vtable+0x38 (FUN_14026b380)。
// addresses.txt `comboProbe=1` 启用。两路信号(只记变化, 共享 500 行上限):
//   COMBOPROBE/COMBOCHK = 逐帧扫四个 type 槽, 检查器 {obj,vtbl,+8 旗标u32} 变化
//   SHEATHEVAL          = ba00 钩子, 游戏"问"某组合的行+评估结果(组合23或非-1)
std::atomic<bool> g_comboProbe{false};
std::atomic<int> g_comboProbeLines{0};

struct ComboChkLast {
    void* obj;
    std::uintptr_t vtbl;
    std::uint32_t flag;
};
static ComboChkLast g_comboChkLast[4][1024] = {};
static bool g_comboBaselineDone = false;

// 扫描体放 callee: __try 函数内不得有带析构的 C++ 对象 (C2712)。
static void ComboProbeScan(std::uintptr_t mgr) {
    for (int type = 0; type < 4; ++type) {
        const std::uintptr_t slot = mgr + 0xC0 + type * 0x10;
        if (!IsReadable(reinterpret_cast<const void*>(slot), 0x10)) continue;
        void* const* arr = *reinterpret_cast<void* const**>(slot);
        const std::uint32_t count =
            *reinterpret_cast<const volatile std::uint32_t*>(slot + 8);
        if (arr == nullptr || count == 0 || count > 1024) continue;
        if (!IsReadable(reinterpret_cast<const void*>(arr),
                        static_cast<std::size_t>(count) * sizeof(void*))) {
            continue;
        }
        for (std::uint32_t i = 0; i < count; ++i) {
            void* obj = arr[i];
            ComboChkLast& l = g_comboChkLast[type][i];
            if (obj == nullptr) {
                if (l.obj != nullptr) {
                    l = {};
                    if (g_comboProbeLines.fetch_add(1, std::memory_order_relaxed) <= 500) {
                        std::ostringstream m;
                        m << "COMBOCHK t" << type << " c" << i << " cleared";
                        Log(m.str().c_str());
                    }
                }
                continue;
            }
            if (obj == l.obj) {
                // 快路径: 对象未换只读旗标 u32 (+8); 游戏托管内存, 换对象才复查
                const std::uint32_t flag = *reinterpret_cast<const std::uint32_t*>(
                    static_cast<const std::uint8_t*>(obj) + 8);
                if (flag == l.flag) continue;
                l.flag = flag;
            } else {
                if (!IsReadable(obj, 0x10)) continue;
                l.obj = obj;
                l.vtbl = *reinterpret_cast<std::uintptr_t*>(obj);
                l.flag = *reinterpret_cast<const std::uint32_t*>(
                    static_cast<const std::uint8_t*>(obj) + 8);
            }
            if (g_comboProbeLines.fetch_add(1, std::memory_order_relaxed) > 500) return;
            std::ostringstream m;
            m << "COMBOCHK t" << type << " c" << i << " obj=0x" << std::hex
              << reinterpret_cast<std::uintptr_t>(obj) << " vtbl=0x" << l.vtbl
              << " flag=0x" << l.flag << std::dec;
            Log(m.str().c_str());
        }
    }
}

static long ComboProbeFilter(std::uintptr_t mgr, EXCEPTION_POINTERS* ep) {
    const int n = g_comboProbeLines.fetch_add(1, std::memory_order_relaxed);
    if (n < 530 && ep && ep->ExceptionRecord) {  // 超 500 也要能看见崩溃行
        std::ostringstream m;
        m << "COMBOPROBE: scan fault code=0x" << std::hex
          << ep->ExceptionRecord->ExceptionCode << " mgr=0x" << mgr;
        Log(m.str().c_str());
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

// SEH 层独立成函数: __try 与带析构对象同函数 = C2712。
static void ComboProbeScanSafe(std::uintptr_t mgr) {
    __try {
        ComboProbeScan(mgr);
    } __except (ComboProbeFilter(mgr, GetExceptionInformation())) {
        g_comboProbe.store(false, std::memory_order_relaxed);  // 崩一次整体停
    }
}

void PollSheathComboProbe(const void* entity) {
    if (!g_comboProbe.load(std::memory_order_relaxed)) return;
    if (g_comboProbeLines.load(std::memory_order_relaxed) > 500) return;
    const std::uintptr_t player = reinterpret_cast<std::uintptr_t>(entity);
    std::uintptr_t mgr = 0;
    {
        std::scoped_lock lock(g_sheathCtxMutex);
        for (const auto& e : g_sheathCtx) {
            if (e.mgr != 0 && e.actionObj == player + 0x61C8) { mgr = e.mgr; break; }
        }
    }
    if (mgr == 0) return;
    if (!g_comboBaselineDone) {
        g_comboBaselineDone = true;
        std::ostringstream m;
        m << "COMBOPROBE: mgr=0x" << std::hex << mgr << std::dec;
        for (int type = 0; type < 4; ++type) {
            const std::uintptr_t slot = mgr + 0xC0 + type * 0x10;
            const std::uint32_t count =
                IsReadable(reinterpret_cast<const void*>(slot), 0x10)
                    ? *reinterpret_cast<const volatile std::uint32_t*>(slot + 8)
                    : 0;
            m << " t" << type << "n=" << count;
        }
        Log(m.str().c_str());
    }
    ComboProbeScanSafe(mgr);
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
    Log("COMBOPROBE: passive checker-array watcher active - COMBOCHK=state "
        "changes, SHEATHEVAL=game queries (cap 500 lines)");
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

    // 输入: 鼠标侧键 (VK_XBUTTON1/2) 任一按住 = 特殊納刀意图。
    // 游戏内部读键的路线(检查器/属性/分类器)已判死, 物理侧键是确定性信号。
    const short x1 = GetAsyncKeyState(VK_XBUTTON1);
    const short x2 = GetAsyncKeyState(VK_XBUTTON2);
    const bool held = (x1 & 0x8000) != 0 || (x2 & 0x8000) != 0;
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
    m << "SHEATHCUT: roll " << id << " f" << mot << " -> DoAction(3:98) "
      << (ok ? "accepted" : "rejected");
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
