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

// --- SHEATHGATE (11-02, user 定案): 纳刀触发后 0.5s 拒绝気刃斬1/刚一族请求 -----
// 分类器行禁用追不平: REQTRACE 实锤分类器(b590, callerRVA=0x26b640)每 ~1.6s
// 反复发 3:112(=52 行 3:11 经 SheathRewrite 转换), FSM 链接边/缓冲输入也绕过
// 分类器行。DoAction 总闸一刀切: 窗口 = SHEATHCUT 落地 或 动作层观察到 3:98
// 起 gateMs(默认500)ms, 窗口内拒绝 block 表(默认 1,11,12,66,112), 98 本体放行。
// 注意: 定义须在 LoadRollSheatheConfig 之前(其 block=/gateMs= 解析要写这些)。
std::atomic<std::int64_t> g_sheathGateUntilMs{0};
std::atomic<int> g_sheathGateMs{3000};  // user: 3s 已够绝大多数动作切换
// 纳刀意图锁存: 翻滚中见输入位(即按即消费, 闪一帧)即锁存 gateMs 时长,
// PollRollSheathe 在动作帧到阈值时凭锁存补发 {3,98} — 治"按早了起身站着不动"
// (输入早被吃, 阈值帧时已无信号; user 定案 11-02: 缓存到气刃斬1 时机=起身点)。
std::atomic<std::int64_t> g_sheathIntentUntilMs{0};
// 纳刀族 combo 表 (intentCombos=, 分类器评估命中即锁存意图 — 竞速免疫:
// 分类器同步亲见输入, 免除 位闪现vs轮询 的消费竞速; 局内实证 c23=快捷键,
// c52/c144=纳刀 combo 在翻滚子表的行)。288=R 不入表。
std::vector<int> g_sheathIntentCombos = {23, 52, 144};
// 仅配置加载期写入, 之后只读(钩子线程并发读安全)
std::vector<int> g_sheathGateBlock = {1, 11, 12, 66, 112};

std::int64_t SheathNowMs() {
    return static_cast<std::int64_t>(GetTickCount64());
}

void SheathGateArm() {
    g_sheathGateUntilMs.store(
        SheathNowMs() + g_sheathGateMs.load(std::memory_order_relaxed),
        std::memory_order_release);
}

bool SheathGateShouldBlock(int id) {
    if (SheathNowMs() > g_sheathGateUntilMs.load(std::memory_order_acquire)) {
        return false;
    }
    if (id == 98) return false;  // 纳刀本体永远放行
    for (const int b : g_sheathGateBlock) {
        if (b == id) return true;
    }
    return false;
}

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
        else if (key == "gateMs") {
            g_sheathGateMs.store(std::atoi(value.c_str()), std::memory_order_relaxed);
        }
        else if (key == "block") {
            g_sheathGateBlock.clear();
            std::size_t pos = 0;
            while (pos < value.size()) {
                const std::size_t comma = value.find(',', pos);
                const std::string tok = comma == std::string::npos
                    ? value.substr(pos) : value.substr(pos, comma - pos);
                try { g_sheathGateBlock.push_back(std::stoi(tok)); } catch (...) {}
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
        }
        else if (key == "intentCombos") {
            g_sheathIntentCombos.clear();
            std::size_t pos = 0;
            while (pos < value.size()) {
                const std::size_t comma = value.find(',', pos);
                const std::string tok = comma == std::string::npos
                    ? value.substr(pos) : value.substr(pos, comma - pos);
                try { g_sheathIntentCombos.push_back(std::stoi(tok)); } catch (...) {}
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
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

static void CaptureSheath23(std::uintptr_t mgr);  // COMBOPROBE 节, 调用期捕获

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
    // mgrCtx 只在本次调用期有效 (11-02 崩溃复盘): 同步捕获 (输入上下文, 逻辑id),
    // 只认玩家 actionObj 的调用; 跨调用保留指针再读 = UAF。
    {
        std::scoped_lock lock(g_actionMutex);
        if (g_lockedPlayerController != nullptr &&
            static_cast<std::uintptr_t>(actionObj) ==
                reinterpret_cast<std::uintptr_t>(g_lockedPlayerController)) {
            CaptureSheath23(reinterpret_cast<std::uintptr_t>(mgrCtx));
        }
    }
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

// --- COMBOPROBE v3 (2026-11-02): 输入上下文调用期捕获 -------------------------
// 崩溃复盘(v2): c0f0 的 param_1 (mgrCtx) 是每次调用不同的短命上下文(局内实证
// 1 秒内 8+ 个交替, 部分 vtb l=0 全零), 跨调用保留指针再读 = UAF, IsReadable
// 挡不住已提交却已释放的堆页 → 未映射读 → 崩。v1 全量扫描没崩只是运气。
// 铁律: **mgrCtx 只在钩子调用期内有效**; 跨调用只保留 (输入上下文, 逻辑id) 值对,
// 且同一对反复出现(seen>=8)才可信; 一切保留指针的读走 SEH。
// 已知语义(静态): 检查器+0x20=输入上下文, +0x28=逻辑输入 id(combo id ≠ 输入 id,
// 局内实证 slot23 检查器 id=24); 上下文位集按逻辑 id 索引:
//   +0x780=当前按住, +0x7C0/+0x820=缓冲, +0x7E0=本帧按下沿
//   (FUN_14122fdb0 press / FUN_14122fe60 hold 的读法);
//   键位重映射表 RVA 0x3EEABF0: 逻辑动作(<0x89)→物理键, 运行时被键位配置改写。
std::atomic<bool> g_comboProbe{false};
std::atomic<int> g_comboProbeLines{0};

static std::mutex g_capMutex;
// (ctx, id) -> 出现次数; 键对为主, 值对为辅, 上限 8 防脏数据滚雪球
static std::map<std::pair<std::uintptr_t, std::uint32_t>, int> g_capCounts;

// SheathCapHook 内调用: 参数只在本次调用期有效, 同步提取 (输入上下文, 逻辑id)。
static void CaptureSheath23(std::uintptr_t mgr) {
    const std::uintptr_t slot = mgr + 0xD0;  // type1 {指针数组,数量} 对
    if (!IsReadable(reinterpret_cast<const void*>(slot), 0x10)) return;
    void* const* arr = *reinterpret_cast<void* const**>(slot);
    const std::uint32_t count =
        *reinterpret_cast<const volatile std::uint32_t*>(slot + 8);
    if (arr == nullptr || count <= 23 ||
        !IsReadable(reinterpret_cast<const void*>(arr), 24 * sizeof(void*))) {
        return;
    }
    void* chk = arr[23];
    if (chk == nullptr || !IsReadable(chk, 0x40)) return;
    const auto* c = static_cast<const std::uint8_t*>(chk);
    void* ctx = *reinterpret_cast<void* const*>(c + 0x20);
    const std::uint32_t id =
        *reinterpret_cast<const volatile std::uint32_t*>(c + 0x28);
    if (ctx == nullptr || id >= 0x400 ||
        !IsReadable(ctx, 0x830)) {
        return;
    }
    std::scoped_lock lock(g_capMutex);
    auto& n = g_capCounts[{reinterpret_cast<std::uintptr_t>(ctx), id}];
    n = n < 100000 ? n + 1 : n;
    if (g_capCounts.size() > 8) {
        auto victim = g_capCounts.begin();
        for (auto it = g_capCounts.begin(); it != g_capCounts.end(); ++it) {
            if (it->second < victim->second) victim = it;
        }
        g_capCounts.erase(victim);
    }
}

// SEH 位集读 (无 C++ 对象; ctx 为保留指针, 读故障按无效帧处理)
static std::uint32_t SheathNibbleSEH(std::uintptr_t ctx, std::uint32_t id) {
    __try {
        const auto* base = reinterpret_cast<const std::uint8_t*>(ctx);
        const std::uintptr_t word = (id >> 5) * 4;
        const std::uint32_t bit = 1u << (id & 31);
        std::uint32_t nib = 0;
        if (*reinterpret_cast<const volatile std::uint32_t*>(base + 0x780 + word) & bit) nib |= 1;
        if (*reinterpret_cast<const volatile std::uint32_t*>(base + 0x7C0 + word) & bit) nib |= 2;
        if (*reinterpret_cast<const volatile std::uint32_t*>(base + 0x7E0 + word) & bit) nib |= 4;
        if (*reinterpret_cast<const volatile std::uint32_t*>(base + 0x820 + word) & bit) nib |= 8;
        return nib;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0xFFFFFFFFu;
    }
}

// 稳定 (ctx,id) 对: 出现次数最高且 seen>=8 才算可信。probe/poll 共用。
static bool StableSheathPair(std::uintptr_t& ctx, std::uint32_t& id) {
    std::scoped_lock lock(g_capMutex);
    auto best = g_capCounts.end();
    for (auto it = g_capCounts.begin(); it != g_capCounts.end(); ++it) {
        if (it->second < 8) continue;
        if (best == g_capCounts.end() || it->second > best->second) best = it;
    }
    if (best == g_capCounts.end()) return false;
    ctx = best->first.first;
    id = best->first.second;
    return true;
}

// 组合23 按住位 (内部输入源, 绑定无关)。供 PollRollSheathe 用。
static bool InternalSheathHeld() {
    std::uintptr_t ctx = 0;
    std::uint32_t id = 0;
    if (!StableSheathPair(ctx, id)) return false;
    return (SheathNibbleSEH(ctx, id) & 1u) != 0;
}

// 滑窗重臂 (PropIntHook 每帧, 独立于 comboProbe): 纳刀输入位(按住/缓冲/边沿
// 任一)每被看到一次就把闸门窗口再延 gateMs。治本: 局内实证 98→6→5 收刀链成功
// 后 1-3s, 仍挂着的纳刀输入在后续翻滚出招检查点被分类器反复消费成 3:11(刚一),
// 固定 0.5s 窗口盖不住; 松键 0.5s 后闸门自然关闭, 拔刀刚一(收刀态 R2)不受影响。
void PollSheathGateRefresh(const void* entity) {
    const void* ctrl = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        ctrl = g_lockedPlayerController;
    }
    if (ctrl == nullptr) return;
    if (entity != static_cast<const void*>(
                      static_cast<const std::uint8_t*>(ctrl) - 0x61C8)) {
        return;
    }
    std::uintptr_t ctx = 0;
    std::uint32_t id = 0;
    if (!StableSheathPair(ctx, id)) return;
    const std::uint32_t nib = SheathNibbleSEH(ctx, id);
    if (nib != 0 && nib != 0xFFFFFFFFu) {
        SheathGateArm();
        // 纳刀意图锁存: 仅当前动作是翻滚(阈值表内)时记, 供阈值帧补发
        if (ctrl != nullptr) {
            const std::int32_t set = *reinterpret_cast<const volatile std::int32_t*>(
                static_cast<const std::uint8_t*>(ctrl) + 0xAC);
            const std::int32_t rid = *reinterpret_cast<const volatile std::int32_t*>(
                static_cast<const std::uint8_t*>(ctrl) + 0xB0);
            if (set == 3 && g_rollSheathe.thresholds.count(rid) != 0) {
                const std::int64_t nowMs = SheathNowMs();
                const bool was = nowMs < g_sheathIntentUntilMs.load(
                                         std::memory_order_acquire);
                g_sheathIntentUntilMs.store(
                    nowMs + g_sheathGateMs.load(std::memory_order_relaxed),
                    std::memory_order_release);
                if (!was) {
                    static std::atomic<int> g_intentLogs{0};
                    if (g_intentLogs.fetch_add(1, std::memory_order_relaxed) < 20) {
                        std::ostringstream m;
                        m << "SHEATHINTENT armed roll=" << rid;
                        Log(m.str().c_str());
                    }
                }
            }
        }
    }
    // 起身补发 (user 定案 "起身时能纳刀"): 意图存续中, 翻滚动作结束(离开阈值表)
    // 而纳刀链尚未开始 => 立即补发 {3,98}。盖住 阈值帧未到翻滚已结束 的漏缝
    // (后摇末段按压/翻滚被取消, 局内实证 18:00:12 意图过期 = 观感"站着不动")。
    if (ctrl != nullptr) {
        static std::int32_t s_prevId = -999;
        static bool s_getupFired = false;
        const std::int32_t curSet =
            *reinterpret_cast<const volatile std::int32_t*>(
                static_cast<const std::uint8_t*>(ctrl) + 0xAC);
        const std::int32_t curId =
            *reinterpret_cast<const volatile std::int32_t*>(
                static_cast<const std::uint8_t*>(ctrl) + 0xB0);
        const bool intentActive =
            SheathNowMs() < g_sheathIntentUntilMs.load(std::memory_order_acquire);
        if (!intentActive) {
            s_getupFired = false;
        }
        const bool curIsRoll =
            curSet == 3 && g_rollSheathe.thresholds.count(curId) != 0;
        const bool prevIsRoll = g_rollSheathe.thresholds.count(s_prevId) != 0;
        const bool alreadySheathing = curSet == 3 &&
            (curId == 98 || curId == 5 || curId == 6 || curId == 9 || curId == 10);
        if (intentActive && prevIsRoll && !curIsRoll && !alreadySheathing &&
            !s_getupFired && !g_insideDoAction.load(std::memory_order_acquire)) {
            s_getupFired = true;
            ActionInfo force{};
            force.actionSet = 3;
            force.actionId = 98;
            const bool ok = g_originalDoAction(const_cast<void*>(ctrl),
                                               &force) != 0;
            std::ostringstream m;
            m << "SHEATHGETUP fire (prev=" << s_prevId << " cur=" << curId
              << ") -> " << (ok ? "accepted" : "rejected");
            Log(m.str().c_str());
            if (ok) {
                SheathGateArm();
                std::scoped_lock lock(g_actionMutex);
                ActionInfo forced{};
                forced.actionSet = 3;
                forced.actionId = 98;
                g_lastLongSwordAction = forced;
            }
        }
        if (curSet == 3) {
            s_prevId = curId;
        } else {
            s_prevId = -999;
        }
    }
}

// UI 状态查询 (overlay 每帧): 稳定对未收敛返回 false。nib 位 = h1|buf7c02|e4|buf8208。
bool SheathGateStatus(int* outId, std::uint32_t* outNib,
                      std::int64_t* outRemainMs) {
    std::uintptr_t ctx = 0;
    std::uint32_t id = 0;
    if (!StableSheathPair(ctx, id)) return false;
    if (outId != nullptr) *outId = static_cast<int>(id);
    if (outNib != nullptr) *outNib = SheathNibbleSEH(ctx, id);
    if (outRemainMs != nullptr) {
        *outRemainMs =
            g_sheathGateUntilMs.load(std::memory_order_acquire) - SheathNowMs();
    }
    return true;
}

// --- INPUTBITS 扫描 (11-02, user 提案: 监测 R 按住类条件) ----------------------
// 动机: user 猜想纳刀键按下后"游戏觉得按的是 R"。R 按住条件的底层 = 同一输入
// 上下文位集(逻辑 id 索引, 可查询空间 id<0x89), 全扫 +0x780(按住)/+0x7E0(边沿)
// 137 位, 变化记行 → 一次按键测试即建 [实体键→逻辑id] 映射, R 的 id 一眼可判;
// UI 同步显示按住位列表。comboProbe 门控。
static std::atomic<std::uint32_t> g_inHeld[5] = {};
static std::atomic<std::uint32_t> g_inEdge[5] = {};
static std::atomic<bool> g_inValid{false};

// SEH 批量读: 两阵列各 5 字 (id 0..159 覆盖, 4 号字按 0x1FF 截到 id<0x89)
static bool SheathScanBitsSEH(std::uintptr_t ctx, std::uint32_t* held,
                              std::uint32_t* edge) {
    __try {
        const auto* base = reinterpret_cast<const std::uint8_t*>(ctx);
        for (int i = 0; i < 5; ++i) {
            held[i] = *reinterpret_cast<const volatile std::uint32_t*>(base +
                                                                       0x780 + i * 4);
            edge[i] = *reinterpret_cast<const volatile std::uint32_t*>(base +
                                                                       0x7E0 + i * 4);
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

int SheathHeldIds(std::uint32_t* outIds, int maxIds) {
    if (!g_inValid.load(std::memory_order_acquire)) return -1;
    int n = 0;
    for (std::uint32_t id = 0; id < 0x89 && n < maxIds; ++id) {
        if (g_inHeld[id >> 5].load(std::memory_order_relaxed) >> (id & 31) & 1u) {
            outIds[n++] = id;
        }
    }
    return n;
}

void PollSheathComboProbe(const void* entity) {
    if (!g_comboProbe.load(std::memory_order_relaxed)) return;
    if (g_comboProbeLines.load(std::memory_order_relaxed) > 1500) return;
    const void* ctrl = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        ctrl = g_lockedPlayerController;
    }
    if (ctrl == nullptr) return;
    if (entity != static_cast<const void*>(
                      static_cast<const std::uint8_t*>(ctrl) - 0x61C8)) {
        return;
    }
    std::uintptr_t ctx = 0;
    std::uint32_t id = 0;
    if (!StableSheathPair(ctx, id)) return;
    static std::uintptr_t lastCtx = 0;
    static std::uint32_t lastId = 0;
    if (ctx != lastCtx || id != lastId) {
        lastCtx = ctx;
        lastId = id;
        std::int32_t remapPhys = -2;
        const std::uintptr_t base = ModuleBaseAddr();
        if (base != 0 && id < 0x89 &&
            IsReadable(reinterpret_cast<const void*>(base + 0x3EEABF0 + id * 4), 4)) {
            remapPhys = *reinterpret_cast<const volatile std::int32_t*>(
                base + 0x3EEABF0 + id * 4);
        }
        std::ostringstream m;
        m << "SHEATHIN23: stable ctx=0x" << std::hex << ctx << " inputId=" << std::dec
          << id << " remapPhys=" << remapPhys;
        Log(m.str().c_str());
    }
    // INPUTBITS 全位扫描 (每帧, 独立于 nibble 转换日志): 变化记 on=[id h/e] 行
    {
        std::uint32_t held[5] = {}, edge[5] = {};
        if (SheathScanBitsSEH(ctx, held, edge)) {
            held[4] &= 0x1FFu;
            edge[4] &= 0x1FFu;
            for (int i = 0; i < 5; ++i) {
                g_inHeld[i].store(held[i], std::memory_order_relaxed);
                g_inEdge[i].store(edge[i], std::memory_order_relaxed);
            }
            g_inValid.store(true, std::memory_order_release);
            static std::uint32_t lastH[5] = {}, lastE[5] = {};
            if (std::memcmp(held, lastH, sizeof(held)) != 0 ||
                std::memcmp(edge, lastE, sizeof(edge)) != 0) {
                std::memcpy(lastH, held, sizeof(held));
                std::memcpy(lastE, edge, sizeof(edge));
                if (g_comboProbeLines.fetch_add(1, std::memory_order_relaxed) <= 600) {
                    std::ostringstream m;
                    m << "INPUTBITS on=[";
                    bool first = true;
                    for (std::uint32_t b = 0; b < 0x89; ++b) {
                        const bool hb = held[b >> 5] >> (b & 31) & 1u;
                        const bool eb = edge[b >> 5] >> (b & 31) & 1u;
                        if (hb || eb) {
                            if (!first) m << ',';
                            m << b << (hb ? 'h' : ' ') << (eb ? 'e' : ' ');
                            first = false;
                        }
                    }
                    m << ']';
                    Log(m.str().c_str());
                }
            }
        }
    }
    const std::uint32_t nib = SheathNibbleSEH(ctx, id);
    if (nib == 0xFFFFFFFFu) return;  // 读故障帧
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
    // SHEATHINTENT 竞速免疫布防 (独立于 comboProbe): 分类器同步报告纳刀族 combo
    // 命中 —— 输入此刻必然存在, 免除 位闪现 vs 轮询 的消费竞速 (11-02 user 实测
    // 首按大概率被分类器先消费 => 两个轮询位点都看不到 => 既无 SHEATHCUT 也无
    // 锁存; "倒计时中按才有效" = 上一次的锁存还活着)。锁存后仍由 PollRollSheathe
    // 在阈值帧统一发刀。
    if (row != nullptr && result != -1) {
        const std::int32_t combo = row[1];
        bool sheatheCombo = false;
        for (const int c : g_sheathIntentCombos) {
            if (c == combo) { sheatheCombo = true; break; }
        }
        if (sheatheCombo) {
            const void* ectrl = nullptr;
            {
                std::scoped_lock lock(g_actionMutex);
                ectrl = g_lockedPlayerController;
            }
            if (ectrl != nullptr) {
                bool mine = false;
                {
                    std::scoped_lock lock(g_sheathCtxMutex);
                    for (const auto& e : g_sheathCtx) {
                        if (e.mgr == reinterpret_cast<std::uintptr_t>(mgrCtx) &&
                            e.actionObj ==
                                reinterpret_cast<std::uintptr_t>(ectrl)) {
                            mine = true;
                            break;
                        }
                    }
                }
                if (mine) {
                    const std::int32_t aset =
                        *reinterpret_cast<const volatile std::int32_t*>(
                            static_cast<const std::uint8_t*>(ectrl) + 0xAC);
                    const std::int32_t arid =
                        *reinterpret_cast<const volatile std::int32_t*>(
                            static_cast<const std::uint8_t*>(ectrl) + 0xB0);
                    if (aset == 3 && g_rollSheathe.thresholds.count(arid) != 0) {
                        const std::int64_t nowMs = SheathNowMs();
                        const bool was = nowMs < g_sheathIntentUntilMs.load(
                                                     std::memory_order_acquire);
                        g_sheathIntentUntilMs.store(
                            nowMs + g_sheathGateMs.load(std::memory_order_relaxed),
                            std::memory_order_release);
                        SheathGateArm();
                        if (!was) {
                            static std::atomic<int> g_evalIntentLogs{0};
                            if (g_evalIntentLogs.fetch_add(
                                    1, std::memory_order_relaxed) < 20) {
                                std::ostringstream m;
                                m << "SHEATHEINTENT armed c" << combo
                                  << " roll=" << arid;
                                Log(m.str().c_str());
                            }
                        }
                    }
                }
            }
        }
    }
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
    // 过渡键只用 result: fin 是 b830 递归标志(0/1 每帧交替), 用它会每帧刷行
    // (11-02(3) c300 刷爆 400 行额度, stable 行/转换行全被饿死)。
    if (l.valid && l.result == result) return raw;
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
    Log("COMBOPROBE: SHEATHIN23 watcher active (capture-stable ctx + input "
        "state transitions, cap 400 lines)");
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
    // internal=游戏逻辑输入态(+0x780 按住位, 绑定无关) / any=两者并集
    // (内部源 A/B 验证期)。读法 = SheathCapHook 调用期捕获的稳定 (输入上下文,
    // 逻辑id) 对 → +0x780 位集 (COMBOPROBE v3 节注释); seen<8 不可信则静默回退。
    bool held = false;
    char src = '-';
    const int inputMode = g_rollSheathe.inputMode.load(std::memory_order_relaxed);
    // 意图锁存: 翻滚中早按的纳刀输入已被游戏消费, 凭锁存在阈值帧补发
    const bool intent =
        SheathNowMs() < g_sheathIntentUntilMs.load(std::memory_order_acquire);
    if (inputMode >= 1 && (InternalSheathHeld() || intent)) {
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
    // SHEATHDIAG (意图存活 + 翻滚表内 + 未发刀 时每 30 帧记门状态): 定位
    // "意图在、阈值帧过、却不发刀" 的吃门者。mot=anim+0x10C 实况。
    if (g_rollSheathe.thresholds.count(id) != 0) {
        const bool intentNow =
            SheathNowMs() < g_sheathIntentUntilMs.load(std::memory_order_acquire);
        static int s_diagTick = 0;
        if (intentNow && (s_diagTick++ % 30) == 0) {
            std::ostringstream m;
            m << "SHEATHDIAG id=" << id << " set=" << set << " inDA="
              << g_insideDoAction.load(std::memory_order_acquire)
              << " mwa=" << ModWeaponActive()
              << " heldInt=" << InternalSheathHeld()
              << " ent=" << (entity == static_cast<const void*>(player) ? 1 : 0)
              << " im=" << inputMode;
            Log(m.str().c_str());
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
    // 补一发尾段诊断: 过了全部门、临开火前一刻的 mot 实况
    {
        static int s_fireDiagTick = 0;
        if ((s_fireDiagTick++ % 30) == 0) {
            std::ostringstream m;
            m << "SHEATHDIAG fire-path id=" << id << " mot=" << mot
              << " thr=" << thIt->second;
            Log(m.str().c_str());
        }
    }

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
        SheathGateArm();
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
        // 11-02(4): 竞争行禁用。局内实证 {3,98} 落地后 1-3 帧被 vanilla 行抢先:
        // key19: 98→{3,1}→112(key19 r17=144→1), key20: 98→112(r9=288→1 /
        // r10=144→1)。{3,1} 进 mod 链续成 112 = 気刃斬1 位(112=刚一动作替换気刃斬1)。
        // type 翻 -1 = b830 直接跳过该行, 无 (-1,-1) gate 会连禁下一行的副作用。
        // 11-02(5): 首版 7 条地址手算 stride 用错(0x18 当 16), 5/12 全 skip;
        // 本轮同 boot REQTRACE 实锤剩余劫持者 = combo52→{3,11}(callerRVA=0x26b640
        // = b590 发请求点), 一并禁用。行地址已对照 dump exe 逐条 verify。
        {0x3F4EBE8, {1, 144, 1, 0, 3, 1}, {-1, 144, 1, 0, 3, 1}},    // key18/19 r17
        {0x3F4EC18, {1, 52, 1, 0, 3, 11}, {-1, 52, 1, 0, 3, 11}},    // key18/19 r19
        {0x3F4ED08, {1, 288, 1, 0, 3, 1}, {-1, 288, 1, 0, 3, 1}},    // key20 r9
        {0x3F4ED20, {1, 144, 1, 0, 3, 1}, {-1, 144, 1, 0, 3, 1}},    // key20 r10
        {0x3F4ED50, {1, 52, 1, 0, 3, 11}, {-1, 52, 1, 0, 3, 11}},    // key20 r12
        {0x3F4EE48, {1, 288, 1, 0, 3, 1}, {-1, 288, 1, 0, 3, 1}},    // key21 r9
        {0x3F4EE60, {1, 144, 1, 0, 3, 1}, {-1, 144, 1, 0, 3, 1}},    // key21 r10
        {0x3F4EE90, {1, 52, 1, 0, 3, 11}, {-1, 52, 1, 0, 3, 11}},    // key21 r12
        {0x3F4EF88, {1, 288, 1, 0, 3, 1}, {-1, 288, 1, 0, 3, 1}},    // key22 r9
        {0x3F4EFA0, {1, 144, 1, 0, 3, 1}, {-1, 144, 1, 0, 3, 1}},    // key22 r10
        {0x3F4EFD0, {1, 52, 1, 0, 3, 11}, {-1, 52, 1, 0, 3, 11}},    // key22 r12
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
