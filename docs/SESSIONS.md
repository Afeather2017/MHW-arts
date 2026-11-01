# SESSIONS — 会话过程档案（原 AGENTS.md 知识区全文搬移，只搬不改）

> 本文件 = 2026-09 ~ 2026-10 的**过程档案**。定案结论的提炼版在 `AGENTS.md`（索引）
> 与 `docs/` 各主题手册；这里用于考古：追捕过程、弯路目录、事故复盘、被推翻的旧结论。
> 日志里的 md5/备份名/部署链以本文件为权威记录。
>
> **新规**：新会话的过程记录追加到本文件末尾；结论蒸馏进对应主题手册；AGENTS.md 只留索引与全局状态。

---

### Native Gauge Plugin (2026-09-17: mod/probe split)
- **Source:** `ValorGauge/` — ONE source (`src/dllmain.cpp`), TWO build targets:
  - **`ValorGauge.dll` (mod, default):** Long-Sword-only gameplay mod. Weapon gate =
    `*(int32*)(weapon+0x9F8) == 3` (LS; same value func_0x141F627F0 returns and
    FUN_14118bda0 switches on — LS case holds 練気ゲージ判定/居合斬り/見切り conditions;
    consistent with wp03/w03/action-set 3. [CORRECTED 2026-09-25: the remapper
    FUN_14118c320's decompiled "case 2" IS LS — its dispatch does `dec eax` on
    weapon+0x9F8 before the jump table, so decompiled case N = weapon type N+1;
    see the 2026-09-25 section]),
    0xFF = no weapon; override via `nativePC/plugins/ValorGauge.weapon-type.txt`).
    Contains: action tracking, clang triggers (bit15 / bit250 / deflect flash /
    condEval-631) → counter window + no-hit injection + auto-force, F6 force,
    valor charge, overlay text. NO audio hooks, NO damage/hit-dispatch hooks.
    No-hit pool derived in-place: `mgr = *(player+0x7D20)`, `pool = mgr+0x28`.
  - **`ValorProbe.dll` (`VALOR_PROBE=1`):** passive research build. Damage calc,
    hit dispatcher, pool query (Aee050), condition-evaluator logging,
    F5 miner, F11 property histogram, F7 state logger, F10 diff capture,
    RXN/EPROBE watchers, Shift+F9 ATKSCAN/ATKTRACE. Installs NO gameplay
    behavior. Logs to `nativePC/plugins/ValorProbe.log`. Install ONLY while
    researching. (Wwise audio hooks removed 2026-10-19 — they caused sound
    residue; see the 10-19 section. No audio hooks remain in either DLL.)
- **NEVER install both DLLs at once**: separate MinHook instances double-hooking
  the same targets corrupts trampolines (crash). Both DLLs self-check at init and
  stand down if the sibling is loaded — but keep only one in `nativePC/plugins`.
- **Renderer:** DirectX 11 (`EnableDX12=OFF`); Libraries: ImGui, Kiero, MinHook.
- **Current mod behavior:** LS-only. Normal attacks charge valor 10%/hit; level-3
  write enters Valor; 见切/居合 clang auto-fires 刚1 mid-action with no-hit
  coverage. `F8` toggles overlay. **User-verified 2026-09-18**: LS triggers work
  post-split and the BGM bug is GONE — root cause confirmed as the old
  all-in-one DLL's audio hooks (sound-post wrapper arg mismatch suspected);
  the mod hooks no audio.

### Live Valor/FSM Discoveries
- `ActionController::DoAction` signature: `48 8D 41 07 48 C1 E0 04 46 3B 04 08`, function start at match `- 10`.
- Player action controller is inline at `player + 0x61C8`.
- Live FSM fields are available at `player + 0x6274` (target) and `player + 0x6278` (id).
- Current weapon pointer is stored at `player + 0x76B0`.
- Long Sword spirit level is `int32` at `weapon + 0x2370`: 0 none, 1 white, 2 yellow, 3 red.
- Aura/spirit level is an entry condition, not the Valor attack state; do not use its transitions as the mode exit signal.
- The FSM owns Valor-to-normal transition and the Valor Spirit guard point. The DLL only supplies normal-to-Valor entry by writing level 3.
- The Valor FSM does not use a custom `南风焓` runtime property; that text is an author label on nodes/links.
- Exported `wp03_action.xml` showed that the relevant alternate branches test the standard `オーラレベル白以上` condition, but runtime testing established that yellow/red activates the intended Valor behavior while white alone did not.

### LMT/TIML mapping rules (2026-09-13; avoids a known detour)

- **FSM action id ≠ LMT entry index.** The id→entry mapping table is `E:\MHW-arts\action-list.md` (见切 86→140, 突刺 79→105, 気刃突刺 87→170, 特殊纳刀 99→306...). Never `dump` an LMT by the FSM id directly.
- Working LMT: `nativePC/hm/wp/wp03/mot/w03_00/w03_00.lmt` (only one; 464 slots, most empty). Tool: `python patch_timl_defense.py dump|move|setval <lmt> <entry> ...` (expects checked edits, auto .bak).
- Vanilla revert point: `w03_00-origin.lmt`. The `.bak` is from an old session, NOT vanilla. Files restored from the game carry the read-only attribute — clear with `attrib -R` before editing.
- TIML frames tick at ~60fps (12f = 0.2s, user-measured).
- Defense recipe (empirically closed): i-frames = PlMotionCommon `mFlag` b26 (0x04000000) window + ONE `mTrigger` 2048 key anywhere inside it (gate, position-free); super armor = b24 (0x01000000) window, no trigger; both on → no-hit wins. 叮+蓝光 deflect presentation is FSM-property-driven, not TIML-switchable.
- 见切 entry 140 now runs a 0.2s parry window: `mFlag` f0=0x80400000 → f12=0x80000000; `mTrigger` f10=2048 (moved from f45 by value-swap so keys stay sorted). In-game verification pending.
- ValorGauge input-layer Ctrl auto-repeat: every Ctrl press pulses Ctrl for N seconds (config `nativePC/plugins/ValorGauge.counter-repeat.txt` = "<seconds> <pulseMs>", default 5.0/50) so any post-clang success window arms `見切り回避成功後の攻撃ヒット` without extra input. Pure SendInput/GetAsyncKeyState — no game memory layout involved. Replaces three failed memory-side negated-hit detectors (stager init-only; leaves 南风-only; negated hits never reach the 0x11B8880 arbitration latch).
- 2026-09-14 route decision: `見切り回避成功後の攻撃ヒット` is HIT-gated ("the follow-up attack landed after a successful foresight"), not a dodge-success flag, and no vanilla move offers dodge-success-without-hit (guard weapons react but chip). R-simulation stays installed as a convenience/test tool but is NOT the 刚1 mechanism. Fallback if the RXN watcher dies: hook the TIML no-hit collision rejection upstream of FUN_141f66dd0 (RVA 0x1F66DD0) — also the last gap in the iframe mechanism.
- 2026-09-14 decompilation round (Reverse/MonsterHunterWorld_dump.exe.c — use it, the on-disk exe is packed): **+0x7C0 is DEAD as a clang detector.** FUN_142045350 (RVA 0x2045350) clears `mgr+0x7C0` then sets it to 1 whenever prop `0xDE idx1 >= 1 && player->0x7626 == 0` — i.e. it arms at 见切 move start, unrelated to negation. `player+0x7626` is a global-config copy (`_DAT_1451c42b8+0xD2EA`), not a clang latch. Property chain fully reverse-engineered from FUN_141922190/1419221f0 + FUN_1412f80f0 + FUN_140462700: `component = *(player+0x126D8)`; `key = *(int32*)(component + 0x84 + id*24)` (resolver FUN_1412f80f0: `lea rdx,[rax+rax*2]` = 3*id then `[rcx+rdx*8+0x84]` = 0x18 stride — a hand-decode first read this as *16 and resolved garbage key=0; capstone + the *0x18 binding code confirm 24); `registry = base+0x5012048`, count `+0xB8`, `row = *(rowTable=*(registry+0xC8))[key]`; counter = `*(int16*)(row+0x24+index*2)`. 0xDE block: idx0 = follow-up latch (刚1 target), idx1 = foresight-family flag, idx2 = success counter (scales clang presentation duration in FUN_1411c26e0). No static write site for runtime counters (goes through virtual/cached-pointer paths) — irrelevant, we read AND write the chain ourselves.
- ValorGauge RXN watcher (deployed 2026-09-14): per-frame reads of mgr+0x7C0, pool+0xE4 no-hit remaining, and the 0xDE idx0/1/2 counters via the verified chain; logs edges only (`RXN f=<frame> act=<set:id> 7C0= nohit= DE=[i0,i1,i2]`), draws a live `DE[..] 7C0=` line top-left (cyan when 7C0 armed). Whiff-vs-clang run decides whether idx0 or idx2 is the clang signal. **F6 = property write test** through the same chain, config `nativePC/plugins/ValorGauge.prop-write.txt` = "<id> <index> <value>" (default `222 0 1`); logs key/row/before/readback. If idx0 isn't the 刚1 latch, repoint the config to the F11-identified id without rebuilding.
- 2026-09-14 first live run: with the wrong stride the watcher resolved key=0 (static read-only row, `PROPWRITE: unresolved or not writable`). Fixed build also logs `gate=` (player+0x7626) and falls back to VirtualProtect when the counter row is write-protected. Watch: does DE idx1 go 1 during 见切 3:86, does idx2/idx0 jump on the clang?

### Weapon attack-box format: wp03.col DECODED (2026-09-19; see COL_AttackBox_Research.md)

- **Weapon hitboxes are NOT in TIML and NOT mesh-bound.** TIML `EventCollision00` only opens the
  damage WINDOW; the geometry + damage live in `hm/wp/wp03/collision/wp03.col`:
  `COL\0`(names) + `CLND`(13 banks) + `CLGM`×13 (72-byte parametric volumes: Attack_Range + 20/20/20
  dims + ±90° sweep) + `ATK\0` (59×218B damage rows: Motion_Value, HitLag, Energy_Gen, Soften,
  MindsEye...) + `OAP\0`. Routing: TIML `CollisionTimelineObject` ノードID = names-table `unk4`
  → that entry's `cgp` = CLGM bank, `move` = ATK row (verified: 見切 node 63 ↔ name[24], 居合2 101 ↔ name[45]).
- **Attack_Range = hitbox volume SCALE, user-verified 2026-09-19**: the record volume is a
  player-centered ellipsoid; rng=500 (10x) felt ~3x bigger and hit monsters BEHIND the player
  (perceived size ≈ √rng). Banks are per-MOVE geometry sets, not weapon-length tiers
  (bank0=normal slashes 50, bank1=thrust 50/57, bank6=必殺 65/77, bank8=见切 75/93,
  bank9=居合2 100/124, bank10=mounted 360; 6 slot records per bank, f30 constant per slot
  across banks = 3/118/153/69/240/154; u48=8/9/10 likely ↔ EventCollision00 ReqNo A/B/C).
  Position does NOT come from the weapon mesh — and needs NO data at all: swap experiment
  (2026-09-19, thrust 3:79=LMT105 vs spirit blade 1 3:11=LMT104 boxes exchanged) proved the
  engine sweeps the volume along the ANIMATED WEAPON during the TIML window (poke anim = small
  front hit, sweep anim = 180° arc; hit TIMING shifted, window unchanged). Labels verified
  consistent (thrust node42="突き", spirit1 node56="気刃斬り１"); action-list.md now records
  11→104 alongside 79→105. **HitLag (hitstop, seconds) user-verified editable**: ATK HitLag_a/b
  set in pairs, scale 0.03 light → 0.40 max (aerial spirit thrust); HitLagSpd keep 1.0.
  Multi-hit recipe found: ATK Lingering=1 + Frame_Delay + long TIML window (3 vanilla rows use
  it; Frame_Delay units unverified). Open: s18/s20 (-1 vs valor's 8), f30 slot-type ids,
  u48↔ReqNo mapping, Frame_Delay units.
- Tool: `col_research/col_tool.py dump|bank|atk|setval` (value edits only, auto .bak; setval
  reproduced SR's one-byte range edit exactly). Structural edits (add bank/name/ATK row like the
  valor author did) need a section rebuilder — parser already splits sections, rebuilder unwritten.
- **SR author's col diff** = pure ATK tuning (MV 14→25/38→55, HitLag 0.03→0.15, Energy_Gen,
  Soften) + ONE geometry change (bank4[0] rng 50→65). **Valor author's diff** = structural: new
  name 南风焓 + new bank13 (rng=200, s18=8 — the only non-(-1) s18 in the file, possible bone
  binding) + 見切り攻撃 cgp 8→13 repoint + new ATK row. No global checksum; appends load fine.
- Blender plugin inventory (installed 4.3): MHW_Model_Editor edits `.ccl` = BONE-BOUND capsule
  collision (StartID/EndID bone ids, 64B/entry) for body/props/acc — NOT weapon hitboxes;
  FreeKinetics = LMT/TIML (windows); **no plugin parses .col** — use col_tool.py. Format spec:
  `col_research/mhw-editor-src/010 Templates/col.bt` (sparse clone of Synthlight/MHW-Editor; its
  CollisionTranslations/wp03.col.csv = move-name translations).

### 2026-10-19 破坏值定案: ATK Part_Modifier = 每击部位(破坏)积累乘数 (A/B 实锤 + decompile 布局)

- **部位破坏值在资源文件里, 不写死二进制**: 每击破坏积累 = 伤害 × ATK 行 `Part_Modifier`
  (file +16)。证据: 大居合(308-311, node86-89→行46-49)/无双斩(node89→行49, MV39) 破坏恒 0 —
  那 4 行是原版居合気刃1 無白黄赤的遗产, **原版 Part_Modifier 全 0.0** (原版居合气刃1 本就不破坏);
  刚1(见切动作→node63→見切り攻撃 行25) Part_Modifier 1.0 → 能破坏 (user A/B 局内确认)。
  原版旁证: 居合気刃2 (原版大居合, 毁部位出名) = **1.5**; 張り付き1/2 (柔化专用) = 0;
  乗りチャンス1-3 = 0。伤害(MV)不受该字段影响 → 它只乘部位积累。
- **修复 (2026-10-19 已做, user-verified 生效)**: 行46/47/48/49 Part_Modifier 0→1.0
  (col_tool setval, auto .bak)。想还原原版大居合的破坏手感可自行调 1.5。
- **⚠️ col 修复未生效的根因 (2026-10-19 定案)**: `nativePC/hm/wp/wp03/collisi on/` 带空格目录
  (2026-10-07 03:41 建立, 与 pre343mv 备份同刻) = Windows 字面路径匹配不到 `collision` →
  **nativePC col 自 10-07 起从未被加载, 游戏一直用 chunk 内 vanilla col**。当时所有 col 编辑
  (343mv/行22/PartMod 修复) 全部未进游戏。旁证: 无双斩无破坏=vanilla 行49 PartMod 0;
  一文字有破坏=vanilla 行17 PartMod 1; 伤害"正确"=挑的行 (17→30/49→39) vanilla 原值就是它。
  同款 spacing=禁用 模式: `sh ell/`、`wp03_ a ction.fsm`。**改 col 后必须确认目录名无空格**。
  激活前已还原隐患: 行10 突き MV 100→12 (旧测试残留, 否则激活后突刺 8 倍伤)。
  行22 (node85, MV100/type1/HitLag0.1, 10-18 编辑) 会随激活开始生效。
- **cAttackParamPl 运行时布局** (decompile: 类工厂跳表 0x143F949E0[type], 类名串
  0x142F12DD0 "cAttackParamPl", Load FUN_14027d880 / Save FUN_14027dd20, 实例 0x270 字节,
  行对象指针数组在 ATK section obj+0xC8): +0x08 Index, +0x18 Attack_Type, +0x20 MV,
  +0x24 Fixed_Damage, **+0x28 Part_Modifier**, +0x68 Stun, +0x6c Exhaust, +0x1c8 Soften,
  +0x1d0 Elem_Mod, +0x1d4 Status_Mod, +0x1d8 Mount, +0x232 MindsEye(bool), +0x240 Flinch(u32)。
- **ValorProbe 新工具 (md5 359e0592)**: **Shift+F9** (裸 F9 与 CSharp loader 冲突) =
  ATKSCAN (全堆扫 vtable +0x2F12DE0 找所有行对象, dump idx/MV/PartMod + 目标行 hex) +
  ATKTRACE (目标行 0x270 字节 PAGE_GUARD 20s, VEH 记每次读写的 rw/off/rip/栈)。
  配置 `nativePC/plugins/ValorProbe.atktrace.txt` = "<行号>" (默认 22 = 用户的自定义行,
  node85)。作用: 确认某行是否真被加载 (MV 对不上=col 没加载) + 抓任意字段的消费 RIP 链。
- LuaEngine (TCP 28991, luaL_dostring 逐行) 有 GetAddress/GetAddressData/SearchPattern,
  但 SearchPattern 只扫主模块不扫堆, 标准 Lua 无 FFI → 堆扫描只能靠原生 DLL。
- **(2026-10-19 晚) ValorProbe Wwise 音频钩子整体移除 (部署 md5 f2922cd8)**: 症状 = 切换声音后
  旧声音残留 (A 起播→切 B, A 不停)。移除内容 = PostEventId/SoundPost(0x1426D4C50 wrapper)/
  PostEventName 三个 MinHook + 死代码 ClangSoundVirtualHook/InstallClangSoundVirtualHook +
  对应 HookId/签名表/名字表/装载块。理由: 叮检测早已定案走 event bit250(居合)/bit15(见切),
  Wwise 路线 09-16 就判死 ("tooling kept" 的工具债现在还)。探测 RVAs (0x28B8DD0/0x28B8FE0/
  0x26D4C50) 保留在本节与 09-16 章节备查; signatures.json 里的三条记录不再被源码引用。
  g_addrAudioSingleton 仍解析 (MEMORYMAP 报告用, 纯被动读)。**probe 现在零音频钩子。**
- **(2026-10-19 晚) 构建系统 /utf-8 定案**: dllmain.cpp 含大量中日注释, MSVC 默认按 ANSI
  (GBK/936) 解码 → C4819 警告是烟雾报警, 实际危害 = 注释尾部多字节序列吞掉换行, 把下一行
  声明并进注释 → 幽灵 C2065 (本次: g_forceCfgSet "未声明", 行号还错位 1)。HEAD 与工作副本
  声明区逐字节相同却一个编过一个报错 = 解码状态对内容位置敏感, 别按行号排查。
  修法 = 两个 target 都加 `target_compile_options(... /utf-8)` (CMakeLists.txt)。
  教训: git stash push/pop 会触发 autocrlf 重写文件 (LF→CRLF), 复现/掩盖这类问题;
  cmake -B 传 Windows 路径必须带引号, 裸 `ValorGauge\build` 的反斜杠被 Git Bash 吃掉
  会建出 `ValorGaugebuild` 假目录。

### 2026-10-19 (2) 无双斩专属伤害行 + 多段判定路由模式 + vanilla 大居合结构

- **多段判定 = CollisionTimelineObject ノードID 轨道多键** (同一判定物体随时间换路由行):
  小居合307 = f0:node84→f32:node101 (行43→行44); 无双斩343 = f0:node88→f58:node89。
  mFlag 是窗口开关, ノードID 是"当前时刻用哪个行" — **第一段窗的节点键通常在 f0 而非窗口帧**
  (改键时按轨道键帧找, 别按窗口帧)。
- **无双斩 = LMT 343** 双窗: 窗1 f31-40、窗2 f58-71。原路由 行48(MV31≈158伤)/行49(MV39≈197伤);
  用户配装 k≈5.05 伤害/MV。行48/49 与大居合310/311 共享 → 不能直接改。
- **专属行改造 (2026-10-19, user-verified)**: names 空闲节点 node90(idx51)/node94(idx55) →
  cgp=9, move=42/45 (手工偏移: names 表 walk o=0x18, entry=nul+1, cgp@+0 move@+4); 工具行 42/45 →
  行42 = {type1, MV31, **PartMod0**, HitLag0.1} (无双斩第一段, 无破坏);
  行45 = {type1, **MV150**, **PartMod2.0**, HitLag0.1} (第二段, 破坏×2, 伤害≈758);
  LMT343 ノードID[0] 键 f0: 88→90, f58: 89→94 (patch_timl_defense setval, .bak 自动)。
  **局内实测 (user)**: 单刀总伤 ≈1000 符合预期; **hit1 PartMod0 附带解决了诟病已久的问题 —
  第一刀的部位积累会把怪头打断位移, 第二刀落空** → 佐证 Part_Modifier 喂的是"部位积累"
  (破坏+部位踉跄共用的那条槽), 归零 = 该击完全不打部位槽。数值为占位, 后续按局内手感调。
- **vanilla 大居合结构**: LMT 308-311 = 無白黄赤四色变体 (vanilla 原生就有!), 各**单窗**
  f17-37 → node86/87/88/89 → 行46/47/48/49 (MV 17/19/31/39, vanilla PartMod=0 → 原版大居合
  本来就不破坏)。用户实测 vanilla 红刃两刀 197/356: 197 = 行49(39MV)✓; 356≈MV70.5 ≈ 行52(黄72,
  +2.2%) **但全 164 条 vanilla TIML 无任何节点路由 91/92/93** (行51-53 居合气刃2 已被开发组删除,
  空挂) → 第二刀来源未钉死, 候选: 代码侧解放倍率(39×1.8=70.2→354.6 差 0.4%) / 运行时选行。
  ATKTRACE 守卫行49 或行52 放一次 vanilla 大居合可钉死。
- **Blend 侧同步 (2026-10-19 晚, 导出对照验证通过)**: 新工作文件 =
  `MHW-valor-blender/MHW-Valor-0.0.2-damage-modify.blend` (FreeHKNodeTree "w03_00",
  LMTEntryNode.entryNum=343 → TIML Data Node → 9×TIMLActionNode, CollisionTimelineObject 轨道
  = action `TIML::nTimelineParam::CollisionTimelineObject.083` 的 fcurve, 轨道名是哈希
  `FreeHKTiml.mB296CA66` = ノードID[0], m08FD20A6×2 = mFlag 位包)。改 fcurve 键值
  (f0:88→90, f58:89→94, handles 同步) 后, 用 export_ops.exportNodeToFile 现场导出到临时文件,
  patch_timl_defense 逐轨 diff entry 343 = **与二进制修改版完全一致**。此后再导出 LMT343
  自动携带 node90/94, 无需重打补丁。注意: blend 当时未保存 (改在内存), 用户需自行 Ctrl+S;
  导出对照产物 `_e343_check_export.lmt` 是含全部 WIP 的临时文件, 不可直接部署。

### 2026-09-15 Original ValorLS.dll mechanism (Reverse/ValorLS.dll.c — use it)

- User's counter-evidence killed the "unbound = no clang" theory: vanilla iai/见切 still clang with the vanilla FSM. Conclusion: 0xDE/7C0 is ONE success candidate (likely tied to the extended-iframe iai mechanic), not THE clang presentation; other candidate evaluators (FUN_1411c2780/27c0/2800 family reading prop 0x35) exist.
- The original mod NEVER touched property R. `Reverse/ValorLS.dll.c` (native C++ layer, MinHook) shows its real mechanism:
  - Hooked 0x140310030 (frame tick) and 0x140AE9EA0 = the **action request processor**; param_2 = request struct: `+0x00` lifecycle (1 idle / 3 executing, old build), `+0x10` actionSet, `+0x14` actionId, `-0x88` current action id. **Our DoActionHook's ActionInfo* == req+0x10.**
  - Key press (configurable VK): if request idle (`*req==1`) → write `req+0x10=3, req+0x14=<art action id>` (0x5e/0x6f/0x6a/0x68 for its four arts) → game queues that action. **Force-action bypasses all FSM conditions** - no property write needed for 刚1.
  - Gauge: player-manager chain from DAT_145073ED0; spirit level lock via `weapon+0x2370` (same field we use); weapon+0x2388 (invuln timer) checked before one art. One hard-coded old-build call `0x142134B70` - the version-locked part that broke.
  - Action-id whitelist (10 ints) = moves that do NOT reset valor charge, checked at every action change (`req-0x88`).
- ValorGauge now has the same force path: DoActionHook captures ActionInfo* (logs a one-time REQ layout check: state + id@-0x98), **F6** queues {set,id} when idle (refuses when busy), **Shift+F6** = old property-write test. Config `nativePC/plugins/ValorGauge.force-action.txt` = "<set> <id>" (default `3 104`; set 刚1's real id).
- Next: validate force mechanism with F6, then wire clang detector + R-window to auto-queue 刚1.

### 2026-09-16 刚1 build session (mid-action interrupt works; clang discriminator still open)

- **Mid-action interrupt SOLVED**: calling `g_originalDoAction(controller, {set,id})` directly from the property-getter hook (game thread, every frame) interrupts ANY current move immediately. F6 arms a one-shot force; config `nativePC/plugins/ValorGauge.force-action.txt` = "<set> <id> [invulnSec]" (default 3 104 0.5). Shift+F6 = property write test. The ActionInfo* that DoAction receives is a STACK TEMPORARY - never store it (captured pointers decayed into float/resolution garbage).
- **Crash lesson**: MinHook on an already-hooked target from inside its own detour = crash (MinHook disassembles the patched bytes for the trampoline). Happened because vtable+0x150 of the audio singleton IS the sound wrapper itself. Also: request-slot scans are dead ends - controller memory contains many {3,0} copies and the one "persistent" hit was a passive echo struct that never gets consumed.
- **Wwise route (dead for detection, tooling kept)**: AK::SoundEngine::PostEvent id-version RVA 0x28B8DD0, string-version 0x28B8FE0, game wrapper FUN_1426d4c50 (RVA 0x26D4C50, args: entity, eventId, gameObjectId, flags), audio singleton global RVA 0x51C458 (vtable+0x150 = the wrapper itself). Event **1086158451 is NOT clang-exclusive** - it is the shared "metal blade ring" family: draw attacks (3:7) post it too. 4-whiff/7-clang diff inside 3:86 looked perfect but does not generalize. WWISEEV + CLANGSOUND logging capped and kept.
- **Hit dispatcher FUN_141f71960 (RVA 0x1F71960, 2 args)**: returns 0xD for ambient no-collision checks (per-frame flood); NON-0xD returns fire whenever the no-hit window is OPEN (negative floats drifting -136→-350 + 0x6B alternating) - **they track the i-frame window, not the clang** (user-verified: COUNTER WINDOW opens on every 见切, whiff included). Damage calc FUN_141f66dd0 (RVA 0x1F66DD0, 3 args) entries = hits that actually connected (FSDDMG log).
- **Current live design**: dispatcher rejection during 3:86 opens a 0.6s COUNTER WINDOW (screen indicator, cyan) + injects pool+0xE4 no-hit 0.5s; a Ctrl (spirit key) press inside the window fires the force. Opens on whiffs too - **known false positive, accepted for now**.
- **見切り回避成功後の攻撃ヒット identity solved**: FSM condition property names live in a .rdata string table (~VA 0x1430BA8F0); the name→member-index resolver (inlined in FUN_14118bda0) maps it to **member index 0x277=631 of cFSMPl_W03**. It is a REFLECTION member, NOT registry-backed (REG id=631 row=0x0; component slot key=1 exists but lookup(1) returns junk). Evaluator = FUN_14118bda0 (node: +8 owner instance, +0x10 index). **Tomorrow's primary lead**: extract the descriptor-table offset for index 631 and read the vanilla latch directly - that is the exact 刚1 trigger. Vanilla 见切 already uses this condition, so the latch demonstrably works.
- User's additional requirement noted: the vanilla follow-up also has a kenki gauge cost - the counter trigger should respect gauge state (window currently opens even with no gauge).
- Current milestone line: see bottom.

### 2026-09-15 ROOT CAUSE: property R (222) was UNBOUND under the vanilla FSM

- Live A/B came back ALL-ZERO: F11 groups (clang 见切 / 居合 3:102 / whiff 见切) show `222 idx0/1/2` read on PLAYER (callers 0x11BE7AC = arbitration-region, 0x11C02CD, 0x20453AC = FUN_142045350+0x5C, 0x11C2713 = FUN_1411c26e0+0x33) but **max=0x0 in every group — even the clang one**. 7C0 never armed because idx1 never reached 1: **the success-window system was dead the whole session.**
- Cause: the 09-14 revert to 原版 wp03_action.fsm removed every FSM reference to property R(222) → the property binder leaves the component key slot 0 (`key = *(int32*)(component+0x84+222*24) == 0`) → all reads fall through to static row 0 → idx1 can never arm. At the 09-12 A-capture the modded FSM still referenced R, so the whole chain lived. The clang the user "heard" was only the TIML b26 negation feedback — no blue-flash presentation.
- Evidence chain: TIML is innocent (vanilla entry 140 `PlMotionCommon mFlag` is already f0=0x80400000/f130=0x80000000 — our edit only moved keyframes f130→f12 and trigger f45→f10). 0xDE's setter lives in the event system: `func_0x141f604b0(target, bit)` = `bts [target+0x8938+(bit>>5)*4], bit` (edge array at +0x8968, id 0x9B special-cased); call sites fire it when a TIML-flags bit28 is consumed.
- Consequences: every earlier "detector failed" result (stager / 南风 leaves / arbitration latch / 7C0 / 0xDE polls) ran against the unbound property — do not trust them as negative evidence. RXN poll + F6 now treat key==0 as "unbound" and say so in the log.
- Fix path: re-add an FSM reference to R via XFsm — ideally the real 刚1 branch condition (VariableNode R / cFSMPl_W03), which both rebinds the property and implements the gating. Then reload the area/quest (binding happens at load) and rerun the whiff-vs-clang A/B with live DE values.

### 2026-09-17 刚1 SUCCESS for iai (101/102); foresight detector built, deploy pending

- **F5 paired-scenario state miner** (the tool that broke the deadlock): records
  per-frame memory deltas per evasion run over weapon/mgr/pool/FSM/player
  windows (incl. the event bit array at player+0x8938!), auto-tags phases per
  F5 press, dumps a correlation-ranked report (clang-rate minus whiff-rate).
  Always-on recording; F5 = phase marker + snapshot dump.
- **Mined + verified: iai negation = event bit 250** at player+0x8954 bit 26
  (dword 0x1C000000 vs whiff 0x18000000/0x18400000; self-clears when the
  presentation ends). Trigger wired: bit rising during 86/101/102 → inject
  pool+0xE4 no-hit 0.5s + counter window + auto-force 刚1 via DoAction.
  **User-verified working for 小居合 101 AND 大居合 102**; whiffs silent.
- **Foresight (86) detector built, not yet deployed-validated**: weapon+0x23A0
  = blue-flash intensity, jumps to 1.0 and decays only when the deflect
  presentation plays (mined 4/4 clang vs 0/5 whiff). Trigger: crossing 0.5
  upward during 86. Was blocked by the game locking the DLL mid-test.
- **Ruled out this session**: player+0xB228 (attack-connect flag - sets on ANY
  connecting attack incl. normal slashes); Wwise event 1086158451 (shared
  metal blade-ring family: draw attacks post it too); dispatcher non-0xD
  codes (track the no-hit window state, not negation; rejected type-6 hits
  never reach FUN_141f71960 at all); FUN_14118bda0 (editor/debug-only path,
  never called in gameplay); weapon+0x2374 (slow linear float timer, likely
  the red-spirit-gauge duration - useful later for gauge gating, NOT the
  clang latch; flooding it masked other probes once).
  **CORRECTED 2026-09-24: 0x2374 IS a 0..1 float gauge-family field, but round-2
  in-game A/B proved it is the LEVEL-DURATION timer (game drains 0.000556/frame
  ≈ 30 s cycle; refilled from params on level change), NOT the visible gauge —
  the 气刃量 is weapon+0x2368. See the 09-24 section.**
- User's mechanics input (authoritative): 见切 negation gates whether the
  follow-up (大回旋 route) is possible - success has TWO parts (i-frame hit +
  follow-up lands = condition 631); 居合 negation only drives the super-armor
  frames - that is why iai has a clean standalone negation state and 见切
  needs the presentation-side signal instead.
- Config surface: force-action.txt "<set> <id> [invulnSec]"; counter-auto.txt
  0/1 (auto-force vs Ctrl-gated inside the 0.6s window); prop-watch.txt ids;
  prop-trigger.txt id/index; prop-write.txt id/index/value; state-address.txt.
- Next: deploy the flash detector (game closed), validate 见切 clang → done.
  Optional later: kenki-gauge gating via the 練気ゲージ判定 properties (628/629).

### 2026-09-17 (late) Vanilla 见切 success latch FOUND: event bit 15 at player+0x8938

- User's model confirmed by decompile: 见切/居合 share the negation machinery,
  见切 ADDS a success judgment; bit 250 is the IAI-specific negation event,
  not "hit during i-frames" in general.
- **The vanilla input remapper = FUN_14118c320** (registered per player-FSM
  init in FUN_14118dca0/14118ddc0 as an action-request callback; IB variant
  FUN_14118d3a0 chosen when player+0x7626 != 0). Signature
  `(mgrCtx, out{state,id}, req{state,id})`; player at mgrCtx+0xD90,
  weapon = *(player+0x76B0). It REMAPS buffered inputs into special actions:
  - weapon+0x2368 (special-sheathe stance): 0→100, 1→101, 2→102, 3→103,
    4→104, 0x12-0x22→105..0x7d family, 0x35/36→160/161, 0x8d→0x8e..0x95 by
    FUN_14211f040(weapon).
  - mgr+0x660/661/662 (mgr=*(player+0x7D20)) latched + req state==1:
    0/9/99/13→762/766/764/764-family, **100/747→(bit15 ? 763 : 764)**,
    7/8/101/748→763, 0x19d(413) with player+0xDDA0 action∈{97,101}→770.
  - **Event bit 15 of player+0x8938 (`u32 & 0x8000`) is the vanilla
    success/failure discriminator** for the 见切 follow-up remap (763 vs 764)
    and the IB special-sheathe remap (162 vs 163). Same TIML event-bit array
    that carries iai negation bit 250 (player+0x8954 bit26). This is the
    "见切成功" latch the user hypothesized - set by the game itself, no FSM
    edit needed.
- Property 631 static facts ( closes the earlier lead): its registration
  accessor SUB_140240720 = `xor al,al; ret` (stub) - condition 631 is NOT a
  stored bool; the "value" lives in the remapper/state logic above. Per-weapon
  condition descriptor tables at 0x143FD5590 (stride 0x130 per weapon type,
  rows {i32 key, ptr, u32} @ block+0x20/+0x28) hold only 2-16 special-case
  rows; global fallback 0x1434DA930 has 12 rows (keys 0x3E8-0x3F3). Name→index
  resolver FUN_140b2e4e0 (giant strcmp chain): 0x1430BA8F0 = 見切り回避成功後の攻撃ヒット
  → 0x277; neighbors 0x274 練気ゲージ判定(見切り 0x275 練気ゲージ判定(必殺 0x276 居合斬り.
- New tools (Reverse/): `vadump.py <va> [len]` (dump bytes at VA from the
  unpacked dump exe), `fsm_table.py` (parse condition descriptor tables),
  `scan_277.py` (immediate-scan); `disasm_rva.py` now reads the dump exe by
  default (on-disk exe .text is encrypted - capstone gets garbage).
- DLL (deployed 862208B): VSEEK watcher logs bit15 + mgr660/661/662 edges
  (`VSEEK bit15= ev0= m660= m661= m662= act=`); trigger = bit15 rising during
  3:86 → counter window + no-hit + auto-force 刚1 (same flow as iai bit250).
  Flash detector (weapon+0x23A0) still armed as a second 见切 signal; log
  lines identify which fired.
- Test matrix: whiff (no trigger expected); clang w/o follow-up hit (fires?
  → bit15 = negation-only); clang + follow-up hit (full success). iai
  101/102 regression must stay green.

### 2026-09-17 (night) 刚1 COMPLETE (all clang paths) + update-resilient address map

- **User-verified done**: whiff never triggers; clang fires immediately on
  见切 86, 小居合 101 AND 大居合 102. Mechanics confirmed: 见切 = step-back
  (whole step is i-frames; hit there = 叮) then counter slash (no i-frames,
  small super armor window when clang'd).
- Final trigger set: bit15 rising during 3:86 (foresight success latch),
  bit 250 rising during 101/102 (iai negation), weapon+0x23A0 flash and the
  CondEvalHook 631 path as extra safety nets. Any of them → counter window +
  pool+0xE4 no-hit + auto-force 刚1 via direct DoAction.
- **Update-resilience** (preps for future game patches):
  - All 10 function hooks resolve by byte signature first (kHookSigs[] in
    dllmain.cpp; wildcards auto-cover rel32 branches + rip-relative disp32),
    then recorded RVA, then `nativePC/plugins/ValorGauge.addresses.txt`
    override (`name=0xRVA`). Signatures generated/verified offline by
    `Reverse/sig_check.py --emit|--verify` (all 10 unique vs dump exe).
  - `MEMORYMAP` startup report in ValorGauge.log: per-hook ok/BAD + propRegistry
    count/rowTable + audioSingleton. Runtime semantic probes log once when
    spirit level leaves [0,3], standby >1, or no-hit < -1 (struct-shift alarm).
  - Recovery wizard: `Reverse/ADDRESS_RECOVERY.md` — step-by-step lookup guide
    (per-target anchor/steps/verify/write-back for every hook, data address and
    struct field; dump+Ghidra setup; find_xrefs.py string→xref tool; F5-miner
    fallback). Someone holding the doc can re-derive all addresses after an update.

### 2026-09-18 Roll i-frame negation: NOT detectable player-side (research closed)

- Question: can a perfect through-roll (drawn roll = 3:18) be detected like
  the 见切/居合 clang? Answer after 3 instrumented mining rounds: **NO.**
- Evidence (ValorProbe.log, 6-run session): during PERFECT through-rolls the
  hit dispatcher produced ZERO type-6 interactions (ROLLDISPATCH log empty);
  no event bits, no presentation fields moved. During the FAILED roll (i-frames
  expired, hit connected): event bit 250 (ev8954 bit26) fired + pB228=1 + HP-1.
- Conclusion: MHW implements roll i-frames as pure collision EXCLUSION - the
  player simply is not in the hit query set, so no layer records "the attack
  passed through me". 见切/居合 are detectable ONLY because they have
  deflect/negation INTERACTIONS (type-6 with special codes + presentations).
  Generic perfect-dodge detection would require computing hitbox-vs-player
  overlap geometrically from monster collision data - a separate project.
- Refinement: event bit 250 is a per-motion TIML event - same id, different
  meaning per move (iai: fired on negation/clang; roll: fired on hit-connect).
  Do not treat event bits as global semantic constants.
- Swap discipline learned the hard way: the old POST_BUILD auto-install kept
  re-adding ValorGauge.dll during research builds and the sibling check stood
  the probe down (two wasted rounds). Auto-install removed from both targets;
  deploy explicitly, one DLL at a time.
- (2026-10-04 更正) **所有带空格的文件名都是用户刻意改的** (如 `w03_00.lm t`、
  `Monster Hunter World v...+.exe`) — 不是损坏事故, 不要当异常报告, 也不要"修复"。
  (早前记录的 09-29 "ValorGauge.d ll 改名事故"经用户否认, 原文已删。)

### 2026-09-19 FSM 驱动链: how zero-inbound nodes (居合 101/102) are entered

- Tools: `dump_fsm_xml.py` (parses XFsm XML export) → `wp03_fsm_map.md` (all
  90 nodes: id/ActionNo/in/out + route sections). XFsm shows NODE NAMES only —
  node identity is ActionNo, names lie: 必殺攻撃=87 気刃突刺, 必殺駆け上がり=89,
  居合気刃=node65 act=102. Node45→55 "必殺攻撃→必殺駆け上がり" IS the 87→89 link.
- Zero-inbound nodes (64 居合斬り/101, 65 居合気刃/102, rolls 20/21/22/1005,
  jumps 1006/1007, ledges 1002/1003) are entered by ACTION REQUEST matching the
  node's ActionNo — links only route buffered combo follow-ups. Node-less ids
  (98, 99, 100, 103 have NO node at all) still play motions: action layer and
  FSM node entry are decoupled. Force-DoAction (F6/刚1 path) is the manual
  version of the same mechanism.
- Iai chain (decompile-verified): ほぼ全攻撃(1013 sys-state) --cond232-->
  居合斬り開始(node61, act=-10 marker, no outbound) hands over to the action
  layer. Special-sheathe stance = weapon+0x2368 via FUN_14211cc60(weapon,bool):
  true→=2 + FUN_140ae08a0(player+0x8808) start-notify; per-frame updater decays
  2→1, then 1→0 + FUN_140ae10f0 end-notify (gate `0x2368 != 0` = stance window
  +1 linger frame). **CORRECTION to 09-17 notes: 0x2368 is a stance flag, NOT a
  0-4 selector.**
- Button → buffered set-3 request → remapper FUN_14118c320 (registered by
  FUN_14118dca0/ddc0; player at mgrCtx+0xD90): only in-set==3 proceeds →
  switch(weapon+0x9f8) case 3 (LS) → if 0x2368!=0: REQUEST-id→action-id remap
  0→100, 1→101(小居合), 2→102(大居合), 3→103, 4→104, 0x12-0x22→0x69..0x7d/0x43,
  0x35/36→0xa0/0xa1, 0x8d→FUN_14211f040→0x8e..0x95; else passthrough. Output
  → DoAction (current build VA 0x140269CE0) → FSM enters node 64/65.
  **CORRECTION: mgr+0x660/661/662 remap (in-set==1 gate) outputs are SET 1**:
  99/13/746→{1,764}, 0→{1,762}, 9→{1,766}, 100/747→bit15?{1,763}:{1,764},
  101/7/8/748→{1,763}, 413→{1,770} (gated on player+0xDDA0 action∈{97,101} via
  func_141cb4fb0∈{0x61,0x65}).
- FUN_14211f040 is NOT an iai selector: it grades the 0x8d draw-family —
  FUN_142178100(player+0x230, player+0x7870)*DAT_142f16ec0 vs obfuscated floats
  (player+0x7810)+0xa40/+0xa44 ^ DAT_142f130d0, returns 0-3 → 0x8e/0x8f/0x90/
  0x92/0x93/0x94/0x95.
- Open: upstream producer of request-id 2 (the 大居合 trigger — test via
  capture log on counter-iai: 3:101 then 3:102?); FSM jump-by-ActionNo function
  not pinned (evidence behavioral); what FUN_140ae08a0/10f0 toggle on
  player+0x8808.

### 2026-09-19 (late) Hit-based valor charging + multiplayer-safe local player

- **Charge now requires a CONNECTED hit.** DmgCalcHook (FUN_141f66dd0, sig in
  kHookSigs, fallback RVA 0x1F66DD0) is installed in the MOD build too. a1 is
  the attacker entity (body derefs a1+0x7D20 = mgr); the session-wide hook is
  filtered: a1 == lockedPlayer-0x61C8 or it is counted (g_hitChargeSkips) and
  never charges. Charge = `ValorGauge.hit-charge.txt` percent (default 10) per
  connected hit of an IsNormalModeChargingAction move while !valorMode &&
  ModWeaponActive; reaching 100 → ActivateValorMode. percent 0 = legacy
  charge-on-action-start; negative = charging off. First 8 entries log
  `HITATT a1=.. localPlayer=.. MATCH/skip` — empirically confirms a1 is the
  ATTACKER (if received hits also MATCH, a1 is defender-side and the filter
  must be revisited).
- **Local player (multiplayer).** ValorLS's manager chain, offsets read from
  its .data (0x1800329A8 = 0x50/0x76B0/0x00): player = *( *(0x145073ED0) + 0x50 ).
  Current build has ZERO .text refs to that VA (may have slid) →
  ResolveLocalPlayerViaManager() uses it only when it validates (weapon+0x9F8
  ∈ 1..14). DoAction lock now REFUSES set-3 controllers that are not the chain
  player (`FOREIGN ... not locking` log); without a valid chain it falls back
  to first-set-3 + IsLongSwordController. Unverified: whether remote hunters'
  actions even route through this process's DoAction.
- Build ok (both targets); deployed mod md5 a8df953f4bc20baa3e1732744d57fbb8.
  Test: 1 hit on monster → HITCHARGE +10 in log; whiff → no charge; accept a
  hit → HITATT a1=monster (skip, no charge).

### 2026-09-20 真正纳刀机制: 动作 id 驱动的挂点表切换 (decompile-verified, 复用槽位定案)

- **合并 = 刀模型绑定节点从"手部挂点表"切到"鞘内挂点表"**（两模型从未合并; 纳刀态刀刚体
  跟随鞘所以抖动同步）。两层机制缺一不可: 030/047 Y-scale (1/2/3) 只做**表选择**（位置）,
  "携带态开关"（游戏认为拔刀/纳刀）由**当前动作 id 每帧决策 + 边沿触发**执行。自定义动作
  只写 (1,1) 通道 → 刀到背后但携带态仍"拔刀" → 独立抖动/分离（(1,1) 不合并的根因）。
- 链路: 每帧 FUN_1421237B0 (RVA 0x21237B0) 读当前动作 id (func_0x141BFF740(player,0)) —
  **收刀家族 {5,6,9,10,69(0x45),87(0x57)} 或 weapon+0xA50/+0x23C0 非0 → 期望纳刀,
  其余 → 期望拔刀**; 动作 98/100/101/102 锁存 weapon+0xAB8=1; weapon+0x2D0 bit0x20/0x40
  = 納刀中/拔刀中 (疑似 FSM 条件源) → 边沿触发 func_0x142150D10 (RVA 0x2150D10, 锁存
  weapon+0xC71: 0=拔刀/非0=纳刀, 仅状态翻转时放行) → 控制器 (weapon+0xAF0, vtable
  0x1435195B8) vtable+0x110 = 0x142374710 写 ctrl+0x181 (0=拔刀/1=纳刀, 极性推定待实测)
  → FUN_142373BF0/D50 重绑全部节点: `[表+0x4B0][节点id&0x1FF]`→行号, 行 =
  `[表+0x4A8]+row*0xC0` (+0x20 起 16 float 变换); 激活表 = vtable+0x158 = *(ctrl+0x170)。
- 表加载: FUN_140325130(持有者, 槽0..4, 状态值) → FUN_1403253F0/140326520 按资源 id;
  **状态值 = 030/047 的 1/2/3 去向**（采样指令→此参数未钉死, Ghidra 在调用点丢 r8）。
  武器上有**三个同款控制器** +0xAF0/+0xFA0/+0x1450 (析构 FUN_14210B710 证实) — 推测
  刀/鞘/配件。特效 FUN_142125150/125860 用同一挂点表取位 (row*0xC0+0x20)。
- **定案: 自定义纳刀动作复用收刀系槽位（优先实测过的 5/6/9/10）** — 决策/携带态/特效/
  挂点自动继承, 与"叮+蓝光复用居合槽"同一策略。LMT entry 的 030/047 翻转帧只负责视觉
  滑入对时。
- DLL 测试: `(*(void(**)(void*,int))(*(void**)(weapon+0xAF0)))(weapon+0xAF0, 1)` =
  强制合并 (传 0 = 分离); 若被更新器拉回, 在收刀系动作期间测。
- 待钉 (ValorProbe): 0x325130/0x3253F0 (槽号+状态值序列 vs 030/047), 0x2150D10/
  0x2374710 (极性+时序), +0xC71 维护方, +0xA50/+0x23C0 语义, +0xAB8 读取方,
  set≠3 分支的 0x214/0x215 (疑似 3:99 架势合并路径), 挂点表资源文件定位。
  详见 LMT_TIML_Hitbox_IFrame_SuperArmor_Sheath_Research.md 文末章节。

### 2026-09-20 (2) 为什么 FSM 里找不到 3:11 気刃斬り1 — 链接条件测按键属性,不测请求 id (XML-verified)

- 気刃斬り1 (request 3:11, LMT104) 在 wp03_action.fsm 里**没有 ActionNo=11 的节点,也永远不会有**。
  它的 FSM 状态 = **节点1: ActionNo MtEnum=1001,编辑器名"特抜(気刃斬1"**(节点名再次说谎;`ActionNo`
  在 XFS 里是单值 MtEnum,没有隐藏第二字段)。证据: 节点1 的 15 条入边 = 恰好所有能 R2 接气刃斬り1
  的招式(待機２ cond38 / 斬り上げ16 / 突き19 / 縦斬り17 / 踏込斬り18 / 回避F·R·B·L 125·132·130·131 /
  J攻撃着地133 / 段登(納刀)47 / 気刃斬り上げ71 / 移動抜刀78 / s0006 231 / s000c 286=小居合路线);
  出边 = 気刃斬り1 的真实派生(→気刃斬2 cond20, →見切り153, →必殺攻撃163, →滑歩気刃1 F/L/R 243/247/248)。
- **机制: FSM 链接条件是 cFSMPl_W03 的按键属性 VariableNode,与请求 id 无关**: cond16-20/38 = `R`
  (气刃键), cond153 = `R+○`, cond163 = `R+△`(气刃突刺输入 R2+△ ✓), 另有 `△`/`○`/`△+○`/`△+○+←`
  树。R2 按下 → 输入层发请求 {3,11} → DoAction → LMT104 播放;同一 checkpoint 上源节点的 `R` 链接
  点火 → FSM 进节点1。**动作层与 FSM 各自 authored 对齐,不靠 id 互查。**
- 因此: DoAction 日志里的 id 只对**零入边节点**(居合 101/102、回避 20/21/22 — 靠请求匹配 ActionNo
  进入)成立;链接进入的节点 ActionNo 用的是另一个编号系(1000 系列 = 1000待機/1002,3登段/1005回避F/
  1006,7跳跃/1008味方アッパー/1009スラ納刀/1012待機２/1013ほぼ全攻撃/1014スリまとめ),与请求 id 无关。
  反编译侧注: hook 的 0x140269CE0 在 Ghidra 里不是独立函数(显示为 `func_0x000140269ce0(...)`;
  0x140269c30 是动作实例池分配器),在 dump.exe.c 里 grep "FUN_140269ce0" 永远落空,按调用点
  `(x+0x61c8, &stack)` 搜 func_0x 才能找到。
- 推论(待实测): force-DoAction {3,11} 只播动作,**不会**让 FSM 进入节点1(没有 ActionNo=11 可匹配,
  链接条件 R 又未被满足) — id 11 实质上是"无节点 id"(同 98/99/100/103)。要 FSM 侧同步得满足对应
  按键属性或直接写 FSM 状态。编辑気刃斬り1 的 FSM 行为 → 改节点1(ActionNo 1001);改动作/判定 → 仍走
  request 11 (LMT104/TIML/COL)。

### FSM Studio — browser-based XFsm replacement (2026-09-20, verified)

- **`FSM-Studio/`** is its own independent git repo (nested, like
  MHW-FSM/SPL-Examples — parent-repo commits do not capture it; check
  `git -C FSM-Studio status` there). Edits MHW `.fsm` files in the browser —
  no game launch needed for editing OR binary conversion. TypeScript +
  esbuild, zero runtime deps.
  Run: `npm start` in FSM-Studio (node at `E:\MHW-arts\node-v24.21.0-win-x64`) →
  http://localhost:8123; double-clicking `index.html` also works.
- Opens `.fsm` (XFS binary) AND `.xml` (XFsm MtSerializer export); saves game-loadable
  `.fsm` directly. Deploy target unchanged (`nativePC/hm/wp/wp03/wp03_action.fsm`).
- **XFS format fully cracked + byte-verified** (see FSM-Studio/README.md and
  `research/` python scripts): `XFS\0` + defs block (15 known classes, regenerable
  byte-identically from `src/deftable.ts`) + ClassEntry tree
  (`id=defIdx*2+1`, `var=ordinal`, header u64 = instance count, null ref = `0xFFFE`).
  Two game-written samples round-trip **byte-identically**; 28 node tests (`npm test`).
- Graph editor: layered auto-layout (main chain + request-entered subgraph bands),
  pan/zoom/drag, node/link/condition-tree inspectors, undo, search.
  The 刚1-era workflow (add `VariableNode R` / `cFSMPl_W03` to a condition) is a
  built-in button flow; verified the written binary reparses with the R child.
- **两节点全边交换 (2026-10-07)**: 右键单节点 `与其他节点交换边…` 或框选两节点
  `交换这两个节点的所有边`。实现 = 双方 `mpLinkList` 整表对调 + 全图
  `mDestinationNodeId` A↔B 重指（每条边两端重标注: A→C 变 B→C、C→A 变 C→B、
  自环跟走；条件/顺序随链接实例走，ActionNo/MotionNo/名称/颜色不动）。
  背景: 刚1 改防御时闲置节点全是强化射击节点，直接替换其 ActionNo 触发游戏崩溃
  （待机接气刃斩2 不崩）→ 改为只换接线不动节点身份。
  `FsmModel.swapLinks`, 6 条回归测试；`mExistConditionTrainsitionFromAll`
  节点级字段不参与（wp03 现状 0/90 使用）。
- Limitations: canonical rewrite re-numbers var ordinals (game-written files are
  already canonical); unknown classes (quest FSMs) synthesize defs and warn;
  `mUIPos` preserved raw, not written back.

### XFsm condition-tree authoring (2026-09-14, user-verified)

- Minimal link condition: one top-level `OperationNode` (Operator `None` = passthrough for a single child; AND=16 / OR=17 for multiple), then `Add Child` → `VariableNode`.
- `VariableNode`: Property Name = the condition property (e.g. `R`), Owner Name = `cFSMPl_W03` for the weapon FSM; leave Is Singleton Owner / Is BitNo / Is Array / Is Dynamic Index / Use Index Enum unchecked, Index = 0, Index Variable N/A.
- Conditions evaluate at FSM checkpoints; presses buffer until the checkpoint (pressing R mid-见切 applies at action end).
- XFsm crash fixes shipped (2026-09-14): root-type-change double-free in `AIConditionTreeInfo.RootNode` setter, stale cached wrapper in `XFsmConditionTreeInfo`, unknown-node-type ctor throw. Unhandled C# exceptions now log to `nativePC/plugins/CSharp/XFsm/render-error.log` instead of killing the game; empty log + instant exit = native crash (x64dbg: break on 0xC0000005/0xC0000409, or attach dnSpy).

Build command:

```powershell
& 'E:\MSVC2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -S ValorGauge -B ValorGauge\build -A x64
& 'E:\MSVC2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build ValorGauge\build --config Release
```

### 2026-09-22 R2+方向 combo 不触发根因: 请求层按武器路由 combo, FSM 条件覆盖不到 + DIRREWRITE stopgap

- **Symptom:** custom wp03 FSM link `VariableNode("R+↑")` on node 12 (気刃斬2, the REAL one: node1 --cond20--> 12 --cond21--> node2 気刃斬3) never fired, while plain-R link (tree 307) worked and its node-entry ACTION plays over the vanilla request (log: `3:67 -> 3:101` — node-entry beats the request layer). AND(`R`,`レバー前キープ`) also failed.
- **Root cause chain (all verified):**
  1. The property string was first saved as bare `↑` (byte check: `.fsm` had `e2 86 91` alone at 0x1cc70, `R+↑` 0 hits) — bare arrows are NOT registered properties; only combos (`R+↑`, `回避+↑`, `レバー前キープ`...) exist. Re-export fixed the bytes (`52 2B E2 86 91` present).
  2. With correct bytes it STILL fails: `R+↑` IS globally registered (string table `.rdata` 0x1430B9C34; reflection builder 0x140B25DC1 + name resolver 0x140B2F26B both contain it; wp05/wp08 vanilla FSMs use it) — but the **combo→request classifier is per-weapon**: LS routes only what vanilla wp03 uses (`R`, `R+△`, `R+○`, `△+○`, `△+○+←/→`; ← works on keyboard user-verified via 左移動斬り). `R+↑`/`↑`-combos are never classified for LS → the property value is never written → condition false forever. Vanilla wp03 has 0×`↑`, 0×`レバー` conditions — no LS precedent.
  3. `レバー前キープ` etc. registration accessor = 0x140240720 stub (`xor al,al; ret`) — all cFSMPlBase transition values are written per-frame by the input system, registration table has no real getters. `wp03_param.w03p` (1584B, w03p.bt template in mhw-editor-src) = numeric tuning only, NO combo routing table → routing is code-side (upstream producer of buffered set-3 requests — same open item as the backlog's 大居合 request-id-2 producer).
- **Tool gotcha:** XFsm/XML export vs `.fsm` can silently desync (xml newer than fsm with different property text). ALWAYS byte-verify the deployed fsm: `python -c "print(open(r'...wp03_action.fsm','rb').read().count('R+\u2191'.encode()))"`.
- **DIRREWRITE (shipped, mod build only):** DoActionHook stopgap — while hold key is down and PREVIOUS action == duringId, rewrite incoming {3,68}/{3,101} → {3,102} (钢2一长). Runs BEFORE the action tracker updates g_lastLongSwordAction; local-player locked only; iai-safe (rule scoped to during-action 3:67). Config `nativePC/plugins/ValorGauge.dir-rewrite.txt` = "<duringId> <holdVK> <toId> <fromId> [fromId...]" (default `67 87 102 68 101`; holdVK 0 = off; logs `DIRREWRITE: 3:x -> 3:y`). Caveat: FSM still enters the level-gated node (64/2) while 102 plays — follow-up links route from that node, not 65, until the real routing fix.
- Fixed two latent `"nativePC\p..."` single-backslash escape bugs (counter-auto.txt, prop-trigger.txt — C4129, files never loaded).

### 2026-09-24 气刃量定位: weapon+0x2374, float 0..1 (decompile+指令级 verified; mod 配置定案)

- **气刃量 = `float` at `weapon+0x2374`, 0.0..1.0**（user 在 MHW Editor 确认 0-1 float 语义;
  ValorLS.dll 早就写过它 — old-disasm 0x18000838B `movss [rax+2374h],xmm0` = accum/除数 的渐充动画）。
  HUD 条 getter `FUN_140616d60` (VA 0x140616D60): 门控 `func_141F627F0()==3` (LS),
  `return *(float*)(weapon+0x2374) * 100.0f`（×100 常量 `_DAT_142f16ec4`=100.0）。
  **更正 09-17 "ruled out" 条目: 0x2374 不是红刃持续时间计时器，就是气刃量本身；
  当时的"线性慢漂移"= 量条的自然衰减，所以看起来像 timer。**
- 等级变动时的回填（DropLevel, decompile ~4985040 行）: 等级归 0 → `0x2374=0`;
  降到白(1) → `0x2374 = *(playerParam+0x9AC)`, 降到黄(2) → `+0x9B4`;
  `playerParam = *(*(weapon+0x9B0)+0x7810)`（player+0x7810 结构; 实测两处 = 1.0, 与
  diff_analysis 的 "+0x2374 -> 1.0 @ 升级" 吻合）。居合事件回填 `FUN_142123510`:
  `0x2374 = clamp(0x2374 + *(playerParam+0x888), 0, 1.0)`（钳位常量 `_DAT_142e4fe58`=1.0,
  满量走 `FUN_142122410(weapon,1.0)`）。武器块 ctor 初始化 0x2374=900.0 — 出生即被覆盖, 忽略。
- **`weapon+0x2368` 双重语义 — 未经实测不要当气刃量写**: 它既是居合架势 int flag 0/1/2
  (指令级实锤: FUN_14211cc60 `mov dword ptr [w+2368h],2/0`; remapper 门 `0x2368 != 0`),
  又是 0..1 float 累加器: `FUN_142122410(weapon,float)` (RVA 0x2122410) 指令级实锤
  `addss/minss/movss [w+2368h]` 钳 0..1, 同时写 `+0x236C = *(playerParam+0x804)`(当前消耗);
  每招调用点 ~15 处 (wrapper `FUN_142122f90`, 增益参数表 playerParam+0x808..0x834 每招一格);
  动作校验器 `FUN_140b980c0` 在 `*(uint*)(w+0x2368) > *(uint*)(w+0x236C)` 时把
  185/278/285 重定向 → 294（正 float 的 uint 比较成立）。第二个 HUD getter
  `FUN_140616dd0` 读 `0x2368 × 100`（同一 ctor 绑两根条: widget 槽 +0x90 → 0x2368,
  +0x180 → 0x2374）。int flag 与 float 量在同 DWORD 上的生命周期交错方式 **未决** —
  直接覆盖会破坏居合架势进入 (remapper 门) 和 force-iai 流。Open。
- **第二轮实测: 气刃量 = weapon+0x2368 定案 (user-verified 锁住了)** — target=2 每帧写 1.0,
  可见蓝条钉死 → +0x2368 就是 HUD 那根气刃条。+0x2374 = 等级持续时间计时器（30s 周期）。
  09-17 的"线性慢漂移"观察因此完全恢复名誉 — 那时看到的就是 0x2374 本尊。
- **气刃双累加器 + 每招路径 (指令级实锤 2026-09-24)**:
  - `FUN_142122410` (RVA 0x2122410) = **+0x2368 气刃量唯一 float 累加器**, 3 条调用路径:
    wrapper FUN_142122F90（~15 个每招事件处理器各传一个参数表槽位）、FUN_142123510（居合回填）、
    FUN_142124B20（TIML/motion 参数缩放）。
  - `FUN_14211EF90` = **+0x236C 同款累加器**（10 个直接调用点, 与 FUN_142122410 前 44 字节相同,
    仅 call 转移与 addss 偏移 68/6C 不同; 签名靠 `E8 DF 0F 0A` 区分）。+0x236C 语义未定
    （validator FUN_140B980C0 拿它与 +0x2368 比较; FUN_142122410 每次调用还会把
    `+0x236C = *(playerParam+0x804)` 当"当前消耗"写入）。
  - **每招→槽位映射是代码硬编码**: 每招事件处理器形如 FUN_1417FDAE0: `param表 = func_0x14211C060(weapon)`,
    `FUN_142122F90(weapon, *(float*)(param表+0x82C))` — 调用点覆盖 **+0x808..0x834 共 12 个
    float 槽**（增量+消耗都在表里; w03p 文件里 MHW Editor 只给 5 个消耗起了名, 其余是 Unk）。
    w03p 文件偏移（user 截图: Blade1 Cost 0.15@1185 等）≠ 运行时偏移, 由加载器重排。
- **GAUGEEVT 钩子已实装 (mod build, HkGaugeAdd)**: detour FUN_142122410, 每次气刃变化记
  `GAUGEEVT +0x<callerRva> val=<增量> gauge=<前>-><后> cost=<0x804槽> own=<是否本地玩家> act=<set:id>`,
  40 行/s 限速。配置 `nativePC/plugins/ValorGauge.gauge-log.txt` = "<minAbs>"（默认 0.01,
  滤掉每帧 -0.000556 的 0x2374 衰减; 0 = 全记）。一局就能实测出完整"招式→增量/消耗"表。
  **后续改增量/加消耗/见切清空的实现点就在这个 detour 里**（改 value 再调 original 即可）。
  气刃锁实验保留: `ValorGauge.gauge-lock.txt` = "[target] value"（当前 `2 0` = 关,
  映射采数期间须关以免钳位污染数据; F5 游戏内切换）。
- **实测基线 (user 2026-09-24 局内确认)**: 3:79 突刺 +0.1 / 3:80 上挑 +0.1 /
  3:84,3:85 普斩 +0.2 / 3:11,3:67~3:70 气刃斩系消耗 / 3:87~3:90 兜割系。
- **气刃规则系统已实装 (mod build, HkGaugeAdd detour + DoActionHook)**:
  `nativePC/plugins/ValorGauge.gauge-rules.txt` 三种规则（仅本地玩家, set 3）:
  - `rewrite <set> <id> <delta> [origVal]` — 该动作期间的所有气刃事件改为 delta;
    带 origVal 时只改 ±0.005 内匹配的事件（多事件动作的精准改法）
  - `cost <set> <id> <amount>` — 动作开始（DoAction 接受后）扣 amount,
    给没有原生气刃事件的招式加消耗用; 走 g_originalGaugeAdd（clamp 安全）
  - `gate <set> <id> <minGauge> <nohitSec>` — 动作开始时气刃 ≥ minGauge 则
    注入 pool+0xE4 no-hit 窗口并清空气刃槽（见切配方）; 不足则什么都不发生。
  cost 先于 gate。注入助手 `InjectNoHitLocked` 已从 OpenCounterWindow 提取共用。
  文件当前全部注释（无行为变化）, 实测基线写在注释里。
  - **(2026-09-25 新增) `timer <set> <id> <seconds>` / `timerhit <set> <id> <seconds>`** —
    延长掉级倒计时 (weapon+0x2374), 与原版见切成功延长 (FUN_142123510 +0x2374 += param) 同一原语。
    timer = 动作开始生效 (空挥算); timerhit = 命中才生效 (DmgCalcHook 路径, 每跳叠加, 多段招多次加;
    不受 valor 充能门控影响)。`id 0` = 通配该 set 全部动作。秒数按当前等级的 w03p Gauge_Duration
    (playerParam+0x858/85C/860) 现场换算成 0..1 刻度, clamp 满级; 等级 0 不加。
    日志 `GAUGERULE timer[-hit] act=.. +Ns lv=.. dur=.. countdown=As -> Bs` (上限128条)。
- **rewrite 规则 user-verified 有效**: 突刺 +0.5 / 上挑改 -0.1（增益变消耗）都生效。
- **气刃 Regen 状态破解 (decompile 实锤 2026-09-24)**: 兜割后 `weapon+0x2378` = 持续秒
  （普通 10s/红刃 15s）, 小居合后 `weapon+0x2388` = 持续秒（普通 15s/掐点 30s, 经
  +0x2CED 掐点标志 → +0x2384 选档）; 每帧 updater 递减计时器并给 +0x2368 加
  `dt ÷ regenRate参数`（兜割 rate +0x840/+0x844 由色刃标志 +0x237C 选, 居合 rate
  +0x850/+0x854 由 +0x2384 选）, HUD 由 FUN_140617730 检测计时器 >0 切显示模式。
  **DLL 激活 = 直接写 +0x2378/+0x2388**。w03p regen 参数文件偏移+0x367=运行时偏移
  （file 1233→+0x838 ... file 1261→+0x854）; 同 skew 下 cost 表 file 0x4A1→+0x808..,
  decay 0.05=file 0x49D→+0x804, Unk120=file 0x499→+0x800（+0x236C 每帧衰减的分母）。
  ValorLS 当年写 +0x2388=90.0 之谜 = 激活此 regen 状态（旧"无敌帧"误判再次翻案）。
  **regen 规则已实装**: `regen <set> <id> <seconds> <variant>`（variant 0=兜割 +0x2378,
  1=居合 +0x2388）, 动作开始时只延长不缩短, updater 自动回气+切 HUD。
  **regen user-verified 有效**。回气量控制 (2026-09-24 补): `regen` 第 5 参 = rate 覆写
  （回气速度 = dt/rate参数, 下值快上值慢; 100000 = 只发亮基本不涨; 同时写该 variant 的
  两个色档参数 +0x840/+0x844 或 +0x850/+0x854, 对话期间有效）; 
  `option regen-freeze 1` = 回气期间气刃不降（GaugeAddHook 把负增量清零, 正增量保留）。
  修正: costSlot/regen rate 读取须双重解引用（player+0x7810 是指针, 之前读的是
  player+0x8014 的垃圾值）。
- 工具坑: `disasm_rva.py <rva> <len>` 第二参是**字节数**不是指令数（传 40 只出 ~10 条）。
- **气刃招式红光破解 (2026-09-24)**: user 交换 3:11(LMT104)/3:84(LMT103) 的动作+TIML 后
  红光不跟随 → 红光不在动作数据里。反编译定位: **红光特效 id 按"动作开始时的气刃等级"
  (weapon+0x2370) 选择, 代码硬编码**: 等级 0→0xC134 / 1→0xC135(白) / 2→0xC136(黄) /
  3→0xC137(红)。写入点两层: ① 每招事件处理器 FUN_1417FE1C0 往事件上下文 +0x30 写 id
  （该簇经函数指针表注册, 反编译无直接调用点）; ② 动作定义对象构造器
  （vtable 0x14337A040, 大小 0xCF8）把三连 id 写进动作对象 +0x19B/+0xCDC/+0x19C
  （基础系 {0xC132,33,34}, 另一系 {0xC135,36,37}）。**验证实验: 写 +0x2370=3 再放招,
  出红光 = 管线正常只差等级; 仍不出 = 自定义动作没接上气刃特效处理器层**。
  另: 刀身常驻光环是另一套 — 升级驱动（decompile 6760498 区）经 FUN_142110A90(weapon, id)
  写 weapon+0xAD4（100=白/0x66=黄/0x69=红; 0x67/0x6A=过渡/清除, 后者还会清 +0x2370）。
- **红光二次定位 (user 反馈: 0xC13x = 刀刃光随等级变色 ✓, 但"玩家身体红光"另有其主)**:
  身体特效候选 = **player+0x8808 通知系统**（纳刀研究同款 emitter）:
  `func_140AE0D70(emitter, 0x7D2/0x7DC, 0)`（门控: 动作 87/102 且等级≠0, 或 +0x2398）、
  `FUN_140ADD670(emitter, 0x7D7, 0)`（门控: +0x2398）。注意有等级门 — 交换实验无等级
  正好不出光。每招处理器尾部还有 FUN_141F755C0(player, yaw, param表+0x8AC, 2, 2) 待查。
  （player+0x1510 三连已排除 = 旋转四元数, 非特效 id。）
  **FX 试枪已实装 (mod build)**: `ValorGauge.fx-test.txt` = "<func> <id>"（func 0=ADD670,
  1=AE0D70; id 支持 0x 前缀; 默认 `0 0x7d2`）, **F7** 对本地玩家发射一次, **Shift+F7**
  重载配置。逐个试 0x7D2/0x7D7/0x7DC（红刃状态下）即可锁定红光特效 id。
  地址解析走 RVA + addresses.txt 覆盖（键 fxNotifyA=0xADD670 / fxNotifyB=0xAE0D70）。
- **红光一发定案 (user-verified 2026-09-24)**: `0x7D2` 经 ADD670 对玩家 emitter 发射 =
  **常规气刃斩身体红光**。`0x140AE0D70` 反汇编 = `jmp 0x140ADD670` 纯 thunk, func 参数
  无意义。试枪扫射结果: `0x7D3`/`0x7D4` 也是红光, `0x7D1`/`0x7D9` 无效。**蓝光改走记录路线:
  HkFxNotify 钩子已实装**（detour FUN_140ADD670, 第 12 个钩子; 签名 47 字节, 孪生
  0x201DCB0 靠 +36..46 的 movss/lea 尾巴区分; E8 rel32 和 rip disp32 已通配）——
  本地玩家的每次身体特效发射记 `FXNOTIFY id=0x.. arg=.. caller=+0x.. act=..`
  （同 id+arg+caller 0.5s 去重 + 30 行/s 上限; 非本地玩家 emitter 静默;
  caller > 0x4000000 = 试枪自身的垃圾 RVA, 已过滤）。
- **FXNOTIFY 全族实测 (user 2026-09-24 局内确认)**: `0x7D2`=气刃斩系通用红光
  （caller +0x2122559, FUN_142124F70 通用位; 3:11/67/68/70/87 共用）、**`0x7D3`=红光
  （user 二次确认, 非蓝光）**、`0x7D4`=居合音效+见切无敌帧受击音（FUN_142124F70 两分支,
  3:86/3:102）、`0x7D7`=**见切起手蓝光+音效**（FUN_1417FDAE0 尾部, 3:86 专属双发）、
  `0x7DC`=大居合专用（3:102）、`0x7D1/0x7D9` 无效。
  代码事实: 架势开始通知 `FUN_140AE08A0` 内部硬编码 `mov edx,0x7d3; call ADD670`,
  特效句柄存 emitter+0x1A40 保活, 架势结束 FUN_140AE10F0 检查同句柄移除 — 但 user
  实测 0x7D3 观感为红光, **特殊纳刀/架势蓝光的实际来源仍未钉死**（该次测试未产生
  任何 FXNOTIFY 行, 可能不走 ADD670）。用户裁定当前特效储备够用, 蓝光溯源停止。
  通知函数注意: `0x140AE0D70` = `jmp ADD670` 纯 thunk; 试枪发射的 caller 是我们 DLL 的
  垃圾 RVA（已在 FXNOTIFY 钩子里过滤）。
- **实验已实装 (mod build): 气刃量锁定** — 每帧写 Present 线程（游戏帧更新后）。
  配置 `nativePC/plugins/ValorGauge.gauge-lock.txt` = "[<target>] <value>"（target 1=+0x2374,
  2=+0x2368, 裸数字=target 1; 当前 `2 1.0`）, **F5** 游戏内切换。两字段恒被监视:
  `GAUGELOCK: lock=... | 0x2374 raw=... drift=N/2s | 0x2368 raw=... drift=N/2s`。
- **第一轮实测 (target=+0x2374): 条没有锁住 + 字段活的** — 游戏每帧写 0.999444（恒定,
  = 每帧 -0.000556 ≈ 1/1800, 60fps 下 30s 空周期）但我们写 1.0 后条照常掉 →
  **可见蓝条不是 +0x2374**（×100 getter 是另一根条）; +0x2374 更可能是等级持续时间
  （09-17 "线性慢漂移" 的原始观察因此恢复名誉）。主嫌疑转 **weapon+0x2368**（第二根条的
  getter、每招增益、消耗比较都在它上面）。
- **0x2368 双重语义解法 (指令级新证据)**: remapper 的架势检查是 `cmp byte ptr [rdi+0x2368],0`
  ——**按字节**。若 rdi=weapon, 0.999444(低字节 0x73) 会误判架势毁掉普通攻击 → 游戏没坏 →
  字节检查的对象不是 weapon（大概率 player+0x2368）。而架势生命周期 int 0/1/2 写在
  weapon+0x2368 dword 上 = **进架势把气刃 float 清零**（int 2 = float 2.8e-45 ≈0）, 语义自洽。
  → weapon+0x2368 就是气刃量本体, 架势 = "复用同 dword 做清零标记"。
  锁 target=2 的已知副作用: 锁定期架势每帧衰减检查 (==1/==2) 会被 1.0f 的 int 视图
  (0x3F800000) 绕过, 居合架势退出通知可能不触发（FSM 3:99 的 5s 超时应仍兜底）。

### 2026-09-25 收刀态输入路由: 纯代码+静态数据, 武器 FSM 完全不参与 (R2→3:11 定案)

- **问题**: 收刀态按 R2 打出 3:11 (拔刀气刃斩1), 这个切换能否用 FSM 编辑改道 (改打刚1)?
  **答案: 不能** — 收刀→拔刀动作路由不经过 wp03_action.fsm 的任何可编辑链路。
- 证据链 (全部 decompile/指令级):
  1. `wp03_action.fsm` 没有任何收刀态内容: 待機(1000) = 0入0出 parked 节点; 收刀移动
     1:0/1:4/1:8 **没有节点** — 收刀 locomotion 根本不在这个 FSM 里。持刀 R2 进节点1
     (ActionNo 1001) 靠的是待機２(1012)的 `R` 链接条件, 收刀态没有对应物。
  2. 重映射器 FUN_14118c320 对 id 11 (0xB) **透传**: LS case 只在居合架势 (0x2368!=0)
     重映射 raw id 0→100, 1→101, 2→102, 3→103, 4→104, 0x12-0x22→0x69..0x7d/0x43,
     0x35/36→0xa0/0xa1, 0x8d→FUN_14211f040 分档; 另 id 7 查 weapon+0x2369,
     id 0x5e(94)→0x5f(95)。**收刀态按 R2 产生的 {3,11} 在重映射器之前就已生成**。
  3. 武器类型 switch 的 dispatch: 0x14118C5B6 `mov eax,[rdi+0x9F8]; dec eax;
     cmp eax,0xA; ja passthrough; jmp [table 0x14118D098 + eax*4]` —
     **反编译 case N = 武器类型 N+1** (修正 09-17 "case 2 是别的武器" 的错误警告,
     case 2 就是 LS 的居合架势重映射)。武器类型枚举此表按 1-based 索引。
- **每武器 set-3 请求表 (分类器的数据面, "upstream producer" 的大致形态)**:
  - FSM 管理器初始化 FUN_14118dca0/ddc0: set-1 注册重映射回调 FUN_14118c320 (IB 态
    player+0x7626!=0 时换 FUN_14118d3a0); **set-3 注册 per-weapon 数据**:
    `row = FUN_142165600(weaponType)` = `type*0x130 + 0x143FD5590`,
    `FUN_14026ae00(obj, 3, *(row+0x48), *(u32)(row+0x50))` 把静态表拷进
    obj+(3+8)*0x10 = mgrCtx+0xAB0 (obj = mgrCtx+0xA00)。
  - LS 行 (0x143FD5920): +0x48 = **0x143F4D6A0** (113 条), +0x50 = 113。
    静态条目布局疑似 0x28B {u64 key, ptr 子表, u64 数量, pad×2} (keys 0-4 实测:
    0/4 共享子表 0x143EF3970×14, 1/3 共享 0x143EF3C10×18, 2 = 0x143EF3DC0×15,
    5 = 0x143EF43A0×7...), 但 FUN_14026ae00 的内存拷贝循环按 **0x20** 步长 —
    静态 0x28 vs 拷贝 0x20 矛盾未决 (可能反编译步长有误, 待指令级核对)。
  - 消费链: FUN_14026b670 (请求 {set,key} → 数组[key] 条目, 逐候选) →
    FUN_14026b830 (候选 0x18B 6×i32 解析, 特判 -0x14(-20)/-10/-1 标记;
    -10 = 居合斬り開始标记!) → FUN_14026b590 (最终 {set,id} 经
    func_0x140269ce0 = DoAction 发出)。子表候选里出现 240/291/196/13/278 等 id,
    语义未解码 — **想纯数据改写收刀 R2 行为, 从 0x143F4D6A0 这张表入手**。
- **实用结论 (收刀态打刚1 的路线)**:
  - FSM 编辑不可行 (无链路可改)。
  - **DLL 改写 — 已实装 (2026-09-25, mod build, md5 5fec7f00a4815013d2b16df6478630e5)**:
    DoActionHook 内、tracker 拷贝之前, `ApplySheathRewrite` 把收刀态进来的
    {3,fromId} 改写为 {3,toId}。门控链: 本地玩家 (锁 + 未锁时 manager chain 判定,
    首次按 R2 即生效) → fromIds → holdVK (可选按键门) → weapon+0x9F8 == 3 →
    **收刀判定 = tracker 里上一个动作是 set 1** (1:0/1:4/1:8/1:5 收刀 locomotion 族;
    持刀态 locomotion 不发请求) → **红刃 weapon+0x2370 >= reqLevel** →
    **气刃量 weapon+0x2368 float > minGauge** (user 规则: 红刃且余量 > 0.15 才允许
    拔刀刚1)。不满足则放行原版 {3,11} 拔刀气刃斩1, 拒绝原因记
    `SHEATHREWRITE reject` / `SHEATHPROBE` (各上限12条)。命中记
    `SHEATHREWRITE: 3:11 -> 3:86`。改写在 tracker 之前 → clang-latch 重臂/日志/
    gauge 规则看到的都是改写后的 id。
  - **无敌帧 (2026-09-25 二轮)**: 改写生效后动作出来了但没无敌帧 — 原生防御窗口
    走 TIML/FSM 状态链, 收刀态 FSM 停在待機(1000) 不进节点28, 窗口没人打开。
    解法: 改写命中同一帧调 `InjectNoHitLocked(nohitSec)` 注入 pool+0xE4 no-hit
    (与叮触发同一通道), 配置第5参 nohitSec 默认 0.2s (与见切 TIML 编写的窗口一致),
    <=0 关闭。**注意: 持刀态的见切叮触发链 (bit15→no-hit+auto-force) 是否会在
    收刀路径照常工作未验证 — 若 bit15 不置位, 收刀刚1 只有这 0.2s 窗口。**
  - **第一版失效复盘 (log 实证)**: 收刀按 R2 的请求确实是 {3,11} (tracker 日志多次
    `1:5/1:0 → 3:11`), 但无任何 SHEATHREWRITE 行 → 静默门控 = **weapon+0xC71 收刀态
    读到 0**。+0xC71 的极性/语义从未局内验证过 (一直标"待钉") — 已弃用, 改用
    tracker set-1 判定; SHEATHPROBE 行继续打印 c71 实测值, 供钉死其真实语义。
  - 配置 `nativePC/plugins/ValorGauge.sheath-rewrite.txt` =
    "<toId> <holdVK> <reqLevel> <minGauge> <nohitSec> <fromId> [fromId...]", 默认
    `86 0 3 0.15 0.2 11` (toId 0=关, holdVK 0=无按键门, reqLevel 0=不验等级,
    minGauge<0=不验气刃量, nohitSec<=0=不注入)。文件缺失 = 默认开启。
  - 注意: 见切(86) 动作从持刀姿势起手, 收刀冷播有姿势跳变 (刚1 为移植动作时需要
    收刀起手版); {3,86} 能否让 FSM 从待機(1000) 经 request-ActionNo 匹配进节点 28
    未实测 (不进也照播, 98/99/100/103 无节点前例, 只是派生链接断)。
  - 居合架势态 R2 的 raw id 是 1/2 (remap 成 101/102), 与收刀态的 11 不冲突,
    改写规则天然不误伤。
  - 顺手修复: counter-auto.txt / prop-trigger.txt 的 `nativePC\p...` 单反斜杠
    路径 bug 再次出现 (C4129, 配置从未加载) — 已改为正斜杠。
  - **事故复盘 (2026-09-25, user 局内确认): gauge-rules 的 `gate 3 86` 会杀死整个
    见切反击链。** 机理: gate 在动作第 0 帧注入 pool+0xE4 全量 no-hit — 该排除在
    命中查询层、比防御仲裁更上游 → 怪的攻击到不了仲裁 → 无弹反交互 → 无叮 →
    event bit15 永不置位 → mod 叮触发链 (反击窗口+追加no-hit+auto-force刚1) 全灭,
    且每次见切清空气刃槽 (log: `GAUGERULE gate act=3:86 ... gauge cleared` ×10+)。
    **定案: pool+0xE4 no-hit = "游戏层面你没被攻击过" (09-18 翻滚结论的另一面);
    帧杀保护与弹反计数互斥。叮类招式 (见切/居合) 的保护必须后置于叮 (bit15 先行,
    注入殿后 — mod 叮触发器的既有顺序)。** gate 规则只适用于无弹反呈现的自定义招。
    rules 文件示例行的 `#gate 3 86 0.1 0.5` 已换成 3:87 并加警告。

### 2026-09-25 (2) 气刃等级自动下降 + 气刃量自动衰减全链路 (decompile+w03p模板实锤, 站桩秒表未做)

- **总更新器 = FUN_1421237B0 (RVA 0x21237B0, 纳刀研究同款每帧函数)**。两套衰减独立:
  - **等级计时器 = weapon+0x2374 (float 0..1)**: 每帧 `- dt' × (1/Duration[level]) × (1.0/FUN_1411c27c0(mgr))`,
    dt' = `*(*(player+0x998)+0x68)`。Duration 按 0x2370 选 1/2/3 档 = playerParam+0x858/0x85C/0x860
    = **w03p `Gauge_Duration_White/Yellow/Red`** (用户冰原版实测 195/135/60; 文件偏移 1265/1269/1273,
    skew +871 ✓)。倍率函数 FUN_1411c27c0 = 属性 0x35 → 表 `_DAT_145011f68+0x5CC+(idx-1)*4`, 默认 1.0。
    门控: event bit0 (player+0x8938 &1, 全部气刃增减共用此门) **且 +0x2390 ≤ 0**。
    **+0x2390 = 升级后免衰减倒计时** (初值来自 w03p `Gauge_No_Decay_Time_after_Spirit_Gauge_Upgrade`,
    playerParam+0x884; 每帧在 bit0 下减 dt', 归零钳 0 → 衰减开始)。
  - **到 0 处理 (updater 内)**: level≥2 → level-1 + 计时器写 1.0 (回满) + FUN_140ae1630 通知;
    **level<2 (白) → level 直接归 0 且 +0x2370..+0x2374 一起清零 (undefined8 写)**。
  - **气刃量 (可见蓝条) = weapon+0x2368**: +0x236C 累计器以 `dt'/Spirit_Gauge_Decay(playerParam+0x800)` 速率扣,
    过 0 时蓝条一步扣 `param+0x804` (=Blade1_Cost 槽, FUN_142122410 每次气刃事件写 +0x236C=+0x804
    所以攻击会重置待扣批量) 并重置累计器 — **净速率 = 1/Spirit_Gauge_Decay 每秒, 与 +0x804 无关**。
    回气状态 (+0x2378 兜割/+0x2388 居合 >0) 时改为 `+0x2368 += dt'/rate(+0x840/844 或 +0x850/854)`, 不衰减。
- **刷新路径 (计时器重置/延长)**:
  - 升级/掉级事件都回填: 掉级事件处理器 **FUN_141822790 (DropLevel, 函数指针注册无直接调用点)**:
    level-1 后按新等级回填 — 白←playerParam+0x9AC, 黄←+0x9B4, 归0→清零; 另有兜割/居合的
    **等级消耗 handler FUN_142122E40**: `param+0x864 (w03p "Gauge_Consumption_for_Helm_Plus_Iai_Spirit_Slash",
    用户实测=1) <= level` 时扣该数值等级并写 +0x2374=1.0; f20/f50 两个变体, f50 在掐点 (+0x2CED) 时跳过。
  - **见切成功 = weapon+0x2399 锁存** (FUN_142125060: set3 且动作 86=0x56 时置位, 发 FX 0x7D4,
    同时写 +0x23A0 = playerParam+0x8A8 = w03p "Foresight_Slash_Super_Armor_Time_on_Successful_Evasion"
    — 09-17 的蓝光/闪白探测器字段就是它, 且 +0x23A0/+0x23A4 >0 期间每帧发 event bit 0x97(151)=超强呈现)。
    动作结束处理器 **FUN_142123510** 消费 (+0x239C 一次性锁存, 动作开始处理器清 +0x239C):
    +0x2399 置位 → **FUN_142122410(+1.0) 气刃量回满 + +0x2374 += playerParam+0x888 (延长, clamp 0..1)**。
    该函数还管: 动作 101(小居合)→+0x2388 回气档 (掐点←+0x2CED→+0x84C, 否则保底 +0x848);
    89-92(兜割系)→+0x2380 = 等级快照; 动作 69 且 bit0 → +0x2CED=1。
  - **居合弹反 handler FUN_142124F70**: set3 且动作 ∈{101,102} → +0x2CED=1 (掐点) + FX 0x7D4 +
    +0x23A4 = playerParam+0x8B8 (w03p "Iai_Super_Armor_on_Successful_Counter", 与 +0x8A8 恰差 0x10 ✓)。
  - 见切整条消耗: 每招 wrapper FUN_142122F90 内, 动作 86 时 `+0x2368 -= 1.0` (见切花掉全部气刃量)。
  - 大重置 FUN_142124590: 清 0x2368-0x237C/0x2388-0x23A4/0x2399, +0x238C=1.0, +0x23C4=7 (收刀/换区/重置)。
- **数值核对**: 09-24 GAUGELOCK 实测 +0x2374 每帧 -0.000556 (=1/1800) — 若当时红档且 60fps,
  则红档 30 壁秒见底 = 参数 60 的一半 (该时钟 30Hz / 参数单位≈2壁秒); 也可能另有 ×2 修正。
  **站桩秒表一测即知** (红→黄多少秒)。模板字段名→文件偏移: Blade1_Cost@1185, Gauge_Duration*@1265-1273,
  Consumption@1277, Damage_Buff*@1281-1289, NoDecay@~1309, 见切延长 Unk_125@~1313 (前四组用户截图核对 ✓)。
  **注意**: 本地 chunk-extracted/G0 与 nativePC 两份 w03p 内容相同且全文件搜不到 195/135/60/15/30/0.15
  — 编辑器里看的是冰原版数据, 改 w03p 前确认导出到 nativePC 的是编辑器那份。
- **mod 相关**: sheath-rewrite 的红刃门 (+0x2370>=3) 会被等级自动下降在 30~97s 后打断 (红→黄) —
  需要长效红刃可每帧写 +0x2374=1 或写大 +0x2390; 09-24 gauge-lock target 1 = 锁等级计时器 = 永不掉级。
- **(2026-09-25 晚) GS 崩溃取证 + 规则系统硬门控**: 持大剑 (weapon+0x9F8=0, GS 原始类型=0) 挥砍必崩,
  WER 两次崩溃同偏移 `MonsterHunterWorld.exe+0x232E245` (0xC0000005) — 游戏代码非本 DLL。
  崩溃指令 `cmp [rdx+0xB0],0` (rdx=野指针容器), 该处 = 辅助函数 0x14232E240 ("取容器第 index 个校验条目",
  0x18 步长 + +0xC&0xE 校验, 仅经 lea 函数指针引用无直接调用点); 调用方 FUN_140ADE270 (FX notify 家族,
  读玩家 +0x8938 event bit2 门控), 容器来自 FUN_140ADECC0 消息查询应答。GS 会话日志止于
  `FXNOTIFY id=0x7d0 caller=+0xae042b` (0x7D0 = GS 专属特效, LS 从未见)。GS 会话本 DLL 零写入:
  全部 gameplay 路径被 ModWeaponActive 关死 (GATE CLOSED 贯穿全程), 无 HITATT (空挥未命中),
  timerhit/timer 规则不可能执行。ADD670 经反汇编确认只读 rcx/edx/r8d 三参 — FXNOTIFY 3参转发无罪。
  结论: 非本次 timer 改动所致; 待 DLL 移除 A/B 定案 (vanilla GS 特效路径 vs 旧钩子交互)。
  **顺手修掉真地雷: ApplyActionGaugeRules 原本无武器门控** — 规则 id 与 GS 动作 id 撞号 (或通配)
  会对 GS 武器结构体跑 LS 专属 gauge 原生函数/偏移 (越界写)。现 ApplyActionGaugeRules 与
  ExtendLevelDropTimer 均有 weapon+0x9F8==3 硬检查 (ca10904, 部署 md5 c0fb69de)。
- **(2026-09-25 深夜) GS 蓄力崩溃 ROOT CAUSE + 修复 (54cc5d2, 部署 md5 3ac107ff, user-verified)**:
  user A/B 定案 (DLL 在就崩/不在就好) + 关键症状 (仅蓄力时崩, 蓄力有红光) → **FxNotifyHook 把
  FUN_140ADD670 的返回值丢了** (钩子声明为 void)。反汇编实锤 ADD670 结尾 `mov rax,rbx; ret` —
  返回创建的特效句柄; 太刀调用点 (0x7D2/0x7D3/0x7D7 一次性特效) 不消费返回值所以几周没炸,
  GS 蓄力红光 (0x7D0, caller +0xae042b) 的调用点继续使用该句柄 → 垃圾 rax → 野指针 →
  0x232E245 `cmp [rdx+0xB0],0` 崩溃。**修复: FxNotifyHook/g_originalFxNotify 改 void* 转发句柄;
  同类隐患 GaugeAddHook/g_originalGaugeAdd 改 float 转发 (FUN_142122410 在 xmm0 返回钳位值);
  g_fxNotifyA/B typedef 同步 void*。教训: 每个 detour 必须核对原函数的返回值寄存器 (rax/xmm0),
  声明成 void 丢返回值是隐形炸弹, 症状只在"调用方消费返回值"的路径上爆发。**
  **user-verified**: 修复后 GS 蓄力不再崩溃; 且 F7 试枪可直接调出大剑蓄力红光。
  **FX id 按"当前手持武器"分表解析 (user 实测更正)**: 同一 id 0x7D0 — 大剑在手 = 蓄力红光,
  太刀在手 = 普通气刃特效。特效库所有 id 都必须标注"哪个武器表下"; 跨武器借特效 (如太刀出
  大剑蓄力光) 靠改 id 做不到, 得走别的路 (其他 notify 函数/特效资源层)。0x7D0 记录:
  **FX id 按"当前手持武器"分表解析 (user 实测更正)**: 同一 id 0x7D0 — 大剑在手 = 蓄力红光,
  太刀在手 = 普通气刃特效。特效库所有 id 都必须标注"哪个武器表下"; 跨武器借特效 (如太刀出
  大剑蓄力光) 靠改 id 做不到, 得走别的路 (其他 notify 函数/特效资源层)。0x7D0 记录:
  蓄力红光(大剑表) / 普通气刃(太刀表)。

### 2026-09-26 动作帧控制全图: ActionController/动作实例 + 起始帧跳转 (decompile 实锤, FRAMETICK 局内验证待做)

- **问题: 能否让动作从第 n 帧开始播?** 结论: **没有原生"带起始帧播放"API** — begin 硬编码清零
  帧计数, 唯一路径 = begin 后直接写 `instance+8`。已实装 (mod build, 部署 md5 c9991130, 含 Shift+F6 热重载)。
- **ActionController = player+0x61C8** (已知 player+0x6274/0x6278 就是它的 +0xAC/+0xB0)。
  全字段图 (decompile: DoAction 0x140269CE0 是个小函数, 更新器 FUN_141f78600 / FUN_14026a820):
  - `+0x68+set*0x10` = instArray 指针, `+(set+7)*0x10` = 该 set 实例数; 实例按 (set,id)
    **预分配全局唯一** (池 FUN_140269c30, 注册表见下)。
  - `+0xA8` = phase (0 idle / 1 请求已接受 / 2 运行中); `+0xAC/+0xB0` 当前 set/id;
    `+0xB4/+0xB8` pending; `+0xBC/+0xC0` 缓冲请求 (DoAction 在 phase!=0 时写这里);
    `+0xC4/+0xC8` 前一动作, `+0xD0` 前一实例 core, `+0xD8` 前一动作结束帧 (只写不读);
    `+0x6A8` (=player+0x6870) = **动作自动结束阈值 float** (<0 关; frame 达到即 end+switch);
    `+0x6AC/0x6AD/0x6AF` 结束标志。
  - DoAction 全逻辑: 校验 set 范围+实例存在 → phase==0: 直写 +0xAC/+0xB0, phase=1;
    否则缓冲 +0xBC/+0xC0。真正的 begin 发生在每帧更新器里 (a820: phase==1 → 收尾旧动作
    a660/begin 新动作 a1e0, phase=2)。
  - **动作实例**: `+8` = **帧 float** (begin 清零; 每帧 `frame += dt`); `+0xC` = 当前 dt;
    `+0x10` = core 指针 (0xCD0 字节每动作运行时对象, 工厂 FUN_1417fada0 族);
    `+0x18` = finished byte; `+0x28` = player。
  - getter 族 (全 thunk, 简单字段读): 0x141bff720/740/760 = {set,id} 指针 / id / set,
    stride 0x6B0 = **控制器槽步长** (slot0=player+0x61C8, slot1=player+0x6878);
    0x14026a420/450/490 = 当前实例 / core / 帧。
  - 实例 vtable: +0x30 = **begin** (见切类 = FUN_1417FDAE0 — 就是笔记里的"见切起手处理器",
    内部: gauge wrapper → 通用 begin FUN_141783470: 存 yaw、发 event bit 0x63、vtable+0x288);
    +0x38 = phase/finish 检查; +0x40 = end (FUN_14026a290); 旧动作收尾走 +0x48 (FUN_14026a660)。
- **每武器动作类表** (FUN_141f5a410 换武器时注册): 行 = FUN_142165600(type) =
  0x143FD5590+type*0x130; LS 行 0x143FD5920: `+0x40` = **set-3 类表 0x142EF5C30** (113 项,
  与请求表 0x143F4D6A0 同一 count 字段 +0x50), `+0x70` = slot1 类表 0x142EF75C0 (8 项)。
  表项 stride 0x20: {id u32, set u32, classPtr, pad}。**每动作一个类**, 类对象在
  .data 0x144FExxxx 区; **类名 ASCII 就嵌在 .rdata 里 vtable 后面**
  ("nHmAction::Wp03:..." 家族; vtable 0x143382be0+0x20 起即名字) — 可整表枚举动作类名。
  set-1 locomotion 类表 per 武器: FUN_141bfcf80(player,1,0x142e6c640,0x138,0) 等 14 处调用。
- **帧控制实装 (mod build)**: `force-action.txt` 第 4 参 `startFrame` (>=0 启用)。force 被
  DoAction 接受后自动武装: **FRAMETICK** 诊断前 24 tick 记 `instance+8/+0xC` (每次强制动作都
  记, 不启用 seek 也先跑 F6 验证帧计数语义 — 预期 0,dt,2dt... 递增), **FRAMESEEK** 在
  控制器 phase==2 且 frame<4 (begin 刚跑完) 时一次性写 `instance+8 = startFrame`。
  走 property-getter 钩子 (游戏线程)。日志: `FRAMETICK act=.. phase=.. frame=.. dt=..` /
  `FRAMESEEK act=.. frame a -> b` / `FRAMEWATCH done|timeout`。
- **警告**: 前向跳帧会**跳过中间 TIML 事件** (事件按帧越过触发): 碰撞窗/音效/FX 若在
  0..n 之间就不会发生 — 见切 (弹反窗 f0-f12) 跳过 12 就废掉弹反; 无弹反呈现的招随便跳。
  motion 是否完全跟随写入值 = FRAMETICK/局内第一验证目标 (若骨架不动, 说明 motion 采样
  另有时间源, 需再挖 motion 对象 weapon+0x468/0x470 链)。
- **首轮 FRAMETICK 实测 (2026-09-26, user F6 两次 3:104)**: `dt = 0.03333 = 1/30` —
  **instance+8 单位是秒 (30Hz 逻辑帧)**, TIML 60fps 帧 n ↔ n/60 秒; force 后第一轮读到的
  0.166667 是该实例**上次运行的残值** (begin 还没跑), 之后 0.0333 = begin 清零后第一帧 ✓
  清零语义确认。property 钩子每游戏帧触发 ~10 次, FRAMETICK 已改"值变化才记" (否则 24 条
  配额 2 帧烧完)。`endAt` (控制器 +0x6A8 自动结束阈值) 已加进 FRAMETICK 行。
- **二轮实测 (user "无效" 定位): 单次写在 begin 前会被洗掉** — 日志: FRAMESEEK 写 0.667 →
  读回 0.667 → 下一帧 0.033 (begin 清零重开)。时序: begin 在控制器更新里, **晚于我们钩子的
  首个匹配 tick**; force 后首帧读到的 0.133/0.166/0.633 残值 = 该实例上一轮运行的遗留
  (DoAction 走缓冲路径时 current 不变、phase 也不变 — phase 不能用来判断 begin 已跑)。
  修复: 改为 "帧回落到 ≤2dt (新 begin 证据) 后写, 写后每 tick 钉住, 直到时间线超过目标
  半个 dt 才 settle"; 动作中途切换则放弃。endAt(=控制器+0x6A8) 实测 3:104 期间恒 -1
  (自动结束阈值未启用, 动作寿命由动作类自己管)。部署 md5 3d086837。
  **三轮实测: 钉住成功 (settled act=3:81 frame=0.700, 2 writes 自愈 ✓) 但画面无跳转 —
  instance+8 只是逻辑/记账时间线, 骨架动画另有时钟。已加 CLOCKMINER: 强制动作期间自动扫描
  两个区域找"每帧恰好±dt"的 float — A=动作 core (instance+0x10, 0xCD0), B=动画组件
  (*(player+0x468), 0x200; prime suspect anim+0x10C, 原生 getter 0x141bff780 读它做
  "步进未完成"判断), FRAMETICK 行新增 mot= 字段, 第 28 tick 报告 CLK A/B 命中偏移。
  部署 md5 1df8bae5。v1 空手而归 (anim 区无 ±dt 字段; instance+0x10 core 实测不可读 —
  "core" 不在 +0x10)。v2 (md5 a6357ca8): 改"任意恒定步长"检测 (motion 时钟可能是 TIML
  帧数 +2.0/tick, 不是秒), 扩区域: core/anim468(0x400)/anim470/weapon(0x2450, TIML 采样
  目的地+0xA50/+0x23C0 在 weapon 上)/playerA(0x400-0x900)/playerTail(0x1D600)/inst(0x60)。
  平表扫描: 动作id→LMT entry 的 u16/u32 平表不存在, 类对象里也无 LMT 字段 — LMT 选择逻辑
  在代码/类工厂深处。**但 mot= 字段直接命中**: **MOTION 时钟 = *(float*)(*(player+0x468)+0x10C)**,
  计 TIML 帧数 (+2.0/逻辑tick, 60/s), 动作/motion 每次 (重)启动归零, 骨架采样吃它 —
  instance+8 纯逻辑线, 写它画面不动 (v2 实测 mot=62 纹丝不动)。原生 getter 0x141bff780
  (=anim+0x10C) 的"步进未完成"判断语义吻合。FRAMESEEK v3 (md5 92343850) = 双写 instance+8
  (秒) + mot (秒×60 TIML 帧), 同套 fresh-begin+钉住+settle 逻辑。
  **USER-VERIFIED 2026-09-26: 画面跳帧成功** (3:81 目标 0.8s: `frame 0.033->0.8 mot 0->48`
  → `settled frame=0.833 mot=50`, 局内确认从中段起手)。CLK 报告: anim468 上有 5 个同步
  +2/帧 时钟: +0x10C(已写)/+0x110/+0x124/+0x128/+0x22C (疑似动画层/混合同步时钟, 只写
  +0x10C 即可视觉生效, 其余待用); inst+0x8 (step=dt) 交叉验证。v2 挖矿报告 bug 教训:
  属性钩子 ~10次/帧, "tick 28" 只等于 2-3 个逻辑帧 → 恒定步长命中数永远不够阈值 —
  已改为每个逻辑帧只挖一次 (frame 值变化才挖)。v2 挖矿报告 bug 教训:
  属性钩子 ~10次/帧, "tick 28" 只等于 2-3 个逻辑帧 → 恒定步长命中数永远不够阈值 —
  已改为每个逻辑帧只挖一次 (frame 值变化才挖)。
- **Shift+F6 = force-action.txt 热重载**
- **Shift+F6 = force-action.txt 热重载** (原本只在插件 init 读一次; Shift+F7 是 fx-test 的,
  别搞混)。热重载会连带重载 weapon-type/hit-charge/gauge-lock/gauge-log/gauge-rules/
  sheath-rewrite 子配置。startFrame 参数单位 = 秒。
- 工具坑: disasm_rva.py 第一参是 **RVA 不是 VA** (传 0x1417fada0 会 "not mapped")。
- **更新恢复文档**: 本章全套(控制器/实例布局、motion 时钟、类表、动态恢复总路线)
  已按锚点/步骤/验证/写回格式收录 `Reverse/ADDRESS_RECOVERY.md` 第 7 章
  (原第 7 章顺延为第 8 章); FRAMESEEK 不新增钩子地址, 会过期的只有结构偏移。

### 2026-09-26 (2) spiritreckoning（居合开刃）逆向: 轮询+缓冲槽写入的动作衔接 (定案: 整体无改进价值, 可借两个原语)

- **文件身份**: `Reverse/spiritreckoning.dll.c` = `.bak-20260901` 旧版的反编译
  (指纹: 0xc06d 立即数只在 bak 的轮询线程里); 交付版
  `MHW-MHRS-migrate/spirit-reckoning/nativePC/plugins/spiritreckoning.dll` 是重编新版,
  机制相同、偏移已更新 (0x2CEC→0x2CED, 新增读 0x2370 等级/0x2D24, anim 标记换 0xC132)。
  DLL 内部名 `居合开刃.dll`, 导出 `?Load@@YA_NXZ` (Stracker 插件约定)。
  `MHW-MHRS-migrate/居合开刃源码/` 工程**不是**它的源码 (无对应偏移, 是另一模板工程)。
- **机制 = 后台轮询线程, 不是 hook**: `Load()` 起两个线程 — FUN_18000c7d0 (按键监视,
  GetAsyncKeyState WASD+配置VK → DAT_180064e2c 移动计时) 和 FUN_1800134f0 (**动作链轮询**:
  先 Sleep(10s) 等加载, 然后 5ms 一圈, QPC 高精度睡眠 FUN_180009840)。
  配置在 `nativePC/plugins/Reckoning/key_config.json` (热键 VK)。
- **三个信息源**: ① 当前动作 = `player+0x6274/+0x6278` (我们已知的 controller+0xAC/+0xB0);
  ② **动作时钟 = `*(float*)(*(player+0x468)+0x10C)`** (就是我们 09-26 破译的 motion 时钟,
  TIML 帧 60fps) 跟**手调帧阈值**比 (5/60/70/78/87/110/117/134/200/206 硬编码);
  ③ 动画状态标记 = `*(int*)(anim+0xE9C4)`: 迁移招自定义标记 0xc06c/0xc06d,
  原生气刃系 0xc132/0xc138。**没有"动作完成"事件/回调, 纯帧数轮询硬衔接**。
- **衔接原语 = 写控制器缓冲请求槽**: `*(int*)(player+0x6284)=set; *(int*)(player+0x6288)=id`
  = controller+0xBC/+0xC0 — **正是 DoAction 忙时缓冲请求用的同一个槽** (09-26 控制器全图),
  游戏更新器在当前动作可派生的帧自己消费 → 后招在原生衔接帧播出, 观感如原生派生。
  旧版 104 链: anim 标记 0xc06c 且 motion≤5f (起手) 武装 pending {3,0x68};
  current==3:0x69 (105 在放) → 写 {3,0x68} 进缓冲槽 → 105 结束自动接 104。
  其余链段: 3:0x45 到 200f→写{3,1}或{3,0}; 3:0x6b 到 78f→{3,7}; 3:7 到 134f→{3,0};
  全是"到帧数就写缓冲槽"。注意 0x6284 的 qword 写 = {set,id} 一次写双字。
- **唯一一次函数调用**: 0xc06d 招 motion>70f 且 weapon+0x2CEC==1 (motion<70f 时清锁存)
  → `call [rip→槽0x630B0]`, rcx=**weapon**。槽值烧死 **0x1421211F0** (旧版) /
  交付版槽 0x5E0A0 烧死 **0x142121190** (当前版本, 反汇编显示写 weapon+0x23A8 一带,
  在太刀气刃状态簇 0x212xxxx, 邻接 FUN_142122410/FUN_142123510)。
  全局槽 (旧0x63060/新0x5E058) 烧死 **0x145011760** = 玩家链根 (`[它]+0x50`=player)。
  **两个槽都没有重定位表项** → 版本锁死, 跨更新必死, 与旧 ValorLS 硬调 0x142134B70 同款死法。
  (PE 分析工具向: bak .data 在 RVA 0x63000 起, 交付版在 0x5E000; rip-rel 引用暴力扫描
  时别忘了加 TEXT_VA 基址, 否则目标全偏 0x1000。)
- **定案 (user 判定): 整体无改进价值** — 5ms 轮询 + 手调帧阈值 + 烧死地址, 比现有
  钩子体系糙。可借用的只有两个原语: ① **写 controller+0xBC/+0xC0 缓冲槽排队后招** —
  我们没用过的原语, 比 force-DoAction 更贴原生 (游戏自己挑衔接帧/走正常取消窗口),
  刚1 链或 Valor 派生链可试用, 放游戏线程钩子里写比它的 5ms 旁路线程稳;
  ② motion 时钟 ≥ 阈值即"完成"判定 (帧÷60=秒), FRAMESEEK 场景可直接复用。

### 2026-09-27 MHGU→MHW 动画迁移链调通(mhgu-valor;坑目录见 valor-remap-pipeline/mhgu/README.md)

- 工作文件 `mhgu-valor-mhw.blend`(= MHW-Valor 副本,MHRS 155 动作保留在 MASTER 未动);
  脚本 `valor-remap-pipeline/mhgu/0~7`。数据事实:MHGU LMT 单位=厘米(÷100);重心骨=GLB `0:0`,
  其 locY ≡ 原生 106 MhBone_000 locY(0.4713/0.4714 同源铁证);地面位移在 -1 Root 轨道(LMT)/
  `reference.loc`(GLB),轴映射 obj[1]=−rawZ;w07::106 与 w03_00::106 仅重心通道同源,肢体不同。
- 局内"缩成一团"= **导出轨迹缺失**(15 条,只有 IK+根),不是 IK 数值错:FreeKinetics 按原生
  entry 轨迹布局逐轨导出,remap 直接接 entry 会被 omitted。修复=`7_merge_into_native.py`
  原位合并进原生 entry(luocha 路线);配 4-bit 帧增量 ≤15 切分 + frame0/末帧键 + 排序后切分
  (三 bug 曾叠加:未排序数组 gap 算出负数静默跳过切分)。
- 其余已钉:ARP 前必须抽走源动作的 object 曲线(否则地面位移污染重心位置);000 永不写旋转
  (0:0 旋转走腰 013,按用户 bmap 原样);0:0 loc 逐字拷贝到 000(轴恒等);object 位移直通不镜像;
  场景 frame_end 污染 ARP 批量长度 + remap 丢 freehk 元数据 → 钉范围+裁剪+拷贝;
  fill 拷 106 曲线要按目标长度裁剪;根管线 plw_ 过滤会空跑(验证必须断言处理数量);
  v67 解析器偶发丢 entry(重解析并集重试);LMT 导入 channelNames 第一元素才是真路径。
- **2026-09-27 局内验证通过(最终):entry 103(樱花 sa::151)+ 104(刚1 w07::192)=
  自烘焙 IK(米)+ 6_tag 全量打标 + 导出器自动 m→cm = 正常,脚手跟随迁移动作本身**。
  (此前"自烘焙 IK 缩成一团"的真正原因 = 打标缺失导致导出省略身体轨迹 + IK 位置
  以米冒充厘米差 100 倍;全量打标后导出器自动转 cm,自烘焙 IK 可用,
  PRESERVE_NATIVE_IK=1 仅作后备。证据链:w03_00-wrongik.lmt。)全部 25 个 remap 已全量打标+min-2-keys,可直接导出。
  待办:其余招式槽位映射(MERGE_PAIRS 批量 7_merge)+ 绝对回避定位(不在 w07_sa 8 动作内)。
- **2026-09-28 "缩成一团"复发定案(已重新部署, md5 c375f133, 待局内复测)**: 部署文件
  entry104 = 79 轨 + 缺 (-1,3)/(-1,4) 根位移轨 + IK/重心米值(249loc[0]=(-0.38,0.10,0.32))
  = wrongik 同签名。**根因(受控实验复现,逐值一致) = `LMT::w07::192_remap` 缺
  `freehk.tetherFrame` 绑定**(ARP 新建动作不设它;原生导入的 103/104 = m_mesh Armature)。
  导出器 LMTActionParser 的 tether 取自动作的 freehk.tetherFrame 而非场景: tether=None →
  `restoreRootFromObject` 不跑(Root 两轨没了) + 骨架逆变换跳过(米值 float 直写)。
  曲线级打标救不了动作级绑定。**修复 = `act.freehk.tetherFrame = m_mesh Armature` 一行**;
  6_tag 已内置绑定+verify 断言。补绑后 remap 直连导出 = 81 轨 cm ✓ 与管线产物逐值一致
  (remap 缺 MhBone_252 旋转 → 81 vs 82 轨,9-27 同配方局内已验证无影响)。
  和名字/接哪个动作无关,只认 tetherFrame。一键复跑+五文件对比: `mhgu/_rerun_and_compare.py`。
  坏文件留证: `nativePC/.../w03_00-curled-20260928-2000.lmt`。
  **下一主任务(2026-09-28 用户定): 武器位置** — 游戏内刀位 vs Blender 预览不一致
  (见 mhgu/README.md 已知问题: 030/047 挂点常量偏移/游戏挂点数学/scale-Y 插值待钉);
  逆向侧已有基础: 2026-09-20 纳刀合并机制章节(挂点表控制器 +0xAF0/+0xFA0/+0x1450、
  行表 [表+0x4A8]+row*0xC0、vtable+0x110 写 ctrl+0x181)。
  **(2026-09-28 晚续) 刚1 游戏全程左手持刀定案**: 用户原版对照(103/104/105/107/108/
  110/111 一致, 换手的 109/113 也一致)排除游戏数学偏移; 实测 192_remap 的 030.scale-Y
  恒 2 / 047.scale-Y 恒 1 → 状态对从未写入。**原版换手参照配方**: 109/113 的 030/047
  scale = bufType 5 + 同帧成对键瞬时台阶(109: 047 f69=1→f70=2→f128=1, 030 f69=2→
  f158=1); 补阶梯按此打标, 勿用 104 的 keyless bufType=1。
  **fcurve 归组**: 9_group_channels.py 批量归组(组名=骨骼名, object=Object; 两 blend
  已跑: 61378+67462 条), 7_merge 重建时自带归组不再丢。
  **(2026-09-29) 刚1 换手不生效真因闭环**: 用户写好 5 键阶梯后导出, 部署文件 entry104
  的 030/047 scale = **0 键 keyless 轨, basis 与原版 104 常量逐字节相同**(basis=阶梯首键)
  — keyed+bufType1 被导出器压回 keyless 常量, 键全丢。**修复 = scale 六条 fcurve 重打
  LIMITS min_x=5**; 6_tag 配方已改 scale 行从原版 109 读 5(否则每次重跑又打回 1)。
  语义: 030.Y=2→左插座持有 / 047.Y=2→右插座持有(109 右手段两者同 2, 047 优先), 1=闲置;
  原版 104 本身就是常量左插座(对它自己的动作正确)。**✅ 2026-09-29 局内验证通过(用户
  确认)**: 重打 bufType=5 后刚1 游戏内左→右→左完全正常; 部署实测 030/047 = bufType 5
  成对键台阶(10/11 换右, 44/45 换回, 60/60 收尾)。另: MASTER 的 LMT::w03_00::104
  已不存在(疑似被孤儿清理), entry104 节点现直连 192_remap(tetherFrame 已绑, 导出
  root✓/cm✓); 重跑 MASTER 上的 7_merge 会因缺原生动作 FAIL(WORK 文件仍有)。

### 2026-10-01 用户手搓动作接入规则化: 10_apply_rules_divide.py + 无头接线验证工具链 (mhgu README 同步)

- 用户在 MASTER (MHW-Valor.blend) 直接新增自定义动作 (divide_long 63f / divide_short 57f /
  left_up·rigit_up·right_down_before_divide 各 6f; 拼写 rigit 是动作真名), 全部左手握持
  + 带前后左右根位移, ARP 原始烘培态 (871 fcurves, tether=None/frameCount=-1/starType=None,
  000 带恒等旋转缩放, 030/047 被 ARP 烘成逐帧)。`mhgu/10_apply_rules_divide.py` 一次套完全套
  已验证规则 (坑⑬ tether + 坑⑥ frameCount/starType + object 层整形 + 030/047=106 左手常量
  + 000 清理 + 6_tag v2 打标 + min-2-keys), 5 动作 871→314 fcurves, 全绿保存 (.bak 留档)。
  **复用: `RULE_ACTIONS="动作名1,动作名2" blender -b MHW-Valor.blend --python
  mhgu/10_apply_rules_divide.py`**, 后续新动作同流程。
- 导出器级验证: LMTActionParser (导出同代码路径) valid=True ×5, 82 变换轨满额, Root 双轨;
  警告签名与局内验证过的 151/192_remap 同族 (UNEXPORTABLE 7 = object 层设计性忽略,
  INVALID_ENCODING_NONKEY ≈43/44 = 常量轨自动降级, 两边一致 = 背景噪声)。
- **divide_short→entry103 无头接线+正式导出成功** (mhgu-export-repro.lmt, 82 轨, frameCount
  57, Root 双轨, 030/047 = keyless 常量左手 — 与原版 104 常量左插座同形; **纯常量左手动作的
  scale 就该是 keyless bufType1, bufType5 只对真有换手阶梯键的动作必需**)。
- **坑⑭ (README): 无头改完 blend, 开着的 GUI 会话必须 File>Revert 重载后再接线导出** —
  旧会话拿内存里的旧动作导出, 崩 `bounds() ValueError: Vector(): sequence size is 0`
  (精确条件 = Root 轨恰好 2 键/非 Root 恰 1 键且 buffered, 正常管线产物出不了该状态);
  用户实测重启即好。entry 的 LMT Animation 输入只能留一条连线, 旧连线不删会被
  `next(iter(actionNodes))` 取到旧动作。
- 无头工具链 (mhgu/): `_preflight_tree.py` 全树 154 动作预检 (problems 必须=0) →
  `_verify_exporter_divide.py` 单动作 parser 预检 (-- 传名) → `_repro_wiring.py`
  无头接线+正式导出 (ENTRY/ACTION/OUT, 不保存 blend) → `_check_repro_entry.py`
  产物解析 (-- 传 lmt+entryId)。
- 存盘 MASTER 的树: entry103←103_mhworig, 104←104_mhworig, 140←192_remap, 101/102←原生。

### 2026-10-01 (2) 勇气纳刀三招迁入 MASTER + MHGU 增量迁移配方固化 (mhgu README 同步)

- **`LMT::w07::195`(松开收刀)/`196`(保持按下构,无位移)/`095`(受击打断) 已全链迁入
  MASTER (MHW-Valor.blend)**, 源 = 用户给的 `E:\MHW-arts\mhgu.blend` (84 个 w07/w07_sa
  导入动作, 全部 0 引用 — **该文件永不可保存, 只许作 append 源**, 一存 0 引用动作全灭)。
- **增量链固化 (mhgu/README "增量迁移配方" 节)**: `_append_sources.py` (append+补
  mhgu_source+fake_user) → 2_arp_remap `MHGU_ONLY=` 每动作一次 → 3_fill/3b (幂等) →
  **`_4_object_root_motion_new.py` (外科版, 原版 ONE-SHOT 守卫会拒绝增量)** →
  3_rebake_ik (原脚本 verify 段 `frame_set(float)` 必崩, 已修 `int(min(f,maxk))`) →
  4_grip → 6_tag → 8_min2keys → `_transfer_to_master.py` (**append 不搬运
  freehk.tetherFrame 指针, 进 MASTER 后必须重绑**, 坑⑬的 append 变体) →
  `_verify_exporter_divide.py` + `_fidelity_check.py` (保真抽查, 3 招 max 1mm)。
- 落盘形态 (MASTER 实测): 81 变换轨/Root 双轨/tether 绑/frameCount=143/169/121/
  030=106 左手常量 (scaleY=2)/047 scaleY=1/000+247-253 全长/object 位移 (195 后撤
  -1.44m, 196 静止, 095 前移 +4.38m); 警告码 7/50/2/43 = 已验证同族背景噪声。
  parser 预检 valid=True ×3。**挂点态用户确认三招全左手 → 030/047 左手常量 = 终态,
  无需精调。**
- 待办: 三招收刀系槽位接线 (候选 5/6/9/10, 09-20 定案), `_repro_wiring.py` 可直接用。

### 2026-09-30 motion 选择机制破译: motion id = LMT entry | 0xC000 (动作层硬编码, FSM 无关; 填补 "LMT 选择逻辑在类工厂深处" 的开放项)

- **动作实例 +0x30 = motion id, 打包公式 `entry & 0x1FF | 0xC000`** (cJumpSlashLanding
  begin 反汇编级实锤; 0xFFFF = 不换 motion 哨兵, 见 FUN_141783710)。begin handler
  (实例 vtable+0x30) 经 **玩家 vtable+0x288(player, motionId, ...)** 启动 motion;
  部分变体走 +0x290/+0x298 (带混合参数)。之前 anim+0xE9C4 读到的 0xc06c/0xc132
  标记就是这个空间的 id (0xC132 = 特殊納刀 LMT306 ✓ 旁证)。
- **id→LMT entry 没有平表**: 每动作类的工厂 (类 vtable+8/+0x10, 构造 0x50 小实例或
  0xCD0/0xCD8 大实例) 在函数体里**硬编码立即数**写 motion id (小实例 +0x30 直写;
  大实例写 +0xCD0, begin 时 FUN_1417fdd30 式拷到 +0x30)。提取: 类表 0x142EF5C30
  (113 项, stride 0x20, **classPtr 在 +8**) → classPtr 首.qword = 类 vtable → vtable+8/
  +0x10 工厂 → 函数体搜 0xC000-0xC7FF u32。批量扫描 17 已知对 6/17 直命中
  (79→105/81→102/83→127/86→140(+170)/87→170/92→174); 共享类 (67/77 同 classPtr)
  的 motion 由 begin/运行时决定, 工厂扫描不适用。
- **运行时 motion 重映射行表**: cJumpSlashLanding begin 遍历实例 +0x48 行数组
  (4×i32: {前一动作set, 前一动作id, 新LMT entry, 触发帧}, 数量在 +0xCC8, 运行时
  由排队方填充, 类模板不含), 命中条件 = 前一动作匹配 **且 motion 时钟
  (anim+0x10C) == 行帧** → 播 `row[2]&0x1FF|0xC000`。0x141881xxx 有一个排队
  {3,11}/{3,12} 后逐个盖章 +0xCD0=0xC09C 的编排器实例 (某架势下 11/12 改播 156)。
- **实锤案例 = LMT 156 = ジャンプ気刃２連斬り（二段目）**: action **77** (用户局内
  3:77 见到; begin 0x1417FE600 = `mov [rcx+0x30],0xC09C; jmp 通用begin`) 和 **76**
  (共用类 cJumpSlashLanding) 开场直播 156; **73/74** begin (0x1417FE5A0) 播 155
  (一段目, 其 TIML ノードID f0=48→f25=60 在 f25 自切二段命中节点); 另有两个
  per-frame helper 监视 "当前 motion==0xC09B && vtable+0x2A8()" → 衔接 0xC09C。
  **76/77/73/74 全部无 FSM 节点** (同 98/99/100/103, 纯动作层 id, 靠请求/链数据
  进入)。col 交叉验证: TIML CollisionTimelineObject ノードID → wp03.col names 表
  node 列 = 招式日文名 (node 60 = ジャンプ気刃2連斬り二段目 ✓)。
- 陷阱记录: ① 跨武器撞号 — weapon type 8 (斩斧) id 114 的工厂也写 0xC09C, 那是
  斩斧自己 LMT 的 156, motion-id 立即数必须按武器区段归属; ② 类名探测要从 vtable
  末尾对齐, 共享类 vtable 会让"vtable+0x20 起即名字"猜偏 (67/77 同 vtable);
  ③ mhgu 迁移链的 "LMT entry 104 = MHGU 192" 用的正是这套实例 +0x30 直读, 所以
  改 LMT entry 内容即可换动作, 无需动代码。
- 事故快照 (2026-09-30): `nativePC/hm/wp/wp03/mot/w03_00/` 目录整体缺失 (含部署的
  MHGU 迁移版 LMT) — 用户局内跑的是包内原版数据; 是有意清理还是误删待确认,
  部署迁移 LMT 前先补目录。

### 2026-09-30 (2) 刚1 双弹反定案: force 链三缺陷 + tracker 失步 (已修复, 部署 md5 9e45890d)

- **症状**: 刚1弹反触发后短时间内按 Ctrl → 弹反再触发一次; 且刚1 (3:77) 期间体感有无敌帧残留
  (刚1 本体应无任何无敌帧)。用户先验: FSM 写的就是"刚1后不接弹反", 故怀疑 DLL — 正确。
- **日志铁证** (ValorGauge.log ~613867): 一次叮 → `COUNTER WINDOW OPEN (deflect flash)` →
  FORCE 3:77 → **~4 帧后 `COUNTER WINDOW OPEN (negation bit 250)` → 第二次 FORCE 3:77**,
  且第二次仍打 `interrupting 3:86`; FRAMETICK 帧数归零 = 刚1从头重播。三连
  `COUNTER INPUT` (614027/38/49) = 0.6s 窗内每次 Ctrl 边沿都再武装。
- **缺陷1 (主犯) force 绕过钩子 → tracker 永远停在 3:86**: TryQueuedForceWrite 直调
  g_originalDoAction (trampoline), DoActionHook 不跑, g_lastLongSwordAction 不更新 →
  `inEvasionNow`/`ActionIsForesightSlash()` (只认 3:86) 在刚1全程为 true, 三个 negation
  检测器 (flash/bit250/bit15) 整个刚1期间仍在"见切门内"站岗。
- **缺陷2 刚1播放中 bit250 再升起 → 重新开窗+重新武装**: 3:77 自己的动作 TIML 事件键
  (per-motion 事件同 id 不同义, 09-18 翻滚教训同款) 在刚1开始 2-4 帧内置位 bit250,
  被当成"见切弹反"→ 第二次 FORCE。另: flash 和 bit250 本就对同一次叮先后各触发一次,
  arm/consume 错开 → 单次叮天然双发。
- **缺陷3 反击窗 fire 后不清零 + Ctrl 边沿无动作门控**: OpenCounterWindow 只写
  g_counterWindowUntil; 0.6s 内任何 Ctrl 上升沿都再武装。
- **修复 (dllmain.cpp, 5 处)**: ① TryQueuedForceWrite 成功后同步 tracker = {3, 刚1id} +
  置 g_clangLatch + 打 `Long Sword action changed (forced)` 日志; ② fire 成功即消费窗口
  (g_counterWindowUntil 清零); ③ 三个触发点接线 `&& !g_clangLatch` (注释
  "one force per 见切 action" 写了从未生效 — 只有 VALOR_PROBE 的 SoundPostHook 读过;
  DoActionHook 仅在进入 86/101/102 时清锁存, 恰好是想要的每见切一次性); ④ CondEvalHook
  (631 判真无条件武装, 无动作门无 autoForce 门) 补 `g_counterAutoForce && !g_clangLatch`
  (本次日志无 COND631 行, 属休眠路径); ⑤ 窗口日志带 `nohit=<秒>` 便于局内核对注入状态。
- **残留无敌判定**: 叮触发链当前**无注入** (部署 force-action.txt = `3 77 0 0`, 第三参
  invulnSec=0 → OpenCounterWindow 的 pool+0xE4 写入被门掉; 默认值 0.5 只在缺参时生效)。
  ClangSoundVirtualHook (2188) 是死代码 (InstallClangSoundVirtualHook 无调用点)。
  故体感残留最可能是**见切自身 TIML b26 窗口 (f0-f12, 0.2s) 穿过 mid-action 强制切换漏进
  刚1** (游戏侧, force 绕 FSM 的副作用) — 修完双发后局内复测: 若仍残留, 下一个杠杆是把
  force 推迟到见切弹反窗 (f12 = 0.2s) 之后触发。
- **杂项**: `ValorGauge.counter-action.txt` (内容 "86") 无代码读取 = 死配置;
  `counter-repeat.txt` (Ctrl 脉冲) 功能已不在现源码中, 配置文件同为遗留。
  上一版部署 md5 92343850 → 本版 9e45890d32f6b863ada58f76ef258df4。

### Known Issues
- **Cracked/Repacked Versions:** FitGirl repacks etc. have altered chunk files incompatible with Nexus mods. Your install appears legitimate (Stracker + nativePC mods work).
- **Hi-Res Pack Confusion:** chunkG60.bin contains Hi-Res DLC, not base game data — ignore for weapon modding.
- **File Structure Complexity:** Damage/attack data location not yet mapped — requires opening files in MHW Editor.

---

### MHRS Animation Extraction (Verified 2026-08-30)
- Rise animation files use `MOTLIST v528`; the Long Sword file is `STM/player/mot/plw_longsword_100.motlist.528`.
- `yun-wulian/REasy` adds MHRS-specific `MOTFSM2 v43` reading/editing, but it is not the MOTLIST animation decoder.
- `PredatorCZ/RevilLib` contains `src/reng/motion_list_528.hpp` and registers `REMotlist528Asset`.
- The `nightly` RevilToolset release was published **2026-05-09 12:29:14 UTC** (20:29:14 China Standard Time).
- Downloaded tool: `tools/RevilToolset-nightly/` (`RevilToolset v2.10.201`, module `reasset_to_gltf`).
- Verified command:

```powershell
& '.\\tools\\RevilToolset-nightly\\revil_toolset.exe' reasset_to_gltf `
  (Resolve-Path '.\\MHRS\\LongSwordExtract\\plw_longsword_100.motlist.528').Path
```

- Output: `MHRS/LongSwordExtract/plw_longsword_100.motlist.glb`.
- Blender 4.3.2 imported the GLB successfully and produced 1 Armature, 3 Mesh objects, and 156 Actions. Animation tracks are present; the GLB has no character skin/mesh weighting, so the armature appears unskinned until a compatible Rise player mesh is imported and bound.
- Preferred workflow is `MOTLIST v528 -> RevilToolset GLB -> Blender`; a future Blender addon should wrap this converter rather than reimplement the binary parser.
- **Path encoding limitation:** RevilToolset `reasset_to_gltf` currently aborts (`SIGABRT`, code 22) when the input path contains non-ASCII characters (for example the Chinese `勇气太刀本体` folder). Stage the `.motlist.528` file under an ASCII-only path before conversion, then copy the generated `.glb` back to the desired location. Example:

```powershell
$stage = '.\\conversion_stage'
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item -LiteralPath '.\\valor-mhrs\\勇气太刀本体\\valor\\natives_backup\\STM\\player\\mot\\plw_LongSword_100.motlist.528' `
  -Destination (Join-Path $stage 'plw_longsword_100.motlist.528') -Force
& '.\\tools\\RevilToolset-nightly\\revil_toolset.exe' reasset_to_gltf `
  (Resolve-Path (Join-Path $stage 'plw_longsword_100.motlist.528')).Path
Copy-Item -LiteralPath (Join-Path $stage 'plw_longsword_100.motlist.glb') `
  -Destination '.\\valor-mhrs\\勇气太刀本体\\valor\\natives_backup\\STM\\player\\mot\\plw_LongSword_100.motlist.glb' -Force
```

- The staging directory and filename must contain only ASCII characters; the output can be copied to a Unicode-named destination afterward.

## Resources & References

### Modding Community
- **MHW Modding Wiki:** https://modding.wiki/en/monsterhunterworld
- **MHW Modding Discord:** Community support and latest tool updates
- **Ezekial711's FSM Editor:** For moveset/state machine editing
- **"MHWI Weapon Moveset Editing Guide" (YouTube):** FSM editing workflow

### Game Knowledge
- **MHXX Hunter Arts:** Original system with chargeable special moves per weapon + universal utility arts
- **MHW Weapon IDs:** See Ezekial711 wiki for definitive weapon type identifiers
- **MHW Action IDs:** See Ezekial711 wiki for action/move identifiers

---

## Quick Reference

### Common Paths
- **Game Root:** `E:\SteamLibrary\steamapps\common\Monster Hunter World\`
- **Chunk Archives:** `E:\SteamLibrary\steamapps\common\Monster Hunter World\chunk\`
- **Extraction Output:** `E:\MHW-arts\chunk-extracted\`
- **Mod Override:** `E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\`
- **Long Sword Data:** `E:\MHW-arts\chunk-extracted\G0\chunkG0\wp\mus\`

### Weapon ID Mappings
- `wg10` = Long Sword
- `wg11` = Great Sword (two-handed)
- `wg01` = Sword & Shield
- `wg02` = Dual Blades
- (Full list in Ezekial711 wiki)

### Tool lists

MHWNoChunk and MHWEditor

### 2026-10-01 特殊纳刀系(98/306 + 100/312)无敌帧 + 受击切换触发链(局内验证待做)

- **TIML 已改**(游戏目录 w03_00.lmt,`.pre-iframes.bak` 快照;幂等脚本
  `apply_sheathe_iframes.py`):entry 306(动作 98 特殊纳刀)mFlag f0/f10/f15
  OR b26 全程窗口 + mTrigger f0=2048;entry 312(动作 100)mFlag f0=0x05000000
  (b26+b24 全程)+ **结构化插入** PlMotionCommon mTrigger 轨 {f0=2048}
  (文件尾 16 对齐追加 keys/transforms 数组 + 类型槽 {toff,tcount} 重指针;
  types/transforms/keys 数组先验证 entry 独占才动手)。
  **坑⑮:blend 里 306/312 的 entry 节点接了 TIMLDataNode(TIML Data 输入),
  exportNodeToFile inject = 整个 entry 替换 → 导出会把 TIML 打回原版;
  凡导出动过这两个 entry,重跑 apply_sheathe_iframes.py**。blend 侧根治
  (TIML 在 blend 是 action fcurve,u32 拆高低 16 位,见 timl_importer.py
  TIMLFlagImport)= 待办。工具坑:patch_timl_defense.py 只能 dump/move/setval,
  插键/建轨要结构化脚本;0x06400000 的十进制是 104857600(手算十进制必错,
  用 python 算)。动作链按用户定案:98(特殊纳刀)→99(入鞘状态,无独立 LMT
  映射)→100(LMT312);action-list.md 里"99→306 特殊纳刀"是旧行,以 98 为准。
- **受击切换 = sheath-counter 系统**(mod build,部署 md5 48d4f78705e1e7d0623b866c794bdc8a):
  见切/居合的 bit15/bit250 处理器按动作 id 门控(86/101/102),98/99/100 没有原生
  处理器 → "无敌帧中被打到"的信号未知(可能像翻滚一样完全静默,见 09-18)。
  `ValorGauge.sheath-counter.txt`:`mine <ids>` = 期间挖 event-bit 数组
  (player+0x8938..8 dwords)边沿,日志 `IFRMINE evN 0x.. -> 0x.. rose=[bit..] act=3:x`;
  **F5 = "被打到了"手动标记**(与 gauge-lock 共用 F5 边沿,lock 当前关不冲突);
  `rule <ids> <eventBit> <toSet> <toId>` = bit 上升沿 → counter 窗口 +
  no-hit(g_forceInvulnSec)+ force DoAction(先写 g_forceSet/g_forceId 再 arm,
  见切同款"先叮后注入"顺序)。默认文件只开 mine,rule 注释待挖矿结果。
  若挖矿全静默(翻滚同款)→ 下一步钩 TIML no-hit 碰撞拒绝点(FUN_141f66dd0 上游)。
- **(2026-10-01 下午)首局挖矿 + 分层定案**:TIML b26 = 查询层排除(与翻滚同类,
  零事件零表现,用户局内确认无敌但不叮);见切=仲裁层弹反(b22 窗口+id 门控 handler)。
  bit250 置位点全在伤害结算函数内(FUN_141217e50/14121ea10),查询层排除到不了;
  pool 查询 FUN_140aee690 type8 = no-hit 检查但调用方全是 FSM 属性 getter。
  **mod 版新装 HitDispatch 钩子(SHEATHDISPATCH,mine ids+86 对照)**:98/100 无敌帧
  中被命中若日志出现 type-6 non-0xD 无伤害 → 启用配置 `drule`(已带注释行)=
  纯无敌帧受击切换;若空 → 走超甲窗路线(b24+DmgCalcHook,代价挨伤害)。
  新部署 md5 待游戏关闭后覆盖(构建产物就绪,旧版 48d4f787 被游戏锁定)。
- **(2026-10-01 晚)SHEATHDISPATCH 零输出 = 钩子 bug 修正**:当前源码的分发器
  日志格式从未产出过(vtable+8 类型镜像永远≠6 → 入口全提前返回),旧的 133 条
  HITDISPATCH(code=0x6B)是类型镜像加入前的"[log only]"老代码产物;"clang =
  non-0xD + 无伤害"语义仍有效。修复(部署 md5 ab2d10039ba8a5344a41ed211e30ecbd):
  去掉类型门控,0xD 洪泛排除,non-0xD 记 SHEATHDISPATCH,drule 触发同步。
  判读要点:老笔记记录 non-0xD 在窗口开着时连空挥都出现 → 下局必须带空挥对照组,
  hit/whiff 的 code 值无差别则转超甲窗方案。用户 MARK 落点 3:99/3:0(链后段),
  下次尽量在 98/100 本体动作里标。
- **(2026-10-01 深夜)分发器判读完毕 + 方案定案**:修好的钩子实测——98 的分发器
  爆发确定性(每次恰好 33 行,空挥一样,负 float 计时器漂移),三次见切弹反零输出
  (老"clang=non-0xD"证据不复现)→ 分发器不可判别,路线关闭。用户定案选
  **超甲尾窗方案**(否决怪物出招触发/手动触发;另提案"见切 FSM 播不同 LMT"留后):
  306 f15 回退原值 = 无敌 f0-f14 + 霸体 f15+(apply_sheathe_iframes.py 已改幂等,
  修了 set_trigger_f0 返回值双义 bug);mod 加 `arule <ids> <toSet> <toId>`
  (DmgCalcHook: 霸体段命中→HP 钳制保底 1 + counter 窗口 + force 切换,配置已启用
  目标 3:77)。HP 链钉死:***(float*)(*(player+0x7630)+0x64)***(+0x60 疑似 MaxHP);
  DmgCalc 出参是分类 out 不可钳伤害。部署 md5 84ef445a7a1b5b2321fb8c7200a7a9e7。
  待验:钳制vs死亡判定时序、a2 防御方钉定(日志前 8 条 SHEADRULE hit)、多人误触。
- **(2026-10-01 深夜2)b31 = 防御/弹开窗位(重大)**:用户回忆突刺(105)当年加过
  防御帧有音效;仓库根 105.txt = 当年配方 dump:f0=0x87400006(b31+b26+b25+b22),
  原版突刺无 b31/b26 → **b31(0x80000000)= 防御窗,叮类机制 TIML 可写**(见切叮窗
  = b31+b22;307/308 无 b31 故叮靠 handler)。当前 105 已被导出冲回原版(配方存
  105.txt)。已应用:306 f15=0x87400006(照抄配方)= 无敌头 f0-f14 + 防御尾到末,
  arule 保留兜底(防御被拒→命中照连→arule 触发)。待局内验证:纳刀家系是否允许
  防御帧、防御音效、以及防御交互在 mod 侧的可测信号(FXNOTIFY/SHEATHDISPATCH/bit15)。
- **(2026-10-01 深夜3)用户规则: b26 罩死 b31**——无敌帧与防御帧同窗时只有无敌
  生效(b26 查询层先吃掉命中,防御交互不发生)。第一轮 105/306 都用 b31+b26 同窗
  (0x87400006)故无声。已改:105 f0 与 306 f15 → **0x82400006(b31+b25+b22,无
  b26)**,306 无敌头 f0-f14 不动与防御尾无重叠;脚本已同步。105.txt 是含 b26 的
  后-state,非当初工作态。判读:105 有音=配方成立;306 有音=纳刀允许防御(再看
  可测信号);306 无音=家系封死 → 见切 FSM 播不同 LMT 上位。
- **(2026-10-01 深夜4)★bit28 = 格挡位(定案,修正 b31 误判)★**:用户提供历史
  会话——全武器 LMT 扫描:**mFlag bit28(0x10000000)= 格挡有效**(CB/SnS/GL/GS
  有,太刀家系无;守备架势 = 单键 f0 整段 0x10000000 或 |bit31)。当年实验:突刺
  105 f0=0x12400006(bit28+b25+b22,无无敌)→ 正面格挡成立(音效/削伤/掉耐力/
  不硬直),背对照常掉血。**b31 非防御位**(深夜2/3 的推断作废,守备架势里只是
  伴随位)。引擎细节:方向=朝向防御锥;后果链=格挡成功分支统一;伤害分类写反应
  管理器 +0x769=0x100/0x101,仲裁 +0x768/769/76A → FUN_1411cac40(格挡反应装填);
  格挡参数 weapon+0x8A0/0x8A4(守备时长)+0x8A8 三组等级表(太刀缺块则 DLL 补)。
  叮+蓝光候选评估器表通用(复用居合槽位或 DLL 置属性 0x35 族即白得演出)。
  已应用:105 f0 与 **306 f15 → 0x12400006**(306=无敌头 f0-f14+格挡尾,无重叠,
  脚本同步幂等验证)。判读:105 正面格挡=配方成立;306 格挡=家系允许→钉切换信号;
  306 无 105 有=家系差异;都无=LS 缺 +0x8A0 块→DLL 补参数。
- **(2026-10-01 深夜5)★闭环: bit28 整段格挡成立 + bit250 受击信号 + 3:37 揭底★**:
  306 三键全 0x12400006(整段单值格挡)→ 格挡音效成立!此前"全程无敌"= f0/f10 的
  b26 键毒化整段(按动作生效非按键窗;b26 多键布局在 306 不可用,防御用单值
  bit28)。**3:37 = LS 格挡反应动作**(格挡成功→FUN_1411cac40 装填→动作换成 37;
  太刀原生不格挡故从未见过;"预知被打马上换动作"之谜 = 同一现象,非 DLL bug)。
  **bit250 在 3:98 上升(IFRMINE 实录)= 受击检测信号到手**。已接线:
  `rule 98,99,100 250 3 77`(sheath-counter.txt,Shift+F6 生效)——格挡被击 →
  counter 窗口 + force 77;格挡链不走 DmgCalc(arule 静默 ✓)。竞态:引擎先装
  37,rule 延迟 1-2 帧切走(37 闪 1-2 帧;介意→钩 FUN_1411cac40 空转抑制)。
  306 终态:三键 0x12400006 + mTrigger 2048@f0;无敌头+格挡尾的多键布局放弃
  (b26 毒化整段,锁存机制未挖);apply_sheathe_iframes.py 目标值待定案同步
  (重跑会洗回头尾布局,实验期勿跑)。
- **(2026-10-02) 强化射击(特殊射击) TIML: 纯表现层定案 (user-verified, TIML 路线死)**:
  签名 (vanilla) = `PlMotionCommon.mTrigger` **`f34=1024(bit10)+f36=1(bit0)` 成对点火
  通知** (332/338/341/344/347 加 f0=2048 无敌门变体); `mSlingerShellAttach 3→0@f36→3`
  弹壳分离同帧; 家族 = LMT 332~349 共 15 entry 全 134f 键位一致 (296/297 = @f9/12
  101f 变体); 306~311 的孤立 1024 语义不同勿混。类表: **id 43/44/45=cKijin1ComboShot
  Base/F/R, 46/47/48/51=Kijin2 Base/F/L/R, 49/50/98=Kijin3 Base/F/L,
  81=cActWpOnSlingerShot**。104 能接射击却无任何射击专属键 = 接受门不在 TIML。
  实验: 105 突刺塞触发对 (sharekeys.py 重指向 338) → 只有画面震动; 341 全套 TIML
  复制到 105 → 投石音效+震动, **无伤害无投射物**。定案: TIML=纯表现 (音/震/弹壳
  动画), 点火/伤害/输入接受全在 cKijinNComboShot 动作类代码侧。改造路线归一:
  ① motion-swap 托管 43~51/98 槽; ② DLL combo-force (duringIds+VK→force {3,43}族)。
  工具 `research/mhw/shot_weave/`: analyze.py (scan/diff/windows) + sharekeys.py
  (transform 头重指向他 entry 键数组, 零字节增长; 正式方案应复制数组)。
  **附带: 部署版 341 丢整个 PlMotionCommon (7轨) = blend 导出管线写残** (全表审计
  仅此一处, 141/142/143 为管线增益; 修复需 blend 侧; 导出后跑 analyze.py diff 审计;
  手改部署 LMT 会被下次导出冲掉)。

---

*Last Updated: 2026-10-01*
*Current Milestone: 2026-09-26: 动作帧控制定案 — 动作实例 player 控制器(player+0x61C8) instArray+id*8 的 +8 float 就是动作帧 (begin 清零, 每帧+=dt), 无原生"起始帧"API; FRAMESEEK 已实装 (force-action.txt 第4参 startFrame 单位=秒[30Hz], Shift+F6 热重载, phase==2 时一次性写帧, FRAMETICK 诊断随每次 force 记录[值变化才记], 部署 md5 92343850; motion 时钟 = *(player+0x468)+0x10C (TIML帧制) 已破译, v3 双写 USER-VERIFIED: 从任意帧起手 ✓). 2026-09-25: 收刀拔刀刚1 已实装 (sheath-rewrite: 收刀态{3,11}→{3,86}, 门控=红刃0x2370>=3 且 气刃量0x2368>0.15, 配置 ValorGauge.sheath-rewrite.txt, 第一版因 weapon+0xC71 收刀态读0而失效(该字段语义待钉,已弃用), 收刀判定改用 tracker set-1 前趋动作; 现版 md5 7fe2c220, 待局内验证). 收刀态输入路由定案 — 收刀按R2产生的{3,11}是代码+静态表路由, 武器FSM完全不参与 (待機1000零链路; 重映射器对id11透传; 每武器set-3请求表 0x143FD5590+type*0x130, LS 113条@0x143F4D6A0, 消费链 b670→b830→b590→DoAction; 想纯数据改收刀R2从这张表入手; DLL改写路线 = DIRREWRITE加 收刀态{3,11}→{3,86}, 门控weapon+0xC71). 修正: remapper反编译case N=武器类型N+1 (dispatch有dec eax), case 2就是LS.  气刃量定案 — weapon+0x2374 float 0..1 (HUD getter FUN_140616d60 ×100; DropLevel 从 player+0x7810 +0x9AC/+0x9B4 回填; 直接写此 float 即可配置, 锁满需每帧写). weapon+0x2368 双重语义未决 (居合架势 int 0/1/2 vs 0..1 float 累加器, FUN_140b980c0 用它+0x236C 做动作重定向) — 未经 A/B 不要当气刃量覆盖. 2026-09-20: 纳刀合并机制实锤 — 动作id驱动挂点表切换状态机 (收刀系槽位 5/6/9/10 复用定案; 详见 Sheath_Research 文末). 刚1 counter-parry COMPLETE + mod/probe SPLIT VERIFIED. 2026-09-19: FSM drive chain mapped — zero-inbound nodes (居合 101/102, rolls) are entered by request-ActionNo match; iai chain = ほぼ全攻撃(1013)→居合斬り開始(-10 marker)→特殊纳刀架势 weapon+0x2368 (FUN_14211cc60)→remapper FUN_14118c320 request-id remap (0→100,1→101,2→102...)→DoAction 0x140269CE0→node. Corrections: 0x2368 = stance flag not selector; 763/764 remaps are set-1. FSM map tool: dump_fsm_xml.py → wp03_fsm_map.md. ValorGauge.dll = LS-only mod, triggers: 见切 bit15 / 居合 bit250 / deflect flash / condEval-631 → no-hit + auto-force; BGM bug RESOLVED by the split (user-verified). ValorProbe.dll = passive research build, never auto-installed. Backlog: kenki-gate via 練気ゲージ判定 (0x274/0x275), gauge UI polish, 大居合(102) request-id-2 upstream trigger, +0x2368 float/int 交错实测, 命中充电字段钉死*
- **(2026-10-01 深夜6)★方案 B 落地: motion-swap 工厂补丁(用户验证)★**:mod 初始化
  扫描 LS set-3 类表(0x2EF5C30)对每类工厂扫 motion id 立即数并改写——配置
  `ValorGauge.motion-swap.txt`="140 141" → 见切(86)播放 entry 141 的动作,原生
  弹反/arm/FSM 全保留(用户局内确认)。bluesim 阴性定案(+0x2399/0x23A0 族全是
  aftermath);池计时器假设证伪(POOLDIFF 13×86 零痕迹);+0x768=居合开场旗。
  141(用户克隆的见切基底)TIML 已修 b31 窗并入脚本;**脚本工作流实战验证**:
  用户导出冲掉全部 blend 侧 entry → 一键重打 ✓。当前布局:306/312=见切形窗口
  (实验态)、105=bit28 格挡、141=b31 窗。设计待定:见切决策点(受击→反击/超时→
  收刀)的 decide 规则、98 终态防御、306/312 终态。部署 md5 0c4782455fede412696fbdad8d2493f3。

### 2026-10-02/03 勇气太刀攻击窗/输入窗 + 强化射击摘除定案(CTO=伤害窗实锤)

- **伤害窗 = `CollisionTimelineObject`(CTO) mFlag,终审定案**:参考包
  `nativePC -xfsm+rs`(SR 集成版,注意其 `w03_00.lmt` 即工作参考)entry 339 =
  能打的一文字,CTO 窗 [23,34] == 用户肉眼观察伤害帧 [24,34] 一帧不差;其 EC 窗
  [30,36) 根本不覆盖 [24,29] → **EC(`nWwiseTimeline::EventCollision00`)与伤害时机无关**
  (请求/音效状态机;早前"EC=伤害窗"的旧笔记作废)。`mTrigger` 1024+1 点火对 =
  投石器弹本体。332–349 强化射击族原版**没有 CTO** → 刀刃部分从无数据侧伤害窗,
  伤害全靠投射物——"换 TIML 射击消失同时伤害消失"的完整解释。
- **强化射击摘除配方**(339/340/343 实测:能打、无射击):保留家族骨架(mTrigger 轨、
  PlMotionInput、EventGroup01/02/03/06),清零:点火对(f34=1024+f36=1)、
  mSlingerShellAttach(dt=0!)、mClawGimmickState、mCameraShakeIndex、EventGroup09。
  SR 的 rs338/339/340 同配方(它保留点火对因为要留射击)。
- **CTO 移植** = 字节级克隆参考 339 的 CTO:双配置 ノードID[0]=59(気刃3二段/bank0)+
  ノードID[1]=72(仲間当たり),mFlag 窗口帧改用户值。node 63(見切り攻撃/bank8)
  疑似带反击态门控(中段空洞来源),弃用。**每个 target 的 CTO 是独立追加副本**
  (types 数组复制到 EOF + typeCount+1 + 指针重排)。
- **动作↔entry 路由定案**:动作 103–106 工厂三连 motion {338,340,339},107–109 →
  {341,343,342};类=通用 begin(三连按 player+0x82C8 三选一+行表),伤害不经类代码。
  FSM 节点 66–75 = 気刃スリ(強化射撃)状态(动作 104–112);用户 FSM 的 69/70/71
  已更名 一文字短/长/无双斩(动作 106/105/108)——与参考作者同款占位思路。
- **窗口分配(用户更正:曾弄反)**:动作 105 = entry 340(126f)= 一文字**长** [29,34];
  动作 106 = entry 339(114f)= 一文字**短** [17,22];343 无双斩 [31,39]+[58,70];
  EC 组别一律 v2(原版带伤害的 EC 窗全是 v2,v1=音效槽);140 刚1 EC[45,55]/CTO[43,58]。
- **崩游戏三连(全修,知识库级教训)**:
  1. col 被插入 3 字节 → ATK 全表错位 → 开游戏崩。鉴别:禁用 LMT 能进 = LMT 无罪。
     col 每次改完必须做长度+结构校验(MHW Editor 保存/装包拷贝都会动它)。
  2. TIML 键帧数组乱序 → 进存档崩。leftover 键清零后必须重铺帧保持严格递增。
  3. CTO 追加**必须先把 types 数组复制到 EOF 再 typeCount+1**——否则第 9 个类型项
     覆盖相邻 transform 数组(add_cto.py 有这步;srcto2 活是因为原地改已有 CTO;
     apply_valor_attack_config 首版踩坑 = 进存档崩)。
  4. 小数键帧(24.4/25.9)疑似崩溃源 → 一律整数帧(原版全整数)。
- **`apply_valor_attack_config.py` = 导出后一键配置,幂等**(断言:键帧排序/整数帧/
  捐赠窗数/CTO 存在性,失败拒绝写盘):射击摘除 + EC v2 窗 + SR 双配置 CTO +
  140 刚1 + INPUT_PLAN 输入窗。**每次 blend 导出后跑一次**;改帧改 PLAN/INPUT_PLAN 表。
- **`PlMotionInput` = 输入接受窗**(用户删 TIML 后中断全灭的机制实锤)。位域解码:
  b8=翻滚接受、b31+b17=特殊派生窗(见切→必殺攻撃)、b14/b16/b30=连段预输入、
  清零键=显式关门。见切 140 原版=限制型(f47 关门、b8 拖到 f110),气刃斩1 104=
  开放型(f46 b8、f56 全开不关)。
- **INPUT_PLAN**(140 刚1 = 104 骨架,翻滚/接招并至 f56,110/121 收势;340 一文字长
  全开 [38,...);339 一文字短 [26,...);343 无双斩 [92,...);**open-ended 模板的
  leftover 键继承末档值**,窗口延续到动作尾)。实测刚1 手感 ✓。
- **刚1 无敌帧**:TIML b26 清零无效(09-18 结论复验,FSM/动作层授予),数据侧关不掉。
- **DLL**(md5 058bc40d):HITFRAME(每次命中带 motion entry+mot 时钟)+ MOTIONTRACE
  (motion 切换追踪)诊断常开(80/150 条上限)。**F6 连发会打断动作+FRAMESEEK 归零,
  污染测试数据——测试期禁按 F6**。
- col 侧:bank8(見切り攻撃)rng 提至 100/124(居合档);move25 HitLag 0.1→0.05;
  Lingering=1 实验撤回(疑似 boot 崩且非洞因)。node 63→59 后 bank0(50/62)偏短,
  需要距离时把 bank0 抬到 100/124 或换 node 84(bank9)。

### 2026-10-03 新工程文件(MHW-Valor-new.blend)重建 + LMT 合并工具战报

- **新工程工作流(已验证可落地的形态)**:GUI 里 FreeKinetics 导入**原版**
  w03_00-origin.lmt(百试百灵)→ `overlay_converted_actions.py` 把
  MHW-Valor.blend 里的转换动作曲线拷到同名 action(103/140/141-143/306/312/
  338/341/339/340/343/357-367)→ `restore_vanilla_entries.py`(GUI 内跑)
  导入原版 + 给所有**非转换** entry 恢复原版曲线 + 全部动作 Fake user + 保存。
  转换 entry 的 TIML(攻击窗/输入窗/射击摘除)不靠 blend——导出后由
  `apply_valor_attack_config.py` 重打,blend 侧保持原版即可。
- **二进制 LMT 合并(build_clean_base_lmt.py)= 搁置**:结构上已通(区间移植、
  规范 TIML 重建、全指针审计 0 违规),但 FreeKinetics 的解析器还咬人:
  (1) types 数组**顺序读**且读前 `align16(timlOffset+40)` 跳齐——TIML 必须
  单区域连续且 types 紧跟头部(CTO 追加造成的两段式 TIML = 原始导入崩溃根因);
  (2) 骨骼关键帧读取存在**越界过量读**(12B/键 的 stride 走到文件尾)——原版文件
  尾部永远有后续 entry 兜底所以从不暴露,合并文件尾=最后一个 entry 就炸;
  (3) 64KB 零填充不够兜。结论:导入走 Blender 原生流程,二进制合并只在
  游戏部署链(指针驱动)有需求时再回来修。
- **字段偏移备忘**(fcurve 骨骼头 48B):bufferSize=i32@+12、bufferOffset=i64@+16、
  lerpOffset=i64@+40——读反了会把文件头当缓冲起点拉出几十 MB 的"跨度"。
- **trace 工具**:tools/parse_test.py(FreeKinetics 解析自测)、
  tools/trace_timl.py(TIML 逐结构追踪)、tools/find_bad_overlay.py(逐 entry
  二分)。注意 blender headless 会话的插件加载顺序有随机性,解析测试结果
  偶发不稳定,以两次一致为准。

### 2026-10-03 (2) MHW-Valor.blend 原版动作重置 + TIML 回灌(_vanilla_reset.py, 全部验收通过)

- **背景**:MASTER(MHW-Valor.blend) 里原版槽位被 MHRS/MHGU/自制动作占用
  (140=192_remap 刚1、312=095_remap、307/343=MHRS plw remap、339/340/358-363=divide 系,
  其余 152 槽动作正常)。用户指令:动作全部换原版(按 entry id 匹配),TIML 全部换成
  部署文件 #1 的(nativePC w03_00.lmt, 其 TIML 均正确)。
- **做法**(mhgu/_vanilla_reset.py, README 有完整章节):按 #2 原版 LMT 的
  fcurveOffset 分组 KBRAction 重导入(154 组,共享组同构)+ entry 重接;
  按 #1 的 timlOffset 分组 processTIMLData+TIMLStructure 重建 164 组 TIML 节点;
  旧动作全部 fake_user 保命、旧同名动作 .pre_reset 后缀、失连节点删除(1716 个);
  #2 不存在的 141/142/143(divide 系)动画保留。备份
  MHW-Valor.blend.bak-20261003-124129。
- **验收**(=_vanilla_reset_verify_final.py + _vanilla_reset_export.lmt):
  接线 164/164、自定义 3 槽保留、entry 头部与原版零差异;TIML 掩块首 u32 后 md5
  与 #1 逐 entry 一致 164/164;动作结构逐骨全等(键时间零漂移),数值残差 = 导出器
  再量化(max 0.125)+ loc basis w 1.0→0.0,均为工具链既有签名(#1 同样携带,游戏实测无影响)。
- **坑⑯**:TIML 块序列化首 u32 = 块在文件内的偏移,两个文件的 timl 直接 md5 永远
  不等,先掩前 4 字节再比。
- **副产品:坑⑮根治**——306/312 无敌帧、140 见切窗口等**文件级 TIML 编辑随 #1 回灌
  进了 blend 树**(164/164 逐字节一致),以后从 MASTER 导出不再需要
  apply_sheathe_iframes.py 重打。
- 部署文件 #1 未动。把 `_vanilla_reset_export.lmt` 部署进 nativePC = 整套退回
  "原版动作 + 现有 TIML 编辑"(刚1/樱花/divide 在游戏里会消失,动作仍在 blend 里
  可随时重接);是否部署由用户决定。

- **(续)用户澄清后取证**:刻意替换(remap/divide)不是问题源。对重置前备份整体导出
  比对:**备份动作与部署 #1 164/164 完全一致**,与原版 #2 的 151 槽差异全部是管线
  重编码签名(frameCount 全等/81-82 轨/时间网格零差/值差≤0.01),无 MHRS/MHGU 渗入;
  10 处"接别的 id 的 action"= 镜像招共享 fcurveOffset 的正常命名(以组内最小 id 命名)。
  布局:FreeKinetics 只有按列堆叠一种默认排布;`_relayout_tree.py` 恢复
  "action 左 / entry 右 / TIML 正下方"配对布局(167 行,按 entryNum 排序,全部展开),
  重排后接线复验 164/164 全绿。

### 2026-10-03 (3) 全树导出卡死存档 + game-safe 构建法(待局内验证)

- vanilla-reset 全树导出(_vanilla_reset_export.lmt)部署后进存档卡死;与最后已知可用
  bak-010916 全量对比:结构/TIML/自定义槽全一致,唯一全局差异 = 动作编码层
  (E 复用原版 bufferType 1/2/7 再编码,R 是管线重算编码 12/13)。4-bit 溢出已排除。
  **教训:FreeKinetics 全树 standalone 导出从未整文件进过游戏,必须先 A/B。**
- **game-safe 构建 = _build_firstaid_lmt.py**:V 原版动作本体字节(round-trip 字节稳定
  已证明)+ R 的 TIML + R 的 141-143 完整 ActionHeader → _vanilla_firstaid.lmt
  (md5 0d673ddd)。自校验全绿;与 R 差异仅"原版槽真原版字节 + 10 刻意槽回原版"。
  若 F 仍卡 → bak-010916 顶回 w03_00.lmt(旧全套,必能进)。
- 部署链第二次出现"扩展名前多空格"文件名损坏(`.l mt`),复制后必须核对文件名。

- **(续 2)F 也卡 + 定罪**:用户实测原版 override 能开、E/F 都卡 → 毒在 R 的 TIML。
  结构 diff:R 的 TIML 161/164 与原版同构,唯一新增 = addcto 链在 339/340/343 共享块的
  占位类型(param=0x4BD36121,transform hash=0x1,0 keyframe);其余 5174 个 transform
  关键帧全部合法(昨晚编辑干净)。**F2(=F 剥占位类型,须同步 TIML_Data.count——
  不同步会错位多读一个类型,假象是"剥离没生效") = _vanilla_firstaid_f2.lmt
  (md5 ee6e195c)**,保留全部关键帧编辑+141-143。**坑⑰:改 PyCStruct 子列表
  (timl.data[i].types)后必须同步父结构的 count 字段,否则序列化字节少了一截、
  count 还是旧值,重解析会"看起来没生效"。** **✅ F2 局内验证通过(2026-10-03):进游戏,占位结构定罪,关键帧编辑全无罪。**
  F2 非 Blender 导出 = struct 级拼装(原版动作字节+R 的 TIML+R 的 141-143),
  Blender 仅当 Python 运行时。E 路径卡死同样是占位结构所致——导出路径无罪。树侧删占位后正式导出 = F3
  (_blend_export_f3.lmt, md5 8a5e3541, 验证全绿),工程已保存,以后导出天然干净。
  **✅ F3 局内验证通过(2026-10-03 深夜):Blender 导入→改→导出→部署链路完全恢复。**
  当前定态:部署 = F3(md5 8a5e3541);blend = 干净基座(原版动作+全套 TIML 编辑+141-143);
  备份: bak-20261003-124129(reset 前)/ bak-20261003-154331(摘毒前);
  拼装脚本留作应急。教训汇总:①全树导出物部署前必须 A/B;②文件名空格损坏已两次;
  ③改 PyCStruct 子列表必须同步父 count。

- **(续 3)重导入动作补绑 Tether(用户指出漏了导入选项 "Tether to Skeleton")**:
  vanilla-reset 的 154 个新动作原本是合成路径(BoneFunction.xxx/Root/reference),
  编辑器无法预览编辑。`_tether_vanilla_imports.py` 补跑 transferTether(路径重命名
  MhBone_XXX、Root 转物体级、记 tetherFrame)→ F4 `_blend_export_f4.lmt`
  (md5 54eda879):Root 烘焙逐帧键(值恒定),与旧管线导出 BK 的 -1 形态完全同构,
  轨道结构=原版 82 轨,TIML/自定义全绿。blend 已保存(bak-20261003-160823=绑前)。
  F4 待局内终验;E/F3(未绑定导出)与 F4(绑定导出)均为合法形态,F4 是项目原生约定。

- **(收尾)新动作骨骼归组**:9_group_channels.py 对当前 blend 跑完(59057 条重归组),
  归组后导出 md5 == F4 逐字节一致 = 纯外观。vanilla-reset 全链收尾:原版动作
  (tether 绑定 + 归组 + 编辑器可预览)+ 全套 TIML 编辑 + 141-143 自定义,
  Blender 导出可部署(F4 已局内验证... 注: F4 同内容 F3 已验证, F4 待用户部署确认)。

### 2026-10-03 (4) 一文字/无双斩无伤害定案+修复: 树侧 CTO 补齐 (user-verified 有伤害)

- **症状**: 339 一文字短/340 一文字长/343 无双斩 播动作但零伤害; 同文件里 刚1(140) 正常。
- **根因**: 这三个槽位源自原版**强化射击家族(332-349)**, 原版刀身部分从没有数据伤害窗
  (伤害走弹体) → 家族 TIML 只有 EC 状态机**没有 CTO**。MHW 出伤害 = EC 窗(请求层,
  只开门)+**CTO(路由+伤害窗: ノードID→col 名字表 unk4→(bank,ATK行), mFlag=伤害窗)**
  两者齐备; 缺 CTO = 请求无处落地。刚1 有 CTO(見切 TIML 自带)所以一直正常。
  "昨天不用 col 也有伤害"与此不矛盾: col 目录坏名只是让 col **编辑**不生效,
  CTO 路由到的是原版 col 行(63/59/72 原版都在)。
- **修复(树侧, 随导出自带)**: 镜像 140 的树侧 CTO 写法给三条 entry 各加一个
  `TIML::nTimelineParam::CollisionTimelineObject` action
  (starType=TIML_Action, timelineParamRaw=`m38FF82EA`, unkn0=0) + 两条 transform:
  `FreeHKTiml.mB296CA66`[0]=**ノードID[0]=63**(原版見切り攻撃 ATK 行, 刚1 同款);
  `FreeHKTiml.m08FD20A6`(mFlag 按 16/16 拆两条) on=a/off=b+1, CONSTANT 键。
  脚本 `mhgu 根目录 _add_tree_cto_1003.py`(PLAN 在文件头, 新招复用改 PLAN 即可;
  GUI 路线=复制现有 CTO 三件套节点再改帧)。blend 备份 bak-20261003-190535-pre-tree-cto。
- **✅ user-verified 2026-10-03: 三招全部出伤害。** 部署=树导出
  `_blend_export_cto_1003.lmt`(md5 f136fb94), 备份 bak-20261003-1922-precto。
  审计 464 entry 0 乱序 0 小数帧。**F2 时代卡死的是 addcto 空壳占位
  (param 0x4BD36121/hash 0x1), 真 CTO 结构树侧无风险** —— 树侧 CTO 正名。
- **EC 静态轨道名破译**(此前裸哈希): m6C333EBF=ReqNo(On) A(=1), mF53A6F05=
  ReqNo(On) B(=2), m823D5F93=**ReqNo(On) C(=10, 目前 140 独有)**,
  mA39EF26C/m4D909340=SkipTime A/C。339/340/343 与 140 现仅差 ReqNo C 一组
  ——**若未来某招补 CTO 后仍无伤害, 下一杠杆=补 ReqNo(On)C=10+SkipTime C**;
  再往后= DLL MOTIONTRACE(实播 entry|0xC000)+HITFRAME(命中 entry+帧) 检测。
- 伤害数值现借見切り攻撃行(与刚1同源); 独立数值待 col 侧加专属行(节点id改指即可)。

### 2026-10-03 (5) entry 312 ← LMT::w07::095_remap 树侧接线 + 60帧无敌 + f100接招窗

- **接线真相**: blend 里 entry 312 一直挂的是旧 `LMT::w03_00::312`(纳刀100时代,90f);
  `LMT::w07::095_remap`(121f,tether已绑) 和整套 **`.095` 后缀 TIML actions**(10个:
  PMC/PIN/PV/EC/EG01/02/03/06/GP/EP, 095 自带数据) 都在文件里但**零接线**。
  本次: 建 LMTActionNode 接 095_remap; 旧 TIML 链接全清, `.095` TIML family
  经新建 TIMLActionNode 接入 entry 312 的 TIMLDataNode; animLength=121。
- **坑⑱: 用户手备的 `.095` TIML actions 里 PlMotionCommon/PlMotionInput/
  EventCollision00/EventGroup01/EventGroup06/GameParameter 的
  `freehk.timelineParamRaw` 全是空串** → ExportTIMLTypesNode `int(paramCode[1:],16)`
  直接炸。修复=按类名从树里其他 entry 的同族 action 自动查表回填
  (PMC=m48D6114F/PIN=m6FCCD10E/EC=m0CFD985C/EG01=m2EC7FA34/EG06=m30A36F97/
  GP=m01739779)。starType 缺失同理补 "TIML_Action"。**以后 GUI 复制/自制 TIML
  action 必查这两个属性**。
- **无敌帧(树侧)**: PMC.095 mFlag = f0=0x06400000|6 (b26=0x04000000 无敌) →
  f60=0x02400006(基线) = 窗 [0,59]; mTrigger 2048@f0 (用户 TIML 里已埋, 校验保留)。
  旧 312 TIML 的全程 b24 霸体随旧 TIML 一起断开(按"没提=没有"规则,未迁移)。
  **⚠ 深夜5 教训待验: b26 在 306 上曾"毒化整段"(按动作锁存非按键窗)——
  局内须测 f60 后能否被打; 若整段无敌, 312 不可用 TIML b26 布局, 改走 DLL
  no-hit 注入或短动作拆分**。
- **接招窗**: PIN.095 三轨 (mFlag/mFlag1/mFlag2 = m08FD20A6/m6E63FBC7/mF76AAA7D)
  重写为 [(0,0),(100, 原f70档"全开"值)] = f100 起可接下一格, 开放到动作尾
  (0xC300/0xC003000/0xC003F0, 无高位特殊取消位——原生纳刀家系集合)。
- **✅ 导出验证**: frameCount=121, mFlag f0=0x6400006/f60=0x2400006, trigger 0x800,
  PIN 三轨 f100, 全审计 0乱序0小数; 339/340/343/140 CTO 金丝雀全绿。
  部署 md5 8d0bcea48e8aa72ef57941b2b4ba6b42 (备份 .bak-20261003-1955-pre312);
  blend 备份 bak-20261003-194224-pre-312-095wire。
- **⚠ 附带发现: 306/105/141 的二进制侧 TIML 实验(格挡0x12400006/见切形窗/b31窗)
  已被 10-03 全天的四轮全量 blend 导出冲掉** —— 现部署文件里它们是基线态。
  要恢复须树侧重做(坑⑮的正解); 实验期结论未定案, 按需再说。

### 2026-10-03 (6) 312/action100 无敌帧崩溃 + 105 A/B (验证中)

- **症状**: 部署 8d0bcea4(312=095_remap+.095 TIML+b26[0,59]+trigger2048) 后, 99→100
  交接瞬间崩溃(用户观察+日志双证: ValorGauge.log 止于 3:98 IFRMINE, 无 id=100 行;
  DLL 改名停用仍崩 = 非 DLL)。action 100 = 无 FSM 节点的零入边动作(98/99/100/103 同族)。
- **数据面已排除**: .095 TIML family 全部键值逐项核对 = MHW 原生同款(1119/115/135/
  3198/28 均见原版 140/306), 无 MHGU 异物 id。
- **A/B 部署 md5 83e9ce63**: entry 105(原生突刺,131f,有FSM上下文) 加同款 b26[0,59]
  +原生trigger2048; entry 312 只摘 b26(其余 095_remap 接线/.095 TIML/f100 接招窗
  全保留, mTrigger 2048 留守=惰性)。判读: 105崩=配方不可用; 105活+312不崩=
  b26×action100 组合崩; 312仍崩=.095 TIML/动作换装崩因。
- **崩溃转储捕获**: 全局 LocalDumps DumpCount=0 吞 dump(C:\CrashDumps 空,20:13 被碰);
  HKLM 需管理员 → `enable_mhw_dumps.reg`(E:\MHW-arts) 双击一次, 崩后 dump 落
  E:\MHW-arts\crashdumps。背景: 10-01 二进制侧 b26 在同 action100 上曾"无敌不崩"
  (旧 312 TIML), 与本次矛盾 → 崩因也可能是 .095 TIML×100 组合, A/B 判读后再定。

- **✅ A/B 裁决 (user 局内)**: 105 同款 b26[0,59]+2048 = 无敌生效且不崩 → **TIML b26
  无敌配方本身有效**("不能加无敌帧"结论作废); 312 摘掉 b26 后 99→100 仍崩 →
  **崩因 = 312 新内容(.095 TIML family 或 095_remap 动作), 与无敌帧无关**。
  注意 095_remap 此前从未局内播过(10-01 接线一直是待办), 无游戏侧前科。
- **二分部署 md5 f3b47b1d**: 312 = 095_remap 动作 + 旧原生纳刀 TIML(PMC 0x01000000
  flat/PIN f40-55/EC [35,42][61,68], 无 b26 无 2048)。判读: 不崩=.095 TIML 有毒
  (目标配置改在旧 TIML 上重建: b26+f100); 仍崩=095_remap 动作本体踩引擎雷
  (导出警告全是背景噪声族, 数据面无异常)。转储捕获 .reg 已备(E:\MHW-arts\
  enable_mhw_dumps.reg, 全局 DumpCount=0 曾吞 dump; HKLM 需管理员手动导入)。
  blend 备份 bak-20261003-204046-pre-312-oldtiml。

- **✅ 二分裁决 (user 局内)**: f3b47b1d(095_remap+旧TIML) 不崩 → **.095 TIML family
  是崩因**(具体哪个 type 未继续二分, .095 族已断开留在树里无害); 095_remap 动作
  无辜。同时 105 确认 **b26 窗口边界真实生效**(f60 后被打飞, "毒化整段"不成立,
  306 教训限其特定布局)。
- **✅ 终局部署 md5 b97d9d953ec6ebd31259dc1049e83b92**: 目标配置重建在旧 TIML 上 = PMC.413 mFlag
  f0=0x05000000(b26+b24, 原生霸体保留) → f60=0x01000000(纯霸体) + mTrigger
  2048@f0(新轨) + PIN.197 全轨 [(0,0),(100,原生终值)] f100 开放接招。
  动作=095_remap(121f)。待局内终验: 无敌窗边界/接招窗/崩溃消失。
  blend 备份 bak-20261003-204858-pre-312-goal。`.095` TIML family 崩因追查
  (哪一步 type 毒) 挂起, 要查就走 dump 路线。

- **✅ 用户终验完成**: 312 终局配置局内通过(无敌窗/接招窗/不崩)。**残留一次不可复现
  崩溃**, 日志证据指向 10-01 实验态 DLL 干预层: sheath-counter 的 `rule 98,99,100,37
  250 3 77`(强切77) + `bluesim 98,100`(每帧写 +0x2399/0x2CED/0x23A0/0x23A4+FX, 对旧
  312 调的实验) —— 最后会话日志正止于 rule 强切 3:77 的 FRAMETICK/CLK。已把两条规则
  注释停用(mine 被动记录保留), Shift+F6 或重启生效; 恢复=去行首 #。
  备份 ValorGauge.sheath-counter.txt.bak-20261003。若再崩: 先导
  enable_mhw_dumps.reg(E:\MHW-arts) 再复现, dump 落 E:\MHW-arts\crashdumps。

- **✅ 312 定稿 (user 选定"维持现状")**: mFlag = f0=0x04000000(b26) → f60=0x00000000,
  无效的 b24 死位清除(TIML 与实际表现一致: 60帧无敌后正常受击; TIML 霸体本就
  不抗击飞, 与玩家SA同语义)。mTrigger 2048@f0 / PIN f100 接招窗不变。
  部署 md5 见本次部署(312final); blend 备份 *-pre-b24clean。
  不可复现崩溃维持挂起(实验层已停用), 复现路径=enable_mhw_dumps.reg。

### 2026-10-03 ValorLS(勇太4.2)逆向定案: 动作转换=纯数据, 防御=防御帧, DLL 零注入 (工具 _valorls_*.py)

- **DLL(3 钩子旧版/.c; 4.2 交付版 9 钩子, _valorls_42_hooks.py 反汇编枚举)**: 帧tick/
  动作请求处理器(双版本地址 0x140AE9EA0/0x141265770)/斩味函数(0x142134B70/0x142141A60,
  detour 只截参数, 绝回触发时重放 +10 斩味)+0x1419C5820(截 [rdx+0x20] 斩味浮点)+
  0x141CE1670(顺手充能)+0x14119FAA0(动作 98/99/69 事件: 99→转狩技)+**0x14213FA12 直接
  NOP 8 字节**。**全部游戏内存写入仅 weapon+0x2370/0x2374/0x2388/0x2390/0x239A + 请求槽
  {3,id}**; 无伤害钩子/无 HP 写/无防御写(吸血=游戏特性, user 确认)。钓鱼四 art 纯
  强制请求槽 {3,94/111/106/104}; +0x2388=90 需 motion==0xC153(entry339) 才写=回气激活。
- **保护=防御帧(TIML, 自带 LMT)**: 原版见切/居合家族 i-frame 在 PlMotionInput mFlag1
  b26(0x04001000→0x0C003000)+PlMotionCommon mTrigger 2048; mod 自定义 entry(339=art106
  的 motion, 74 帧窗)同配方; 加强签名 0xCFFC10CC/0xCFFC30CC(b26+b24+…)用于 106/107/110/162;
  entry140(见切槽改 13f GP) PlMotionCommon mFlag f0=0x07000000(b26+b25+b24) 全程覆盖;
  **140↔170 TIML 整体互换**=槽位复用。FSM 条件全原版属性(含 条件631 見切り回避成功後の攻撃ヒット)。
- **特殊纳刀保持机制(98/99→LMT306)**: ① entry306 原生 TIML loopStartPoint=129 动画不结束
  →动作不结束; ② 动作 98 触发的线程 10Hz 轮询, motion==0xC132 期间反复写
  **mgr+0x858=1(int)+mgr+0x85C=1.0f**(mgr=*(player+0x7D20)=FSM 管理器条件成员存储,
  指针链 {0x50,0x7D20,0x858}; motion id 链 {0x50,0x468,0xE9C4} 与我们研究一致) 钉死
  原版衰减; ③ b26 窗 f75 起无关闭键穿透循环; ④ 松键=FSM 条件"押してない"退链接火→动作
  切换→线程自然退出。DLL 松键分支只清边沿锁存。
- 工程结论: TIML 属性清单(138 项)无防御角度字段, b26=全向; "前180°"只能靠 FSM/代码侧。
  勇气条/狩技条是 DLL 自有浮点(getyqadd/getskadd 可查), 每招上限 420/830/580/1250。

### 2026-10-04 勇气纳刀动作构建 (valor_sheath_start/done, _valor_sheath_build.py, ALL GREEN 已存盘)

- **拼接点由 2D 姿态扫描实测**(_valor_sheath_scan2d.py, 195×196 全帧): 最佳对
  **(tA=46, tB=131)** — 手 IK 目标差 247=2.6cm/248=3.4cm, 头 2.3cm, 重心 3.8cm,
  体骨平均 ~8°; tA 诊断线 46 处 V 谷(user 目测"50帧后"≈实测 46)。**指标必须含
  247/248 手部目标位置**(体骨 FK 差被手 IK 运行时解算部分掩蔽); bone.isdigit() 对
  "MhBone_NNN" 恒 False 是第一版扫描全错的根因。tB 定 131 → 循环段=196[131..169]
  (38 帧定势); B@0/B@169 自循环有 4 根附属骨(053/056/060/063) 12-20° 不闭合。
- **产物(MASTER MHW-Valor.blend, 全部过 LMTActionParser valid=True, 81 变换轨+Root 双轨,
  与已局内验证迁移动作同签名)**:
  - `valor_sheath_start` **84f** = 195[0..38] + 渐变[38..46] + 196[131..169]
    (尾 8 帧回渐变到 B@131 → 循环回绕无缝; 接缝复检 head/wrap worst 3.5°/2.3°);
    位移通道 B 段加 A@46-B@131 偏移(rootY -0.2187 连续, 与 done 首键一致);
    247 符号翻转对齐; freehk.loopFrame=46(**仅记录, 导出生效在 entry 节点 loopFrame=46,
    LMTEntryNode.construct 写头部**)。
  - `valor_sheath_done` **97f** = 195[46..143] 前移(A 自身轨迹, 松手衔接; 供备选完成段)。
  - `TIML::nTimelineParam::PlMotionCommon`(TIML action): mFlag b26(0x04000000)@f0 常开
    + mTrigger 2048@f0, 无关闭键=穿透循环 —— 用户已验证配方(见切/312 同款, 312 局内通过);
    **TIML 无角度字段, b26=全向**, "前180°"归 FSM/代码侧。
  - 编码: TIML action 级 `freehk.timelineParamRaw="m48D6114F"`(PlMotionCommon);
    fcurve path=`FreeHKTiml.m<hash>`(mFlag=m08FD20A6, mTrigger=m99897C43), bigFlags 拆
    low16/high16 两条(array_index 0/1, 键帧必须对齐), 导出侧 ExportTIMLTypesNode 回读已验证。
  - 树 w03_00 已建 3 个未接线节点(2×LMTActionNode+1×TIMLActionNode), **接线时:
    entry 的 "LMT Animation"←动作节点, "TIML Data"←TIML 节点, entry.loopFrame=46**。
- 坑: ①object 层裸路径 "location"/"rotation_quaternion" 不含点, is_loc/is_rot 要单列;
  ②动作段截取后骨的 4 通道必须齐全(缺通道→导出器 Quaternion 空序列崩, 坑⑭家族),
  done 全曲线强制补 0/97 边界键(eval_hold 越界取端点保持, bezier 外推会漂);
  ③min2 兜底键方向: 末帧单键向前补(f0-1), 否则顶爆 frameCount; ④四元数比较必须
  2*acos(|dot|), 逐分量差值被 q/-q 双覆盖污染(虚报 38°→实为同姿态)。
  blend 备份 bak-20261004-004945。GUI 开着需 File>Revert 后再接线(坑⑭)。

- **✅ 根位移实测更正 (user 确认设计)**: 195_remap 根位移 0→-0.2185m **43 帧即完成**,
  之后全程持平(残余 <3mm); 196_remap 三轴恒 0(纯静止)。2026-10-01(2) 记的
  "195 后撤 -1.44m"是旧量测, 作废。valor_sheath_start 46 帧后根恒定 -0.2187
  (循环段零位移, 回绕静止→静止), done 残余漂移 2mm 级 —— "50 帧后玩家不移动"
  由现有产物自然满足, 无需重建。

- **✅ 尾段位移双变换 bug 修复 (user 抓出, 按其公式验证)**: user 给出精确构造式
  "196 段每个关键帧 = 接点(x,y) + 196 当前帧位移"(196 静止 → 恒等于接点位置)。
  实查 valor_sheath_start 根 Y 键 [77..84] = -0.246→-0.437: fade_to_bt 把"已变换目标"
  与"B 原值"直接 lerp 后又叠一次 bval → 位移通道偏移被加两次(每圈尾漂 0.22m + 回绕瞬移)。
  修复 = 两侧各自 bval 到输出空间后再 lerp, 不再二次叠加。新增公式验证: 接点后
  root X/Y/Z 全键 maxdrift=0.00000, ALL GREEN 重存。旧节点(动作重建后指针悬空)已清,
  树里保留 3 个未接线节点(LMT Action Node.003/.004 + TIML Action Node.010)。
  备份 bak-20261004-004945(修复前 specimen)/002031(pristine)。

- **✅ 循环段扩至整个 196 (user: "动画不够长")**: start = 195[0..38]+渐变[38..46]+
  196 全程(131..169 -> 绕回 1..130), **214 帧, 循环 169 帧(≈2.8s), loopFrame=46**。
  新发现: **196 源内部 B0->B1 有 42.9° 复位帧阶跃**(004/006/007/012 族, 全时间线唯一
  内部缝; B@169->B@0 反而闭合) → 循环窗口跳过 B@0, 回绕渐变目标改 B@1(608 模式带
  tgt 参数)。回绕 B@130->B@131 为源连续帧天然无缝(worst 1.1°)。接点公式复验:
  root X/Y/Z @>=46 全键恒定 maxdrift=0.00000; B-seam 残余 9.8°/4帧=渐变斜率非阶跃。
  悬空节点已清(动作重建会使旧 LMT/TIML ActionNode 指针置空, 每轮重建后要清一次)。
  备份 bak-20261004-115624(改前)/004945/002031(pristine)。

- **✅ entry306 防御帧定稿 = bit28 角度格挡 (user 指出此前"TIML 无角度"结论有误, 已更正)**:
  角度防御就是 **2026-09-13 研究文档的 PlMotionCommon mFlag bit28(0x10000000) 格挡有效**
  (运行时实锤: 正面=格挡咚声不掉血不硬直、背面=正常受击, 锥体随朝向; 当时的验证值
  306184198=0x12400006)。本session 早前"138 项属性无角度字段"的结论错误——w03_00 清单
  只覆盖用到的属性, bit28 语义记在研究文档里。**bit28 与 b26 互斥(b26 赢则格挡不运行)**,
  要角度格挡就必须去掉 b26+2048。
  落地(_valor_sheath_guard_patch.py 原位修补, 不重建动作-节点连线不动): 我们做的
  TIML::nTimelineParam::PlMotionCommon 改为 **mFlag 单键 f0=0x12400006 整段常开,
  mTrigger 轨删除**; 并入 user 精剪过的 entry306 TIMLDataNode(其保留 PlMotionInput.191/
  PlMotionVisual.119/EventCollision00/EventGroup01 四条, 原版带特效+自动纳刀的
  PlMotionCommon 已被 user 删除, 我们这条作为 PlMotionCommon 类目补位, 无冲突)。
  端到端导出回读: entry306 motion=214f/loopFrame=50/TIML 5 类, PlMotionCommon mFlag
  f0=306184198 ✓。user 已调好的三行为(不自动收刀/循环/可接招)不动。
  注: TIMLDataNode 的 TIML Animation 输入 link_limit=0 可挂任意多条 TIML action 并列导出。

- **✅ 新导出 LMT 开机崩溃定位 = TIML 类型头 unkn0 (类目分派 id)**: entry306 接上
  valor_sheath 后游戏无法启动(改名停用即恢复)。字节级对比全文件 249 个 PlMotionCommon
  类型头: **unkn0 恒为 11**(含全部此前开机过的自定义 entry 141/312/339/340/343/358-363,
  均继承自导入原版 TIML), PlMotionInput 恒 9, 其余类目恒 0 —— unkn0 = 加载器类目分派 id。
  我们凭空新建的 TIML action 照 EffectParameter 样例默认写了 unkn0=0 → 全文件唯一的
  PlMotionCommon=0 → 加载崩溃。修复 = freehk.unkn0=11(原位, 接线不动), 重导出五类目
  类型头与各常量一致(PlMotionCommon unkn0=11 + mFlag f0=0x12400006), 已部署待开机验证。
  **教训: 凭空新建 TIML action 时 freehk.unkn0 必须按类目写常量**(PlMotionCommon=11/
  PlMotionInput=9/多数=0), 不能照别的类目样例抄; build 脚本已同步。ENTRIES 区分:
  entry.loopFrame(=-1 之外 23 个先例) 与 TIML loopCtrl(全文件 163:1, 唯 306=1, 但 user
  已在 loopCtrl=1 下进过游戏 → 排除) 均非死因。
  备份 MHW-Valor.blend.bak-20261004-pre-unkn0fix。

- **✅ user 局内终验 (2026-10-04)**: unkn0=11 修复后正常进游戏; entry306 全动画
  **真·带角度防御实锤 —— 背后被打直接击飞**(正面=格挡)。至此 valor_sheath 链路定稿:
  306 = valor_sheath_start(214f, entry.loopFrame=50 循环整段196, 无 b26/无自动收刀/
  可接招) + 5 类 TIML(user 精剪 4 条 + 我们的 PlMotionCommon mFlag f0=0x12400006
  bit28 格挡整段)。后续通路: 正面被格挡 → 反应管理器 +0x769=0x100(研究文档格挡反应
  入口)可作代码侧格挡成功信号; 背面受击 → 正常受击管线 → fsm 100 打断路线可用。
  突刺(105)角度防御 = 同一个 bit28 单字段改动(9-13 已验证过一次), 待接"引到新动作"。

### 2026-10-04 勇气纳刀代码侧: krel 规则(松键收刀) + arule 复用(被打打断) (待局内验证)

- **需求**: 持 3:98 期间 ①被命中 → 强制 fsm 100(受击) ②松开纳刀键(鼠标侧键上) →
  强制 3:6 收刀; 其余不干预。98 以后可能换节点 → 动作 id 全部走配置。
- **实现 = sheath-counter 规则系统扩一个模式**, 两条规则纯配置:
  - `arule 98 3 100` — 现成 arule(DmgCalc 连接命中 + attacker≠player → arm 强制),
    自带致死锁血(HP 钳 1)。**修了一个真 bug: arule 单发锁原是 g_clangLatch, 而该锁只在
    86/101/102 复位 → 第一次被打打断后, 之后所有持刀被打永久失效**; 换成按动作实例的
    fired/lastActId(id 变化即重新武装), 与 bit 规则同模式。
  - `krel <ids> <vk> <toSet> <toId>`(新) — PollSheatheCounter 里 GetAsyncKeyState
    down→up 边沿 → 纯 arm(g_forceSet/Id+g_forceArmed), **故意不走 OpenCounterWindow**
    (纳刀收刀不需要 no-hit 注入), 无 latch(边沿即单发, force 被拒则下次按压重试)。
    侧键上=XBUTTON2 vk=6, 侧键下=5。
  - 强制执行走既有 TryQueuedForceWrite(游戏线程 getter hook), 配置追加于
    ValorGauge.sheath-counter.txt(旧实验规则保持注释)。
- **部署状态**: DLL 新版本 md5 10ec5fc1 在 ValorGauge/build/Release/ 待拷
  (游戏运行中 DLL 被锁, 关游戏后拷到 nativePC/plugins/), 配置规则已追加。
  顺手修了 ValorProbe 目标的既有 ifdef 失衡(ApplySheatheMotionPatch 调用点)。
- **已知边界**: arule 以 attacker≠player 判"被打", 联机时队友被打会误触发
  (日志有 SHEADRULE hit a2=... player=... 行, a2==player 确认后可收紧);
  致死钳 1 与"防秒杀"后续设计共用入口。

- **✅ 局内实测反馈 + drule 补全**: 日志证实持刀 id=3:98 正确、规则加载 2 条; 正面被打
  = dispatch REJECTED(格挡否决,无伤害连接,arule 天然不触发 ✓) **但引擎加载 {3,37}
  格挡反应把保持态踢出 98**(松键规则因此没机会触发)。补全: 配置加
  `drule 98 3 100`(rejected dispatch -> 打断进 100; drule 分支本来就在, 纯配置,
  Shift+F6 热重载即生效); 代码修 drule 的 g_clangLatch 死锁门(同 arule, 换
  fired/lastActId 按实例复位) + drule 触发时写 reassert(吞 {3,37} 1.2s, 保持态不
  经过 37 直接落 100)。新 DLL md5 4054ea24。三规则现状: arule(背面真命中)/
  drule(正面格挡否决)/krel(松键收刀), 全部围绕 3:98。

- **✅ krel 验证通过 + drule 撤换**: 日志实锤 krel 全链正常(侧键 vk=6 确认,
  down=[6] 唯一键; 松开边沿 -> force 3:6 accepted -> action changed forced id=6
  -> 收刀待机)。drule 对自定义动作 98 失效: 其 dispatch 返回恒非 0xD(环境帧亦然),
  入场第一帧即触发 = "按下即 100" → 配置撤换为 **bit250 位规则
  `rule 98 250 3 100`**(正面格挡交互时 250 升起, 无攻击持刀不升; krel 探针
  SHEATHKRELDBG 留用)。Shift+F6 热重载生效, 待局内确认三路齐全。

- **✅ 局内二轮: 规则全部真实触发, 新漏点 = 格挡反应变体 3:35**: 日志实锤 drule 触发
  → FORCE accepted → changed (forced) id=100 ✓; krel 全链 ✓(探针 down=[6] 唯一键,
  松开边沿 → force 3:6 accepted → id=6 ✓)。**"有时候看到 3:35" = 格挡反应家族变体**:
  引擎按情况在 35/36/37 里选, 我们只吞 37 → 35 漏网顶掉 100。**ValorLS 无此处理**
  (grep 证实: 请求改写点仅 art 触发; 白名单 0/301-306/501-506 为免重置表) —— 它不需要:
  其 306 用见切式窗口(b31+b22, 无 bit28), 走否定/叮系统不产生格挡反应; 我们用 bit28
  真格挡就必须自己吞。修复 = DoActionHook 吞单扩为 **35/36/37 全族, 且当前动作处于
  任意被监视动作(现在=98, 换节点自动跟随)期间无条件吞**(reassert 窗口之外也吞),
  SheathWatchedAction() helper + 前置声明。DLL md5 1aa879d9 已部署, 待局内验证。
  若正面被挡后 100 仍偶发被顶: 下一步查非格挡族反应 id(日志 action changed 序列)。

- **✅ 35 漏吞真因 = DoActionHook 内部顺序**: 追踪器更新(含 "action changed" 日志,
  行 1256)在吞单(行 ~1280)**之前**执行 → 吞单检查 watchedHold 时 g_lastLongSwordAction
  已被覆盖成 {3,35} → SheathWatchedAction(3,35)=false → 不吞。修复 = DoActionHook 顶部
  先快照 prevSheathAction, 吞单用前一个动作(=玩家所在 98)判断。DLL md5
  e8ed4216 已部署, 待局内验证。判定链备忘: 35 经 DoAction(已证实, "action changed
  id=35" 唯一输出点在 DoActionHook), 非缓冲直写。

- **✅ 35 仍漏的第二层真因 = 吞单位置在追踪器更新之后**: 上一版吞单虽触发(日志
  "swallowed 3:35" ×3), 但 DoActionHook 里**追踪器更新先于吞单** → 吞单前追踪器已被
  改成 {3,35}: ① bit250/arule 规则的 duringIds(98) 对着追踪器里的 35 永久失配 →
  强制 100 永远不来; ② 引擎重发第二次 DoAction({3,35}) 时 prevSheathAction 已是 35 →
  二次不吞 → 35 真播。修复 = **吞单整块上移到 DoActionHook 顶部(prevSheathAction
  快照之后、追踪器更新之前)**: 被吞的反应完全不污染追踪器(保持 98), 规则继续工作。
  DLL md5 d5eabefd 已部署。验证点: 日志 "swallowed 3:35" 后应紧跟 SHEATHCOUNTER
  (bit250 触发) + FORCE 3:100, 且不再出现 changed id=35。

- **✅ 被打信号换 bit222 + bit 规则哨兵重武装**: 吞掉反应动作后 bit250 不再升起
  (旧的 rose=[250] 发生在被吞前的 3:37 反应动作里)。日志实锤**每次格挡命中事件位
  222 脉冲一拍**(ev6 0x0→0x40000000, 6 打 6 中; 另有 225/147 部分命中伴生) →
  配置改 `rule 98 222 3 100`。bit 规则同样补哨兵重武装(离开监视集置
  lastActId=0xFFFFFFFF, 同 id 重入 98 也重新武装; 旧逻辑第二次持刀的第一下会被
  fired 卡住)。DLL md5 9bee73aa + 配置已部署, 待局内验证。

- **✅ 重击路径补全 = 吞单即信号**: 轻击(bit222 脉冲)走 bit 规则 ✓; **重击(引擎选
  35/36/37 反应)脉冲的位不同且反应被吞后无事件位** → 吞单本身即每击一次的精确信号。
  SheathArmHitRule(holdId, reactionId): 吞单时按 holdId 匹配规则 target 直接 arm
  (SHEATHGUARD 日志行)。**教训: DoActionHook 在 sheath 规则表声明之前, 相关逻辑一律
  helper + 前置声明**(本轮连续踩了两次声明顺序坑)。DLL md5 01da4c89 已部署。
  现役信号矩阵: 轻击=bit222 脉冲 / 重击=反应吞单 / 背面真命中=arule / 松键=krel。

- **✅ 刚1 被劫持成 100 的真因 = 强制目标单例被吞单武装覆盖**: g_forceSet/g_forceId
  既是 force-action.txt 的配置默认值(刚1=3:77)又是"待发目标", 吞单武装把默认值改写成
  3:100 后, **弹反链的四个武装点(iai bit250/见切 bit15/deflect flash/Ctrl 窗口)只 arm
  不装填**(依赖启动默认) → 之后所有刚1 都打出 100。修复 = 配置默认值与待发目标分离:
  新增 g_forceCfgSet/g_forceCfgId(LoadForceActionConfig 与 Shift+F6 重载同步), 弹反链
  四个武装点 arm 前从配置副本恢复。DLL md5 2bb3063c 已部署。刚1(3:77)与收刀回避
  (3:100)两条强制链现已互不污染。

- **✅ user 局内终验 (2026-10-04 晚)**: 四路全部通过 —— 持 98 轻击/重击/背面被打均进
  3:100(收刀回避), 松开侧键上键进 3:6(收刀), 见切弹反恢复刚1(3:77)且与持刀链交叉
  不串。勇气纳刀代码侧完结。遗留优化(未排期): 98 换专用 fsm 节点(现借 98); arule
  的 attacker≠player 判据在联机会误触发(a2=defender 字段待钉, SHEADRULE 日志行可证);
  krel 探针 SHEATHKRELDBG(8 采样/会话)留用可删。

### 2026-10-04 (2) 滑歩気刃 106/107/108: 气刃消耗 + 气刃特效 (id 探测定案, 待局内验证)

- 需求: 3:106(気刃１スリR) / 3:107(気刃２スリF) / 3:108(気刃２スリL) 加气刃特效 + 各消耗 0.25;
  "气刃不足不出招"的门由用户改好的 FSM 条件承担, DLL 只管扣除与特效。
  **定案: vanilla 每招特效/扣气刃都走按动作 id 硬编码的每招 handler, 改不了数据 → 只能走 DLL 的 id 探测 (gauge-rules)。**
- **新规则 `fx <set> <id> <fxId>`**: 动作开始时对 player+0x8808 emitter 经 ADD670
  (g_fxNotifyA, 与 F7 试枪同一调用通道, 返回句柄无消费方) 发特效; `0x7D2` = 气刃斩系通用红光
  (user-verified)。fxId 支持 0x 前缀。当前唯一已验证的身体红光 id 就是 0x7D2 —— 按等级变色的
  刀光走事件上下文另一套, ADD670 打不出来; 想换 id 直接改配置。
- **cost/fx 触发时机从"DoAction 接受"迁到"动作真正开始" (begin watcher)**:
  连招派生请求在接受时上一招还在播 (缓冲 +0xBC/+0xC0), 接受点扣气刃会在"被打吞掉缓冲请求"时白扣,
  特效会闪在上一招上。`PollGaugeBeginRules` (property 钩子每帧, 游戏线程) 监视 controller+0xAC/0xB0:
  set:id 变化 或 同 id 下实例帧 +8 回落 >0.5s (缓冲重入同招, begin 清零帧计数) = 一次 begin →
  `ApplyBeginGaugeRules` 发 cost + fx。**gate/regen/timer 仍在接受点, 行为不变**; cost 从
  ApplyActionGaugeRules 移出 (无既有规则在用, 无回归)。武器 LS 硬门控提取为共用 `GaugeRuleWeapon()`。
- 配置已部署: gauge-rules.txt 末尾 `cost 3 106/107/108 0.25` + `fx 3 106/107/108 0x7D2`
  (Shift+F6 可热重载)。DLL md5 ebdfd659 已部署 (mod)。
- 局内验证点: 滑歩气刃起手瞬间红光 (不早于前一招结束) + 蓝条 -0.25 + 日志
  `GAUGERULE cost act=3:10x -0.25` / `GAUGERULE fx act=3:10x id=0x7d2`; 被打吞连招时不应扣。

### 2026-10-04 (3) 勇气连段常量 + 刚1 阈值定案 + "刚1 变气刃斩"取证

- 用户定案: 刚1 (sheath-rewrite) 气刃门保持**默认 0.15** (`g_sheathRewriteMinGauge`,
  dllmain.cpp:896, 无配置文件时代码默认即生效)。勇气连段: **105/106=一文字, 107=无双斩,
  108=连段第三招**, 四招统一 = cost 0.25 + fx 0x7D2 (begin 红光)。
- **代码 seed 常量**: `LoadGaugeRulesConfig` 开头注入 105/106/107/108 的 cost+fx;
  gauge-rules.txt 显式行按 (type,set,id) **替换** seed (配置可覆盖), Shift+F6 重载同路径
  ——配置文件丢失也不丢行为 (sheath-rewrite.txt 缺失事故的教训)。现有 106/107/108 行降级为
  覆盖示例。日志标识 `Gauge rules: N loaded (incl. valor-chain seeds 105/106/107/108)`。
- 局内实测 (上一版部署): fx/cost 实际触发 106×7 / 108×11; 107 当次会话没出招, 105 当时无规则
  ——本版四招全覆盖待复测。
- **"待机刚1 按下后变气刃斩"取证 (日志 69.6万行全量 + 代码)**:
  - 假设 A (刚1 出招后迟到 {3,11} 顶掉): 全日志 124 次 SHEATHREWRITE, 其后**下一个**动作变化
    是 id=11 的 = **0 次** (紧邻 6 行窗口 0/124, 末会话宽窗口 0/5); `SHEATHPROBE ... last=3:86`
    全日志 **0 条** → 无证据, 暂按不成立。
  - 假设 B (**收刀过渡窗漏改写, 证据成立**): 收刀判定 = "上一个动作是 set 1", 但收刀动画本身
    是 set-3 (3:0/3:6 家族, 3:6=松开侧键收刀)。R2 按在收刀动画期间/刚收完的一瞬 → lastSet=3
    → 不改写 → 原版气刃斩1(带前摇)直接出来。日志 `SHEATHPROBE blocked last=3:0` ×67 /
    `last=3:6` ×27 (探针每会话限12条, 实际更多)。
  - 修复方案 (待确认后实现): ① sheath-rewrite 的收刀判定扩展: last ∈ set1 ∪ 收刀过渡家族
    (可配置, 初值 3:0/3:6); ② 改写命中后 ~0.8s 内吞掉同 fromId 迟到请求 (复用 35/36/37 吞单
    机制, 吞单日志兼作假设 A 的探测器)。
  - 设计问题: 收刀动画**播放中**按 R2 → 改写成刚1, 还是吞掉不出招? (收刀冷播有姿势跳变,
    09-25 章节老问题)

### 2026-10-04 (4) "走路刚1变气刃斩"破案: 收刀态行走 R2 = 动作 id 12 (user 方向正确, WASD 相关)

- 用户否掉"收刀过渡窗"解释, 提出与行走按 WASD 有关 → **全量日志迁移分析实锤**:
  `1:4(收刀走) -> 3:12` ×**187**, `1:8(跑) -> 3:12` ×6, `1:585 -> 3:12` ×15, 而
  `1:0(站) -> 3:11` ×158 —— **收刀态移动中按 R2 的请求是 3:12**(拔刀气刃斩迈步变体,
  与 09-30 编排器 {3,11}/{3,12} 兄弟对吻合), 不是 11。fromIds 只有 11 → 静默放行原版,
  且 fromIds 不匹配的分支**一行日志都不打**(探针只覆盖匹配后阶段) → 几周不可见。
- 修复: ① fromIds 默认 `{11, 12}` (g_sheathRewriteFromIds); ② 新增 `SHEATHSEEN`
  日志 (限30条/会话): 收刀态(set-1 前置)收到不在 fromIds 的 set-3 请求时打印 id,
  攀爬/游泳等剩余变体一次会话即可暴露。
- 迁移表补充 (set1 -> set3): `1:8->3:7` ×447 / `1:4->3:7` ×268 = 拔刀纵斩(△键),
  不在改写范围(用户只要 R2 系); `1:440/432 -> 3:18` = 翻滚; `3:5` = 普通拔刀(△系)。
- 收刀过渡窗 (3:0/3:6 期间按 R2) 仍是独立的二类洞 (blocked ×94), 优先级低于本修复,
  修不修等用户复测后定。
- **教训: 静默分支必须留探针** —— fromIds 不匹配纯 return, 让 187 次泄漏零痕迹。

### 2026-10-04 (5) fromIds 修复无效复盘: loader 局部默认值覆盖全局 (与 g_forceCfg 分裂同款教训)

- 上一版把 fromIds 默认改成 {11,12} 改在了**全局变量**上, 但 `LoadSheathRewriteConfig`
  有自己的局部 `fromIds{11}` 且加载时无条件覆盖全局 (init + Shift+F6 都走) → 新 DLL 日志
  依旧 `requests 3:{11}`, SHEATHSEEN 照抓 `3:12 not in fromIds (last=1:8)` —— 诊断对、修复
  改错层。修复 = 改 loader 局部默认 `{11,12}` (933 空回退同步), 全局变量注释标注
  "effective default lives in the loader"。
- **教训 (第二次): 配置 loader 拥有并覆盖默认值, 改默认必须改 loader** —— 与 1b0b2f2
  (g_forceCfg* 分裂, 吞单武装覆盖强制默认) 同类: 默认值有两份时, 生效的永远是 loader 那份。
- SHEATHSEEN 首日战果: `3:5 (last=1:0)` = **纳刀系动作本身** (收刀家族 {5,6,9,10}),
  不该改写, 行为正确无需处理; `3:12` 多次 (last=1:8/1:4) = 修复目标, 本版已收编。
- 附带实测: 3:12 原生自带 cost 0.15 (GAUGEEVT val=-0.15) + 原生 0x7D2 红光
  (FXNOTIFY caller +0x2122559) —— vanilla 迈步拔刀有自己的气刃呈现, 改写成刚1(86)后
  由 86 的呈现接管。
- 部署 md5 63a454a8。验证点: 启动行 `Sheath rewrite: ... 3:{11,12} -> 3:86`;
  收刀态走动按 R2 出刚1 + `SHEATHREWRITE: 3:12 -> 3:86`。

### 2026-10-04 (6) 剩余泄漏定案: 纳刀动作 3:9 播放中按 R2 (用户"1:8→1:4中间态"的实体)

- {11,12} 修复后的测试局: 改写全中 (12×13+11×2, set1→set3 迁移 15/15 → 86),
  SHEATHSEEN 零条 —— 请求 id 无漏网。仅剩 SHEATHPROBE `blocked: last=3:9` ×4。
- **3:9 身份 (前驱/后继实锤)**: 后继 = 1:15 ×293 / 1:8 ×258 / 1:4 ×100 (89% 回收刀态
  locomotion) → **3:9 = 纳刀动作**; 前驱 = 3:1(持刀走) ×464 / 3:65(翻滚恢复?) ×182 /
  翻滚。历史: 3:9 → 3:12 ×26 / → 3:11 ×22 = 收刀中按 R2, 请求仍是 11/12, 只被
  "前置必须 set1" 挡住 → 原版气刃斩1 在收刀动画里蹦出来。用户主观感受为"快跑→慢走
  中间态"(跑动中收刀/刚收完的窗口)。**1:8→1:4 本身无中间动作** (transition 表无其它 id)。
- 修复: 收刀判定 = last ∈ set1 ∪ {3:9, 3:6} (kSheatheActionIds 常量; 6 = 勇气持刀
  松开收刀)。**明确排除 3:0/3:1/3:2 = 持刀待机/走/跑** (3:0→3:11 ×347 = 持刀 R2 原版
  气刃斩, 正确放行; 修正此前"3:0 身份待定"的记录) —— 排除 3:5 (罕见, 语义未钉, SHEATHSEEN
  继续监视)。部署 md5 e14b8859, 待局内复测: 收刀动画中按 R2 应出刚1
  (`SHEATHREWRITE ... last=9` 类日志 + 无 blocked last=3:9)。

### 2026-10-04 (7) 刚1 气刃消耗: 快照-结算 (snapshot - 0.15), 放弃逐事件拦截

- 需求: 刚1 借见切(86)槽, 每次按下气刃被清; 目标 = 每次刚1 净消耗 0.15 (clamp ≥0)。
- **取证推翻单点拦截**: GAUGEEVT 全量直方图 (act=3:86) = -0.1 ×987 / -0.2 ×45 / +1.0 ×6,
  **全日志零条 -1.0** —— 09-24 "见切 -=1.0 清空"的记录与实测不符; 且实测序列里每招扣完后
  气刃都回到 1.0 (0.9→1.0 / 0.85→1.0 / 0.75→1.0) 而 GAUGEEVT **无对应事件** = 存在绕过
  FUN_142122410 的直接写入者 (未钉)。逐事件改值会漏, 放弃。
- **实现 = 用户方案的轮询版**: `ApplySheathRewrite` 改写命中 (toId==86) 时快照
  `+0x2368` → `g_gang1GaugeSnapshot`, 武装 scope (5s 兜底 deadline);
  `PollGang1GaugeScope` (property 钩子每帧): 当前动作离开 86 或超时 → 一次性写回
  `clamp(snapshot - 0.15, 0, 1)`, 日志 `GAUGERULE gang1 settle: gauge a -> b`。
  86 播放期间 vanilla 的花费/回填随便走, 结算时一笔归位 —— 净效果恒为 -0.15。
  只在改写路径武装, **自然见切(持刀 86)行为不变**; 后续连段招 (67/106/108...) 的
  消耗照常作用在结算后的值上。
- 待局内验证: 收刀刚1 一次 → 蓝条恰好 -0.15; 连段时后续招正常再扣。

- **(补丁) settle 竞态修复**: 首版 settle 读 controller 当前动作 (+0xAC/0xB0) 判"离开 86",
  但改写请求是**缓冲**的 (上一招还在播) → 当前动作仍是 {1,0} → 武装后第一帧即误结算
  (实测 settle 0.418→0.268 紧跟改写行, 之后 86 的 -0.1 和收尾继续扣 → 用户看到清空)。
  修复 = 改读 **tracker (请求流)**: 改写时 tracker 已是 86, 下一个请求到来才结算 —— 结算
  落在下招 begin 扣费之前, 连段消耗正确叠加在其上。部署待游戏关闭。

### 2026-10-04 (8) 刚1 气刃最终形态: 结算 + 1s 保护尾窗 (whiff 清空 = vanilla 见切设计)

- user 实测定案: **弹反成功 → x-0.15 ✓; 失败(打空) → 清空** = vanilla 见切的原版设计
  (打空丢条), 且失败清空是**结算之后**才落地的直接写入 (绕过 FUN_142122410, GAUGEEVT 无痕)。
- 实现 = 两相状态机 (`g_gang1Phase`):
  - **phase 1 (waiting)**: 改写武装 → tracker 离开 86 (5s 兜底)。vanilla 随便动, 无守卫。
  - **settle**: 写 `clamp(x - 0.15)` → `expected` 同值, 进 phase 2 (尾窗 1s)。
  - **phase 2 (guard tail)**: 账面 `expected` 记账 + 每帧对账 —— GAUGEEVT 可见的**负**事件
    (连段原生扣费) 叠上 expected (正事件 = 见切成功回条, 压掉, 维持 -0.15 净值);
    实际值偏离 expected >0.01 = 直接写入 (失败清空) → 打回 + `gang1 guard` 日志 (限10条)。
    尾窗结束再写一次 expected (吸收衰减漂移), 归 phase 0。
- **自吞坑**: 106/108 的 cost 规则走 trampoline 绕过 GaugeAddHook, 尾窗守卫会把自己扣的
  费打回去 → ApplyBeginGaugeRules 的 cost 分支补 expected 记账。
- 成功路径的净效果不变 (settle 压掉 vanilla 回填); 失败路径的清空在 settle 后 ≤1s 内被
  revert。部署 md5 7144d7df。验证: 打空刚1 → 蓝条也稳定 x-0.15 + 日志 `gang1 guard:
  reverted direct write`。

### 2026-10-04 (9) 刚1 气刃终版: 命中确认即撒手 (vanilla 回满设计入章)

- user 确认 vanilla 见切设计: 规避 → 回砍**砍到** → 气刃自己回满 (打空才清条)。守卫尾窗
  之所以"还是错的": 成功路径的回满是**命中确认时点的滞后直接写入**, 与 1s 尾窗的相对时序
  不定 —— 落窗内被压、窗外又弹回, 观感忽上忽下。
- 终版模型 = 顺着 vanilla 分叉: **`NotifyPlayerHitConnected` 里 phase==1 (刚1 窗口内) 的
  本地命中 → `g_gang1Phase=0` 整体解除** (`gang1 hit-confirm` 日志) —— 砍中 = vanilla
  自己回满, mod 全程不碰; **打空 = 无命中, scope 走完** settle x-0.15 + 尾窗 revert 清空。
- 语义: 刚1 砍中 = 免费+回满 (vanilla 奖励), 打空 = 0.15。部署 md5 ae2957a4。
  验证: 打空 → 稳定 x-0.15 + `gang1 settle`; 砍中 → 回满 + `gang1 hit-confirm`,
  全程无 settle/guard 行。

### 2026-10-04 (10) "空了"取证: 蒸发在持刀期, 刚1 本身清白 + GAUGEWATCH 每帧对账器

- ae2957a4 测试局账本: 唯一一次收刀刚1 **砍中了** (HITATT +110 → hit-confirm 解除 →
  气刃回满 1.0, 后两次 86 均从 1.0 起扣) —— 命中路径正确。**0.9 → 0.52 → 0 的蒸发无任何
  事件行** (无 67/106/108 扣费, GAUGEEVT 静默) = 持刀游玩期间 (自然见切打空/阶梯衰减) 的
  账面外直接写入; 最后收刀刚1 在 gauge=0 被 reject (minGauge=0.15 门), 出原版气刃斩1 =
  用户看到的"空了"。
- 新工具 **GAUGEWATCH** (PollGaugeWatch, property 钩子每帧): 对 weapon+0x2368 逐帧对账,
  |Δ|≥0.02 记 `GAUGEWATCH d=.. g=a->b act=set:id` (限200条/会话; 每帧衰减 ~0.0008 被阈值
  滤掉, 阶梯式衰减 -0.05/步会被抓)。一轮即可绘出全部隐形写入者的时刻/动作分布。
- 部署 md5 a46d04fc。待用户复测一轮 (含打空收刀刚1) 后读 GAUGEWATCH 定位蒸发源,
  再决定: 衰减要保留 (vanilla 经济) 还是给刚1 连段挂账面保底。

### 2026-10-04 (11) 成功信号纠错: HITATT ≠ 弹反成功, 换 bit15; 帧杀注入与回满互斥定案

- user 接受 vanilla 经济 (打空=清空是设计), 但测试打空刚1 "还是清空" —— 根因 = 上一版用
  **HITATT (回砍物理砍中) 当成功信号**: 贴着目标测试回砍必砍中 → hit-confirm 解除守卫 →
  vanilla 判定未弹反 → 清空无拦截。**物理命中 ≠ 弹反成功**。
- 修复: 解除信号换成 **event bit15 上升沿** (player+0x8938 & 0x8000, 见切成功锁存, 弹反链
  同款; arm 时快照基线防残留位)。HITATT 解除逻辑删除。
- **结构性定案 (09-25 老警告坐实)**: 收刀刚1 的 0.2s no-hit 注入把怪攻击排除在仲裁上游 →
  **叮/bit15/成功回满在收刀刚1 上根本不可能发生** ("帧杀保护与弹反计数互斥") → 当前模型:
  收刀刚1 恒 = x-0.15 (无论砍没砍中)。要真弹反 (叮+回满) 必须去掉注入试原生窗口
  (sheath-rewrite.txt nohitSec=0), 而 09-25 的 open question (FSM 停在待機1000 不进节点28,
  TIML 防御窗打不开) 大概率让纯去掉= 挨打 —— 真解法在状态侧/TIML 侧, 待排期。
- 部署 md5 82a5db9c。验证: 打空/砍中收刀刚1 都应稳定 x-0.15 (日志 gang1 settle, 无
  hit-confirm); GAUGEWATCH 继续监视条面蒸发。

### 2026-10-04 (12) 刚1 消耗语义对齐 + 恢复时机四触发 (user 定稿设计)

- user 对齐: **消耗恒 0.15, 与打中/弹反/打空无关, 唯一条件 = 用户出招** (入场门 = 红刃 +
  气刃 ≥0.15, 已有)。中间写入者众多 → 不拦截, **绕开**: 出招前快照 x, 到时机一次性恢复
  x-0.15。恢复时机 = 四者取先:
  1. **叮** (bit15 上升沿, player+0x8938 & 0x8000; arm 时存基线);
  2. **56 帧** (kGang1RestoreFrame, TIML 60fps → 0.933s; 用 86 实例 +8 帧计数, 取消即冻结
     不误触发);
  3. **controller 当前变为其它 set-3 动作** (86 结束/被打断 —— 出招即计费; 注意初段缓冲期
     controller 还是前一招 set1, 不会误触发);
  4. 5s 兜底。
- settle 后 1s 守卫尾窗保留 (负事件记账/正奖励压掉/账面外直接写入 revert) —— 兜住 56 帧
  之后才落地的 vanilla 清空。
- 日志 `GAUGERULE gang1 settle[parry(bit15)|frame56|left86|deadline]: gauge a -> b`。
- 部署 md5 3a7a7b79。验证: 打空收刀刚1 → 约 0.93s 时条 = x-0.15 并保持; 接连段照常再扣;
  HITATT 不再影响任何行为。

### 2026-10-04 (13) 武装点统一到 DoAction: 自然见切(持刀86)也纳入刚1 计费 + 测试无效局复盘

- 复盘 ae2957a4 测试局: **无任何 SHEATHREWRITE** —— 三次 86 (+68/+152/+234) 全是持刀自然
  见切, 打空 → vanilla 清空 (+70/+154/+236 GAUGEWATCH d=-1), 机制从未武装 (只覆盖收刀改写)
  → 用户测的招不在管辖内。且局内有**回满器**活动 (每次扣费后一帧内被补回精确扣减量:
  cost -0.25 → GAUGEWATCH +0.25 → 1.0) = 训练器气刃功能, 它开着任何 settle 都无法停留。
- 修复: 武装点从 ApplySheathRewrite 挪到 **DoActionHook post-accept** —— 任何本地玩家
  set-3 actionId==86 接受即 ArmGang1GaugeScope (收刀改写/持刀自然见切同一招同价 0.15;
  快照确定性先于起手扣费; phase==1 跳过/phase==2 重武装; LS 硬门控保留; probe 构建需
  #ifndef 包裹 —— gang1 全局在 probe 排除区)。ApplySheathRewrite 侧武装删除。
- 部署 md5 51cd4c4b。**测试前提: 训练器气刃功能必须关** (只留锁红刃), 否则 -0.15 无法停留。

- **✅ user-verified (2026-10-04)**: 训练器气刃功能关闭后, 收刀/持刀 86 (刚1) 打空/砍中
  均稳定结算 x-0.15。刚1 气刃链完结 (武装 DoAction 统一入口 + 四触发恢复 + 1s 守卫尾窗,
  最终形态 md5 51cd4c4b)。遗留备忘: 若未来要收刀刚1 真弹反 (叮+回满), 需解决 nohitSec
  注入与防御仲裁互斥问题 (09-25 open question, 状态侧/TIML 侧)。

### 2026-10-04/05 见切防御两层模型定案: L1 沉默无敌(状态侧) / L2 无效化交互(TIML 侧) + bit15 写入链钉死

- **user 判决实验 (10-04/05)**: entry 140 删掉**全部** TIML → 局内无敌帧依然生效, 但**无叮、
  无 bit15、无动作切换**。复现了 09-07 "I-frames that survive TIML deletion" 旧案并强制修正模型:
  TIML 防御窗 = 真实但只是**可授权的加法层**; 见切还有一层不经过 TIML 的防御。
- **两层模型**: **L1 沉默无敌** = FSM/动作状态侧授予 (09-25 收刀重写路径没有它 → 当时的"没无敌帧";
  查询层排除, 攻击"没发生过", 无叮无 bit15, 删 TIML 不死; 翻滚同族)。**L2 无效化交互** = TIML
  防御窗 (窗位+mTrigger 2048 结构; 140 原版长窗位实为 0x00400000, 旧文档表里写 "b26 f0-f130"
  是位编号笔误——结构才是重点) → 命中到达仲裁被判定无效 → 家族演出 (叮/蓝光) → hub 标志 →
  **bit15 上升沿** → 原生 763/764 派生重映射 + mod auto-force 刚1。pool+0xE4 注入 = L1 的
  查询层仿制品, **结构上永远产不出 L2** ("纯粹的防御")。
- **bit15 写入链钉死 (反编译 6430953 行)**: `FUN_141f6bd80` (玩家主更新每帧事件位传播步,
  调用点 6432955, 门控 player+0x1140 bit8) 每帧拷贝
  `if ((*(uint*)(*(player+0xe470)+0x170) >> 8 & 1)) { *(uint*)(player+0x8938) |= 0x8000;
  *(uint*)(player+0x8968) |= 0x8000; }` — **bit15 = [player+0xe470]+0x170 bit8 的逐帧拷贝**
  (状态阵列 +0x8938 与边沿阵列 +0x8968 同帧写)。同函数还有 bit10 ← hub+0xc bit0。
  player+0xe470 = 玩家 FSM/输入枢纽 (区域邻接 0xDE 属性链 FUN_141922190)。hub bit8 的置位方
  未钉 (别名写入静态不可见), watcher 一测即知。
- **mTrigger 语义破译 (新钉)**: mTrigger 值 = **事件位掩码**, 过帧即向 +0x8938/+0x8968 注入:
  2048=bit11 (b26 无效化窗的 commit/gate — 解释"窗内必须有 2048"配方), 1024=bit10,
  65537=bit0|16 (bit0=气刃增减公共门), 306 尾帧 f73=1024。**140 的 TIML 从不触碰 bit15** →
  bit15 只能来自 hub (状态/仲裁侧)。
- **特殊纳刀 306 无 TIML 无效化窗 (原版解码)**: mFlag 仅 f0=b25|b22 → f10 +b24 → f15+0x6
  (霸体 f10 起), mTrigger 仅 f73=1024 — **没有 b26/2048 结构**。用户实测其"防御帧(弹反)"为
  状态/原生层守卫, 不跟动作数据走。bit15 是**见切专用**锁存 (唯一消费者 = 派生重映射 763/764;
  居合用 bit250, 各机制各一位) → 特殊纳刀防御帧理论上不置 bit15, 待 VSEEK 微测确认。
- **FSM 可用属性新钉**: `オーラレベル赤以上` 存在于游戏属性表 (UTF-8 @ 文件偏移 0x30b8cd8,
  与 オーラレベル白以上 相邻) → 收刀路径 FSM 链接条件可原生表达红刃门。
- **干净化路线 (待判决实验)**: ① 140 写回标准窗 (105 模板 f0=121634822/f18=33554438/f120=6 +
  2048@f0), sheath-rewrite nohitSec=0, 测收刀刚1: 叮? bit15? force 刚1? → 成立则删
  dllmain.cpp InjectNoHitLocked(sheath); ② FSM-Studio 加 98→28 链接 (cond R AND
  オーラレベル赤以上) = 转向锁 + L1 + 28 出边派生; 进 28 后节点动作进程直发 86 (09-22
  "节点进入压过 request"), DLL 改写在红刃态变冗余保险; ③ bit250/0x23A0/CondEval631 三个
  备胎触发器在 bit15 可靠后退役。
- **部署 entry 140 现状 (10-04 dump)**: 用户已删全部 TIML; mFlag 曾为 f0=f130=0x80000000
  (无任何防御窗, 09-25 "没无敌帧"的另一半原因); 当前 mTrigger 已随删除消失。

### 2026-10-05 判定链全景钉死 + bit15 降级 (日志实锤, 推翻同日早前"bit15 见切专用"结论)

- **叮的真身 = 武器侧无效化事件处理器链 (反编译全读)**: 命中经仲裁判为无效化后向防守方武器
  派发**交互事件类型 8** → `FUN_1421038B0` (RVA 0x21038B0): 门控 = 实体标志
  (bit4→当前动作==0x45 / bit1 / bit10), 反应类型 4/7 直接返回; 路由 = byte
  `[weapon+0x3168]` (工厂 FUN_1421039F0, 0x3190 对象, 初值 0): 0→`FUN_142125060`
  (set3 且 id==86: +0x2399 一次性锁存 + FX 0x7D4=叮 + +0x23A0=playerParam+0x8A8),
  1→`FUN_142124f70` (id∈{101,102}: +0x2CED 掐点 + 0x7D4 + +0x23A4=+0x8B8)。
  动作结束 FUN_142123510 消费 +0x2399 → 气刃回满+计时延长。**FUN_142125060 只判
  "规避了"这半, 完全不碰回砍**; 第 2 半 (回砍命中) 在 0xDE 计数器 (idx0/idx2) +
  条件 631 层, 与动作 id 解耦。事件 8 的发送方 (仲裁上游) 仍未钉。
- **bit15 降级 (35MB 全历史日志统计)**: `bit15=1` 共 66 次 — 全部在收刀/移动动作
  (1:8×31, 3:9×16, 1:4×10, 1:55/56/0/15/451), **3:86 期间 0 次**; 全历史 counter
  window 触发统计: flash(+0x23A0) 275 / bit250 169 / 规则 66 / 旧路径 15 /
  **bit15 路径 0**。09-17 "bit15=见切成功锁存" 为误判 (被闪探测器抢先掩蔽);
  DLL 的 bit15 触发器是死代码, 备胎们才是真触发者。
- **bit15 唯一写入方** = `FUN_141F6BD80` (汇编逐字节核对): 每帧拷
  `[ *(player+0xE470)+0x170 ]` 的 bit8 → bts 0xF 进 +0x8938(状态)+0x8968(边沿)。
  全反编译仅此一处 +0x8938 的 0x8000 写点。hub 指针经用户实测**会话内稳定**
  (收刀/拔刀同值), 但 bit15 历史上在 1:8 升过 → hub bit8 的写入时机/路径未钉;
  见切叮时 hub+0x170 无写入 (hw 断点验证, DR 未被清, ANIM 时钟断点同法可断)。
- **新钩子 negrEvent (mod build)**: HkNegrEvent @ FUN_1421038B0, 签名已入
  sig_check (唯一命中), addresses.txt 键 `negrEvent`。被动日志
  `NEGREVT sel=<0x3168> latch2399=0x latch2ced=0x act=` (仅本地玩家武器, 上限 120 条,
  原调用先行)。**判决问题: 非家族动作 (如带 TIML 窗的 105) 被无效化时事件 8 到不到**
  — 到 (latch=0) → 任意招的叮+判定可在该 detour 泛化; 不到 → 需上挖仲裁。
- **实验部署 (md5 090914518f99d7acd4b6a18500eb0177)**: entry 140 已写标准窗
  (mFlag f0=2214592512=0x84000000, 清除键 f130→f45, 触发器 2048@f45 = 308 同构;
  工具自动 .bak); entry 105 保持验证配方 (b26+b24 f0–60 + 2048@f0);
  sheath-rewrite.txt = `86 0 3 0.15 0 11` (**nohitSec=0**, 注入关闭)。
  待测: E2=收刀红刃 R2 路径 TIML 窗是否给无敌 (停泊 FSM 状态下逐帧评估是否成立);
  E3=105 吃无效化时 NEGREVT 是否出现。

### 2026-10-05 (2) 防御/判定最终模型定案 (E2/E3 判决实验完成, 全链日志实锤)

- **E2 通过**: entry 140 写标准 b26 窗 (f0–45+2048@f45) 后, **收刀重写路径 (FSM 停泊在
  98/1000) 无敌真实生效** — dispatcher 层逐帧 REJECTED (3:86 期间 396 次, 怪物持续
  判定框每帧覆盖每帧被拒), 用户体感无伤+动作正常走完。**结论: TIML 窗逐帧评估不依赖
  FSM 节点进入, DLL 注入 (sheath-rewrite nohitSec) 正式可退役; 部署配置已置
  `86 0 3 0.15 0 11`**。
- **E3 定案: 非家族动作的 b26 无效化 = 查询层静默排除**。突刺 105 (同款窗) 在确认被
  击中的前提下: 零 REJECTED (dispatcher 根本没看到交互)、零 NEGREVT (handler 没被调)、
  无叮 — 与 09-18 翻滚结论同构。**叮/成功呈现对非家族动作不可泛化 (排除事件在玩家侧
  不可观测), 复用 86/101/102 家族槽位是唯一正道** — 现有 mod 架构 (刚1 骑 86、居合骑
  101/102) 即正确做法。
- **叮链全貌 (日志+caller RVA 实锤)**: 家族动作期间事件 8 每帧 tick 到武器**状态对象**
  → bVar10 门控过 (该帧确有被无效化交互) → FUN_142125060 (id==86 门) → FX 0x7D4
  (caller +0x21250c9) + 锁存。本会话 4 次叮全在 3:86; 0x7D4 每窗一次 (一次性锁存)。
- **双"武器"对象结构修正 (重大)**: 游戏有两个都叫 weapon 的对象 — ①**武器装备块**
  (*(player+0x76B0)): +0x9F8 类型/+0x2368 气刃/+0x2370 等级/+0x2CED? 所属待重标定;
  ②**武器状态对象** (0x3190, 工厂 FUN_1421039F0, vtable 0x1434D00C8, owner=玩家
  @+0xB40): 事件 8 tick 的接收者, FUN_1421038B0/FUN_142125060/FUN_142124f70 的
  param_1, +0x3168 路由在此。**笔记中 weapon+0x2399/0x23A0/0x2CED/0x3168 实际属
  状态对象**; 装备块上同偏移是别的数据 (NEGREVT 实测状态对象 +0x2399 恒 56=0x38)。
  闪探测器读装备块 +0x23A0 — 它被 iai 族动作开始参数拷贝 (FUN_14211E8A0 kind==7)
  每次起手点亮, **闪触发器 ≈ 每次见切起手误触发一次, 不是叮信号** (本轮 3 次点火
  ≈ 3 次起手, 钉死)。
- **bit15 降级补完**: 本会话 3:86 全程 bit15=0 (含真叮帧)。DLL 四个叮触发器的实际
  状态: 闪=起手误报, bit15=死代码, CondEval631=偶发, bit250=待重标定。
  **最可靠的真叮信号 = SheathDispatchHook 的 REJECTED 行** (dispatcher 层逐帧拒绝,
  只在家族动作的无敌窗出现) — 反击链触发器建议全部收敛到它。
- **工具**: negrEvent 钩子现为诊断版 (无过滤, 前 40 条全记, 带 weapon/owner/curWpn
  三指针+match), 稳定后应恢复 own 过滤 (own = weapon == *(ownerPlayer+0x76B0) 换成
  owner==lockedPlayer)。部署 md5 0e68408556a02d912512c11d8e1a9d6f。
- **用户原始问题最终回答**: ①蓝色无敌帧本体 = TIML b26+2048 窗, 纯数据、逐帧、
  任意动作可贴 (含停泊路径) — 泛化完成; ②叮/成功呈现 = 家族处理器对"dispatcher 可见
  无效化交互"的 id 门控反应, 仅 86/101/102 槽可触发 — 泛化即复用槽位。

### 2026-10-05 (3) 方案定案: 防御帧 + 临时防御值提升 (用户裁定)

- 家族叮链不可泛化已定案 → 自定义动作的安全层改用: **TIML 防御帧 (b26 无敌/b24 霸体,
  已验证任意动作可贴、含停泊路径) + 动作期间临时提升玩家防御值** (减伤续航, 非
  无敌)。防御帧侧配方/工具全部现成。
- **下一研究目标: 玩家防御值**。入口 = 伤害公式 FUN_141F66DD0 (RVA 0x1F66DD0,
  dmgCalc 钩子已在 mod build 常驻, a1=攻击者侧已验证)。要钉: ①防御力数值字段
  位置 (player 装备聚合值/playerParam); ②公式中防御项的读取点与格式 (减伤曲线);
  ③临时覆盖的注入位置 — dmgCalc detour 内改参 vs 直写字段 (前者对多人/多目标
  更稳)。可复用工具: FSDDMG 日志 + F9 struct-diff + F11。
- negrEvent 钩子保留为观测口 (诊断版无过滤, 稳定后恢复 own 过滤, own 改用
  owner==lockedPlayer 而非装备块指针)。

### 2026-10-05 (4) E4: mFlag2 b15 ≠ 弹反开关, 是事件位写入轨 (已回滚)

- **假设被否**: mFlag2 b15 单贴到非家族动作 (101 拔刀纵斩, b26 窗 f0–f120 + b15 f0–f50)
  → 防御整个失效 (必被炸飞); 对照组 102 (仅 b26) 无敌正常。b15 ≠ "弹反型开关"。
- **新解释 (强假设)**: mFlag2 与 mTrigger 同类 = **事件位写入轨** (值为位掩码)。b15
  (0x8000) → 事件 bit15 → 特殊纳刀重映射机器 (763/764, 162/163) 误判收刀态 → 输入
  重路由/招式被打断。这同时解释 bit15 历史上升全在收刀移动 (收刀系 motion 的 TIML
  带 b15 段 = 收刀态信号) — bit15 的真身 = **纳刀态信号位, TIML mFlag2 b15 所置**。
  (待验证: 采样收刀系 motion 的 mFlag2 轨道; VSEEK 若漏采 bit11=2048 是因边沿即时
  清除, mTrigger 2048 的位写入同理需重审。)
- **家族武装真身**: FUN_14211E8A0 与 FUN_1421038B0 同构 — param_2 为事件对象,
  vtable+8 取类型号 (==7 iai 族 / ==8 见切族)。泛化卡点 = 事件 7/8 的**投递端**
  只对家族动作发, 谁投递/判定条件未钉 (深挖方向: 状态对象事件流的上游 dispatch)。
- 101/102 实验改动已全部回滚 (expect-value 三连 setval, LMT 恢复原状)。
- 方案不变: 防御帧 (b26/b24, 已泛化) + 临时防御值 (FUN_141F66DD0 入口, 下阶段)。

### 2026-10-05 (5) 防御系统落地: 原生防御借用 + 血量/耐力/转身三钩子 (部署 md5 4c3c5c41)

- **用户定案**: 见切/特殊纳刀的"全角度无敌+自由转身"太强, 换成**带角度限制的防御**;
  防御的削伤缺点用"钩血量写入"抹平 (只要防御的被打中检测, 不要削伤); 玩家血量三值
  (动态上限 100/150/200 / 当前红条 / 可恢复黄条), 拦写必须三者自洽; FSM 侧后续把
  见切/特殊纳刀换回普通节点 (mod 按 id 挂窗, 不依赖特殊节点)。
- **逆向新钉 (三个研究面全清)**:
  - **血量**: vitals box `*(player+0x7630)`: **+0x60=maxHP(动态), +0x64=curHP(红条)**
    (微型 getter 族 0x141216780-0x141216A30: get cur 0x141216910 / get max 0x141216920 /
    比率 0x141216930 / set cur clamp 0x141216940 / set max 压 cur 0x141216980; ctor=100/100)。
    maxHP 重算 FUN_141f79cd0 (技能/吃饭/多人缩放 mgr+0x608>0 ×表)。
    **伤害唯一写入口 = FUN_141f518e0(player, delta)** (负扣正回; (0,1]→1.0 保底; 尾部
    `add rsp,0x40;pop rbx;ret` 无 rax): 内部读 cur→加 delta→0x141216940 写回。
    **黄条不在 box 里** = player+0xEBA8+slot*0x40 (登记: +0x30 量/+0x34 active) +
    player+0x1D78+slot*0x40 (每帧插值状态 [0]value/[1]base/[3]acc/[7]state), 由
    FUN_141f637a0 每帧把增量回灌红条 (经 FUN_141f628c0)。**消费顺序: 受伤红条全额
    先扣, 黄条只是待回填额度** → 拦掉 FUN_141f518e0 负 delta + 清两侧槽 = 完全免伤
    无幻影回血。免死 setter 0x141f72ab0/0x141f72ac0 (置 1 HP)。
  - **耐力**: box `*(player+0x7670)` 同款布局 (+0x60 max/+0x64 cur); 统一消耗/回复入口
    **FUN_141c49f10(player, delta)** (负=翻滚/常跑/防御消耗, 发 0x7de UI 事件; 尾部
    `add rsp,0x150;pop rdi;ret` 无 rax)。max-stamina 持久控制器 player+0x7638 (推测级)。
  - **朝向/坐标**: 权威朝向 = **em+0x170..17C 四元数** (Y-up Z-forward; yaw=atan2(fwd.x,
    fwd.z) 现算); 位置 = em+0x160/164/168 (怪物同布局); 修正: player+0x1510 是锁定目标
    方向向量**不是四元数**。转身系统唯一咽喉 = **FUN_141F755C0(em, yaw度, 时长, mode,
    turnKind)** (mode 0=空闲时/1=相对/2=绝对; 度; 第 5 参栈上 rsp+0x28; 尾部无 rax=void),
    每帧消费端 FUN_141f637a0 自带限速, 写 em+0xB220(请求)/0xB224(已应用)/0xB228(转身中)。
  - **原生防御 (可借用, 无需防御动作)**: 防住判定链 = 武器非远程 (9F8∉{11,12,13}) +
    **weapon+0x20F8** 防御等级经 FUN_142117110 (查表默认 2) + FUN_141e1feb0 守门 (>1) +
    FSM 条件求值器 FUN_140f0f830: 夹角 vs **武器参数块 `*(player+0x7810)` +0x940(下限)/
    +0x944(上限) 度** — **防御角度是每武器数据字段, 可直接写**。防住下游: 削伤
    (×_DAT_145012058+0x294/298, 运行时表), 后仰档 record+0x25C (0/1/3, 按 record+0x248/
    24C 阈值), 受击路由 FUN_142042140 case 0x16 → 每武器命令表 **{3,36/37/38}=
    DAMAGE_GUARD_S/M/L (14 武器全有, WP_00 表 0x142EEEF98)** — 太刀收刀被路由到 3:37
    的历史实测由此解释。攻击侧 mGuardType@攻击数据+0xD0 = ガード不能标记。
- **mod 实装 (16 钩子; 新增 turnSet/hpDelta/stamDelta, 签名全库唯一, sig_check.py
  TARGETS 已补)**:
  - 开窗 (DoActionHook 后置挂点, 改写后的 id 也认): GUARDOPEN = 锁起始 yaw (四元数现算)
    + 保存并写 weapon+0x20F8=guardLevel + 参数块 +0x940=-arcHalf/+0x944=+arcHalf;
    关窗 (动作变化/deadline 6s/武器丢失): GUARDCLOSE 全部恢复。
  - **"防住了"双信号**: ①GUARDREACT = 引擎排防御反应 {3,35..38} 被 DoActionHook 捕获
    (默认吞掉保持招式继续, swallowReaction 可关); ②GUARDARM = DmgCalcHook 里按
    attacker→player 位置角自查 (±arcHalf, 贴脸<1m 视为正面)。
  - **免伤**: GuardHpDeltaHook (FUN_141f518e0 detour): 窗口内本地玩家负 delta 且
    (react 1.2s latch ∨ armFallback 0.8s latch) → 整条跳过 + 清 4 槽黄条额度 +
    gaugeRefill 写 weapon+0x2399=1 (vanilla 见切成功锁存, 动作结束处理器自动回满气刃)
    + GUARDBLOCK 日志 (path/delta/hp)。无 latch 的窗口内扣血 = GUARDPASS 放行 (角度外
    该掉血就掉血)。
  - **耐力返还**: GuardStamDeltaHook (FUN_141c49f10 detour), 仅 react latch 门控。
  - **转身钳制**: GuardTurnSetHook (FUN_141F755C0 detour): 起始 yaw ±maxTurn 度
    (mode 2/0 绝对钳; mode 1 相对先算绝对目标再反解输入), TURNCLAMP 日志。
  - 配置 `nativePC/plugins/ValorGauge.guard.txt` (缺文件=功能关; Shift+F6 热重载):
    `guard 86,99 90 30 2 1 1` (arcHalf/maxTurn/level/stamRefund/gaugeRefill) +
    option maxWindowSec/armFallback/swallowReaction/counterOnBlock(默认 0)。
- **设计取舍**: GUARDBLOCK 默认**不**开反击窗 (counterOnBlock=0) — 86 槽位现在就是
  刚1 本体; TIML 旧 no-hit 窗 (entry140 f0-f12) 本轮未动, 与防御共存, FSM 换普通节点
  时再决定去留; armFallback 默认开 (引擎路由万一在 LS 上死掉也能保命), 局内 A/B 看
  GUARDBLOCK path=react/arm 分布后再定是否关。
- **局内验证清单 (待 user)**: ①GUARDOPEN/TURNCLAMP: 86/99 期间转身是否被限 ±30°;
  ②正面挨打: GUARDREACT(吞)+GUARDBLOCK path=react, 血条不动; ③背后挨打: GUARDPASS,
  正常掉血; ④超伤招正面: 不死; ⑤黄条: 防住后无幻影回血 (黄条额度被清); ⑥关窗恢复:
  GUARDCLOSE 后其它武器/正常防御行为无残留 (weapon+0x20F8/参数块弧已还原)。

### 2026-10-05 (6) 防御轮 2: 根因=架势持续动作 3:98 不在窗口 id 里 + VIT 追踪器 + HP 锁 (md5 d5daf4d2)

- **第一轮局内实测复盘**: 有防御音效但照样掉血, 日志零 GUARDOPEN/GUARDBLOCK — **窗口从未开过**。
  用户在特殊纳刀**架势持续动作 3:98** 里挨打 (12×`SHEATHGUARD: reaction 3:37 swallowed during
  3:98`), 而窗口 id 只有 99/86: 99 是进入动作, 进架势即变 98 → 立刻关窗。**叮 = 架势原生防御点**
  (引擎在 3:98 本来就路由 {3,35/36/37}, 不需要我们的 0x20F8 写入), 削伤照扣。
  (颜色更正 user: **绿条=当前血量, 红条=可恢复血量**; box+0x64=绿条=致死血量, 红条=待回填额度。)
- **修复**: ①窗口 id 默认 `86,98,99` (98=架势持续); ②GUARDREACT 吞让位: 先算 reassertActive/
  watchedHold, **架势里 (watchedHold/reassert) 不吞**, 反应走原 SHEATHREASSERT→SheathArmHitRule
  流程 (force 3:100 保持), 我们的 latch 仍记 → HP 免伤照常; 非 watch 的招 (86) 才吞反应保招式;
  family 扩到 {35..38}; GuardOnReactionAction 改 void (latch+log only); GuardConfig/g_guardCfg/
  g_guardActive 定义上移到 DoActionHook 前 (hook 体直接读)。
- **VITSET/VITADD 追踪器 (HkVitSet=0x1216940 / HkVitAdd=0x1216880, 签名唯一)**: 两个底层 box
  setter 全钩 (set cur min(v,max) / cur+=delta), 按 box==本地玩家 HP box(+0x7630)/耐力 box
  (+0x7670) 分类, 记 `VITSET HP prev->v caller=+0xRVA GW=?` — **谁扣血/扣多少/哪个函数扣, 一局
  定案**, 顺带验证 FUN_141f518e0 是不是唯一伤害入口 (若 chip 走别的 caller 立刻现形)。
- **HP 锁验证工具**: `ValorGauge.hp-lock.txt` = "<值>" (0=关; 已建文件=0), >0 每帧写 box
  max+cur=值。**验地址**: 填 200 → HUD 显示/保持 200 = 地址对; 锁定期挨打, VITSET/VITADD 行
  可见游戏写入与我们覆写的搏斗。Shift+F6 热重载。
- **待 user 验证**: ①hp-lock 200 → HUD 200? (地址定案) ②特殊纳刀架势挨打 → GUARDOPEN(98)+
  GUARDREACT+GUARDBLOCK path=react, 血条不动, SHEATHGUARD 流程照旧 (force 3:100 仍触发);
  ③86 正面/背面 A/B; ④VIT 追踪行里伤害的真实 caller RVA (若非 FUN_141f518e0 链, 免伤钩子
  换靶)。

### 2026-10-05 (7) 重大翻案: +0x7630 box = 锋利度, 整个"vitals"簇是磨刀系统 (md5 6bd2a516)

- **user 实测定案**: hp-lock 写 `*(player+0x7630)` box → **改到的是武器锋利度**。之前
  agent 报告的"血量 getter 链" (FUN_141f62320/141f62730/141f62330 → 0x7630 box, maxHP 重算
  FUN_141f79cd0, 喝药 FUN_141f628c0, 伤害 FUN_14121ea10, 黄条槽 +0x1D78/+0xEBA8, "cur≤max
  clamp" setter 族) **全部重新归类为锋利度系统** — 砥石="回复 case"、磨刀技能="max 重算"、
  攻击磨刀="伤害扣减"。MinerReadHealth(0x7630) 同错 (Lua 引擎布局假设, 从未局内验证)。
  旧 SheathArmorHitCheck 的"钳 1 HP 免死"实为钳锋利度。
- **动作 id 更正 (user)**: 98=纳刀起手/保持, 99=正式进入纳刀态; 当前 TIML 改造让动作停在
  98 不进 99 → 防御窗口挂 **98 才有效** (99 留在 id 表无害)。
- **mod 侧地址可插拔 (本轮)**: `ValorGauge.vitals.txt` = "hp <槽>" / "stam <槽>" (hex, 缺省
  7630/7670, Shift+F6 热重载)。**真槽位未配置前: 依赖血量地址的写路径 (黄条额度清理/
  SHEADRULE 免死钳制) 自动禁用**; 血量拦截钩子本身不受影响 (跳过整个写调用, 与地址无关)。
- **VITSET/VITADD 追踪器升级 (HkVitSet=0x1216940/HkVitAdd=0x1216880, 两个底层 box setter)**:
  分类器先按配置槽匹配 HP/STAM, 否则**自动扫 player+0x7400..0x77F0 指针槽** (步长 8) 找
  box 归属 → 日志 `VITADD ent -23.5 (100 -> 76.5) slot=+0x76XX caller=+0x...`。挨一刀即
  现形: 真血量 box 若在 1 级指针槽上, slot= 字段直接给出槽位; 不在则给 box 绝对地址
  (x64dbg 对照)。ent 配额 64 条。
- **x64dbg 找血量槽位手册 (user 执行)**: ①进游戏拔刀 (控制器锁定), ValorGauge.log 抄
  `ADDR player=0x...`; ②x64dbg attach MonsterHunterWorld.exe; ③Ctrl+G 跳 player 地址,
  在 +0x7400..+0x7800 找槽位; 或直接按值搜: 当前 HP 整数 (如 100) 以 float 在
  [player, player+0x22000] 搜 → 挨一刀变 87 → 再搜 87.0 交集 → 定位槽位/box;
  ④把槽位填进 vitals.txt (`hp 0x7XXX`) Shift+F6。
- 下一步 (user 验证链): vitals.txt 填真槽位 → hp-lock 200 验证 HUD → 复测 86/98 防御
  (GUARDREACT+GUARDBLOCK path=react, 血不动) → 看 VITADD 里真伤害 caller (确认是否
  FUN_141f518e0 链; 若 chip 走别的 caller, 免伤钩子换靶)。

### 2026-10-05 (8) HunterPie 破案轮: +0x20F8=锋利度铁证 + HPWATCH 周期日志 (md5 271c2c99)

- **用户提供 research/HunterPie 源码** (`HunterPie.Integrations/Datasources/MonsterHunterWorld/`),
  三记连环翻案:
  1. **MHWSharpnessStructure: weapon+0x20F8 = 锋利度(打击数 int), +0x20FC = 锋利度颜色** —
     上一轮 agent 的"防御性能指数"判定错误; GuardOpenWindow 已**移除 +0x20F8 写入** (那是在
     开窗期间把刀打成红锋利度!)。原生防御借用的"等级"前提作废: 3:98 的防御反应是架势原生
     防御点, 与 0x20F8 无关。
  2. **MHWHudStructure** (从 EQUIPMENT_ADDRESS=base+0x50139A0 链读, PLAYER_BASIC_INFORMATION_
     OFFSETS=[0x50,0x7630,0x0]): **MaxHealth@+0x60 / Health@+0x64 / Stamina@+0x12C /
     MaxStamina@+0x130 / RecoverableHealth@+0x2DD4 / HealingArrayPtr@+0x30** — 即 HunterPie
     认为 *(player+0x7630) 容器本身就是 HUD 结构; 但 user hp-lock 实测同 box 改的是锋利度,
     两说矛盾 → **不猜, HPWATCH 一锤定音**。Healing 数组 = *(c+0x30)+0xEBB0, 4 项 ×0x40
     (项内 +0x10 Heal/+0x18 MaxHeal/+0x2C Stage; 0=不在恢复/1/2 两段恢复) — 上轮 agent 的
     "player+0xEBA8 黄条槽"实为该数组 (基址差一个解引用)。
  3. **链语义校准** (ReadPtrAsync: 加偏移→解引用, 末级只加): ABNORMALITY_OFFSETS=[0x50,
     0x7D20,0x0] → *(player+0x7D20)=mgr 与我们已钉事实吻合 ✓; 玩家链根 = base+0x50139A0
     (421810 build; 旧 build 0x5011760), [root+0x50]=player — 与 spiritreckoning/ValorLS
     的 +0x50 根吻合。
- **实装 (md5 271c2c99)**: ①PollHpWatch: 每 90 帧打 `HPWATCH hpBox=[max,cur] hudStam=[..]
  rec=.. heal=h/ms+.. box7670=[..] sharp=int/color cfgHp=[..]`, 上限 400 行 — **挨一刀看哪个
  数字跟着 HUD 掉 = 真血量字段**; ②GuardOpenWindow 不再写 0x20F8; ③vitals.txt 槽位机制不变
  (真槽位确认后仍用它接管)。
- **悬案**: +0x7630 box 的 HP(HunterPie) vs 锋利度(user hp-lock 实测) 二选一待 HPWATCH
  定案; player+0x7670 box (FUN_141c49f10"耐力"钩) 语义同待验 (HunterPie 耐力在 hud+0x12C,
  不是 box) — 若 7670 也不是耐力, GuardStamDeltaHook 换靶。

### 2026-10-05 (9) 血量定案 + 免伤重构到 setter (md5 120eba28)

- **HPWATCH+VIT 局内铁证**: `VITSET HP 200 -> 199 slot=+0x7630 caller=+0x1218E18` —
  **血量 = *(player+0x7630) box 定案** (+0x60=max(=200 吃满), +0x64=cur), HPWATCH 的
  hpBox 值逐 tick 跟随 HUD 掉血 (200→199→198→197 1 点/跳 = 持续伤害类)。HunterPie 的
  HUD 结构判定**正确**; user 之前"hp-lock 改到锋利度"实为**旧版开窗 +0x20F8=2 写入**的
  锋利度掉落 (时间线吻合: 不在挨打时, 在做招开窗时)。
- **真伤害写入链**: 未知函数(返回地址 RVA +0x1218E18) → **底层 setter 0x141216940** —
  **不经过 FUN_141f518e0** (旧免伤钩子拦不到的根因)。fun_141f518e0/14121ea10 链要么是
  别的伤害类要么是反编译误读, 弃用为免伤依据。
- **免伤重构**: 否决点移到 **GuardVitSetHook (HkVitSet=0x141216940)**: box==本地玩家
  hp box + value<cur + 窗口内 + (GUARDREACT latch ∨ GUARDARM latch) → **跳过原写** +
  清黄条真数组 `*(box+0x30)+0xEBB0`×4 (Heal@+0x10/Stage@+0x2C 清零, 替代错误的
  player+0x1D78/0xEBA8) + **耐力回满** (box+0x12C←box+0x130; +0x7670 box 实测恒 [0,0]
  不是耐力, HunterPie 耐力=hud+0x12C/0x130 ✓ hpwatch 实测 175↔200) + gaugeRefill
  (+0x2399) + GUARDBLOCK vitset 日志。GuardHpDeltaHook (FUN_141f518e0) 保留为次级
  (清额同真数组)。
- **开窗写入全部退役**: param 块 +0x940/+0x944 原值实测 [1200,0] — 非角度, agent 的
  "防御弧字段"也错; GUARDOPEN/GUARDCLOSE 现在零游戏内存写入, 窗口只做: 起始 yaw 锁 +
  转身钳制 (FUN_141F755C0 detour) + react/arm latch + setter 免伤。原生防御路由本来
  就不依赖我们的写入 (3:98 实证)。
- **验收 (user)**: ①纳刀架势(98)/见切(86) 正面挨打 → `GUARDOPEN` + `GUARDREACT`(架势)
  + `GUARDBLOCK vitset negated` 血条不动; ②背身挨打 → GUARDPASS/VITSET 正常掉血;
  ③锋利度不再异常掉; ④大伤害招防住 → 无红条残留 (heal 数组同步清)。

### 2026-10-05 (10) 时序翻案: 引擎先扣削伤后发反应 → 回滚式免伤 + 致死击穿保护 (md5 e5d9affd)

- **局内实测**: 正面防住仍掉血。日志时序: `VITSET HP 200->179 (GW=1)` 在前,
  `GUARDREACT 3:37` 在后 — **引擎的削伤扣减先于防御反应动作入队**, react latch 追不上
  同一击; 第二组 `44.7 -> -76.3` 直接致死 (死亡置 0 由 caller=+0x121EDFD 写)。
  另: GUARDED 命中**不走 FUN_141f66dd0** (整场零 GUARDARM), arm 兜底在防住路径无信号源。
- **修复 (回滚式免伤)**: GuardVitSetHook 三分支 — ①react/arm latch 已置 → 直接否决;
  ②无 latch 但 value<1.0 (致死击穿) → 直接否决 (path=lethal, "超高伤害不能死"的硬保证,
  不依赖时序); ③其余削伤放行但记 `g_guardRestoreHp=prev` (600ms 窗) →
  **GuardOnReactionAction 收到反应动作瞬间把 cur 回滚到挨打前值** + 清黄条数组 +
  耐力回满 + gaugeRefill, 日志 `GUARDBLOCK restore a -> b`。视觉 = 血条闪一帧回满。
- GuardApplyBlockBenefits 提取共用 (否决/回滚两路同享: 黄条清+耐力满+气刃锁存+可选反击窗)。
- **待 user 验收**: ①架势/86 正面防住 → 血条闪一下即回 (restore 行), 大伤害致死击穿
  (lethal 行); ②背身照常掉血 (restore 只在 react 后触发, 无 react 的背身伤不会回滚 —
  这是判定"防没防住"的引擎裁决, 不是我们自己的角度); ③连续多击看回滚值是否合理
  (snapshot 只记最近一次 prev, 多击间隔内第二击会覆盖)。

### 2026-10-05 (11) 防御可用定版 + 误免伤收紧 (commit 8f4cfb0 + md5 f764da17)

- **user 定性: 防御路线可行**, 大多数情况无伤; 先提交 (8f4cfb0) 后继续调。
- **bug: 不在使用勇气纳刀时被打飞却保持血量** — 两个误免伤通道:
  ①**react latch 预否决**: 引擎先扣血后发反应, latch(1.2s) 永远罩不住自己那一击, 只会
  把"挡住之后 1.2s 内的后续挨打"全部白免 → **撤销 react 预否决** (回滚机制已覆盖防住
  那一击, latch 只保留给耐力返还/reactId 记录);
  ②**armFallback 窗口内误罩**: 98 窗口 6s, 窗口内任何正面伤 (含非防御点帧) 走
  DmgCalc→arm→免伤 → **arm+致死保护限定 armIds** (新配置 `armIds <ids>`, 空=全部;
  部署默认 `armIds 86` — 86 无原生防御点, 方向免疫是设计; 98/99 只靠原生 GP+回滚,
  非防御点帧伤害照常成立)。GuardCurrentActionInArmIds() 以 DoAction 跟踪的当前动作
  实时判定, setter (arm/lethal 两分支) 与 NotifyGuardDamageHit (武装时) 双重门控。
- 部署 guard.txt 重写 (含 armIds 86 + 注释), md5 f764da17。
- **已知残留**: ①回滚快照单槽 (多击间隔内后击覆盖前击 prev, 极速连击第二击可能只回滚
  到第一击后值); ②GUARDREACT 吞反应在非 sheath-watch 的招 (86) 上默认开启 — 若想看
  原生防御表现可 swallowReaction 0; ③GP 防御 crush (大伤害破防) 若引擎仍排 35-38 会
  回滚 — 观察后再定。

### 2026-10-06 (12) 86 真身纠正 + 时间窗绑状态机 (md5 e201dcc1)

- **user 纠正 86 真身**: 当前刚1 = 见切改的, **FSM 绑定无敌帧** (TIML 修改无效) —
  天生全角度无敌, 这就是开题抱怨的"太无敌"本体; 防御实验一直是在 **98** 上做的
  (86 上从未见过 GUARDARM 的根因: 无敌帧在上游拒掉命中, 伤害计算不跑)。86 的
  防御化等 FSM/普通节点迁移; armIds=86 目前是占位。
- **user 状态机模型 (时间窗的裁定)**: ①3:98 中被打飞 → 状态必然离开
  {98,35,36,37,100} 家族 (可检测); ②削伤与防御生效间隔 ≈1 帧 ("绝对达不到1.2s");
  ③98 防住 → 必转 3:35/36/37 (可检测)。**1.2s/600ms 的来历**: 逆向说伤害经事件位
  队列每帧结算, 我给的大余量 — 实测 (VITSET 伤害与 GUARDREACT 同帧相邻) 证明不需要,
  大窗口只制造了误免伤。
- **实装**: react/restore/arm 三个 latch 统一为 **blockWindowMs (默认 150ms,
  option blockWindowMs 可调 30-1000)**; 部署 ids 扩为 **86,98,99,100** (100 = 架势
  家族状态, user 列举的家族成员)。
- **验收语义 (user 模型)**: 98 中 GP 防住 → 转反应动作 → 回滚生效; 98 中被打飞 →
  状态离开家族 → 无回滚, 伤害成立; 86 → FSM 无敌帧原样, 与本系统基本无关。

### 2026-10-06 (13) 3:100 真身: mod 自己的防住追击, 不进防御窗口 (config-only)

- **user 纠正**: 3:100 是我们代码写的 — **98 中检测到防住 (引擎排 35/36/37) → mod 吞反应
  + 强制切 3:100** (既有 SHEATHGUARD/SHEATHREASSERT 流程, force 目标)。即 user 视角的
  "防住可检测" = 吞反应切 100 这个完整链条; 100 是防住后的追击招, 不是引擎架势家族态。
- 上一轮把 100 误加进窗口 ids 已回退 (部署 ids = 86,98,99): 100 期间无防御窗口, 追击招
  受击正常掉血。想让追击也带防御 → guard.txt 里手动加 100 即可 (Shift+F6)。
- 98 防住一击的完整事件链 (全可检测): 削伤 VITSET 落地 → 引擎排队 35/36/37 →
  DoActionHook: GUARDREACT latch+回滚 (GUARDBLOCK restore) + sheath 流程吞反应 + force
  3:100 → 玩家进入 100 (窗口关闭, 100 内正常受击)。

### 2026-10-06 (14) 耐力控制模块: drain/cost/emptyExit (md5 33a384f2)

- **user 设计要求**: 一般防御掉体力但我们的防御不掉(现状, 暂留); 勇气太刀招式**应该**
  耗体力 — 见切(86)=耗体招(一次性), 特殊纳刀(98)过程**持续耗**, 耗尽强制进收刀态+
  疲劳(机制后做, 先验证可控性)。
- **真耐力定案**: hp box+0x12C(cur)/+0x130(max) — HPWATCH 实测跟踪 HUD (跑步 175↔200)。
  引擎每帧更新器 FUN_141C4A260 经 vtable+0xC68(耗)/0xC70(回) 虚调用到写入点(静态止步,
  控制不需要钩它 — 直读写字段, gauge-lock 同款)。疲劳锚点备用: player+0x208EC(阈值),
  +0x20914(力竭 flag)。+0x7670 box 恒 [0,0] 非耐力。
- **实装 (ValorGauge.stamina.txt, 缺文件=关, Shift+F6 热重载)**:
  `drain <ids> <perSec>` 持续扣(实时 dt) / `cost <ids> <amount>` 接受一次性扣
  (DoActionHook post-accept) / `emptyExit <ids> <toSet> <toId>` 归零时写缓冲请求槽
  (controller+0xBC/0xC0, spiritreckoning 同款原语) 强制切招(一次性 latch, 动作变更重置)。
  部署默认: drain 98@10/s, cost 86-20, emptyExit 98→1:0(收刀 locomotion, 待 user 调)。
  日志 STAMCOST/STAMDRAIN(整数变化跳变记)/STAMEXIT, HPWATCH hudStam 字段可对照 HUD。
- **待验证**: ①86 出招 → STAMCOST 掉体力; ②98 架势 → STAMDRAIN 持续掉(注意引擎自然
  回复与之对抗, 速率可调); ③耗尽 → STAMEXIT 是否真把玩家切到收刀态(缓冲槽在架势动作
  可派生帧被消费?) — 若槽不被消费, 换 force-DoAction 路径; ④疲劳状态(+0x20914)后接。

### 2026-10-06 (15) 耐力轮 2: 86 原生耗体(代码写死) + 绝对值压制 drain (md5 3732023e)

- **user 实测**: ①见切消耗 ≠20 — **86 自带代码写死的体力消耗** (删光 TIML 照样掉),
  我们的 cost 20 是叠加 → 部署配置撤下 cost 86 (控制原生值需先钉 +0x12C 真写入者;
  VIT 追踪器看不见 — 不走 box setter; 引擎更新器经 vtable+0xC68/C70 虚调用)。
  x64dbg 路线: HW 断点 WRITE [box+0x12C] (box=[player+0x7630]) → 出 86 抓 caller RVA。
  ②98 只在开头掉一点 — 增量式扣跑不赢引擎每帧回填 → **drain 改绝对值压制**: 进招记
  快照, 每帧拉到 max(0, 快照-速率×elapsed), 只降不升, 引擎回填即被压回 (帧内锯齿可忍)。
- **力竭 vs 疲劳 (user)**: 两个不同状态 — 力竭疑似"力尽倒下"(类死亡), 疲劳 = 停下喘气。
  机制阶段分开处理, 锚点 +0x208EC/+0x20914 归属待定。
- 部署 stamina.txt: drain 98@10/s(绝对压制) + emptyExit 98→1:0 + cost 86 注释掉。

### 2026-10-06 (16) 再纳刀直接空 bug + 两段退出 (md5 ff49abb7)

- **bug 修复 (user: "后续我再按纳刀,体力条直接空了")**: 切到收刀态 (set 1) 后轮询在
  set!=3 提前 return, 绝对压制快照永不重置 — 再进 98 时 elapsed=巨值 → target=0 →
  秒空。修复: **StaminaOnActionAccepted 在任意动作接受时无条件 disarm 快照**, 重进招
  从当前体力重新起算。
- **两段退出 (user 设计)**: **1:324 = 喘粗气态**(user 提供); 耐力耗尽 → 先排收刀
  (phase1) → 轮询看到 phase1 动作成为当前 → 马上排 1:324 (phase2) → 到达后 stage 清零。
  `emptyExit <ids> <toSet> <toId> [thenSet thenId]` (后两参可选); 部署
  `emptyExit 98 1 0 1 324`。stage 机跨动作接受存活, 3s 超时兜底, phase2 未配置则
  单段结束。PollStaminaControl 去掉 set!=3 提前 return (exit stage 机要跟踪 set 1)。
- **user 语义记录**: 力竭(力尽倒下, 类死亡) ≠ 疲劳(1:324 喘粗气) — 两个状态。

### 2026-10-06 (17) 动作衔接转向角定案: TIML PlMotionCommon.mAttackAdjustAngleIndex (user A/B 实测)

- **每个动作的 TIML `nTimelineParam::PlMotionCommon` 轨道里的 `mAttackAdjustAngleIndex`
  (int 键, 动作开始生效) = 该招出招/被衔接进入时的转向修正角档位**。user 局内 A/B 实测
  (live w03_00.lmt entry 105 突刺): **档 0 ≈ 3-5°(几乎不转), 档 4 ≈ 40°(明显吸向目标)**。
  作用点定案: **影响"上一招 → 本招"的进入衔接**(本招变 current 时 f0 键生效), 不影响
  出招衔接(下一招自己带自己的索引)。
- **vanilla 太刀索引分布** (w03_00.lmt dump): 101 拔刀纵斩/102 踏步斩/105 突刺 = 0;
  104 气刃斩1/108 气刃3/110 气刃2连/170 気刃突刺 = 1; 109 大回旋/140 见切 = 4;
  103/306 居合系**无此键** = 不修正。同轨道配套字段: mAttackAdjustGroundSpeedRate
  (修正期间地面位移速率)、mTurnSpeed2(动作中转身速率)、mMoveBankType、mLookAt 族
  (timl.md "方向范围"老记录即此套)。
- **执行链 (静态新钉)**: 属性绑定 PlMotionCommon 控制器 +0x110 (注册类型 6, 注册表
  FUN_14191F7D0 @ decompile 行 ~5175993); begin 处理器取玩家朝向四元数 (em+0x170..17C)
  与锁定目标方向 (player+0x1510) 夹角 (FUN_142178230, 弧度 ×57.2958@0x142F16EC0 转度)
  → 转身唯一咽喉 FUN_141F755C0(player, yaw度, 时长=param表+0x8AC, mode=2 绝对,
  turnKind=2)。setter 本体无角度钳制 (只写 em+0xB220 请求槽, mode 0=空闲才转/1=相对/
  2=绝对), 每帧消费端 FUN_141f637a0 限速应用。**角度表本体 VA 未钉** (消费方走运行时
  指针路径, 与防御池桥同款静态盲区; 需要时 x64dbg 对 ctrl+0x110 读断点)。
- **见切/居合例外 (user 判断+反编译佐证)**: FSM 特殊节点, 见切 begin 处理器
  (FUN_1417FDAE0 尾部) 硬编码"朝锁定目标绝对转身", 与索引机制并存/或旁路 — 特殊节点
  转向不归索引管, 别拿它们做本机制的实验对象。
- **修改配方**: `python patch_timl_defense.py setval <w03_00.lmt> <entry> PlMotionCommon
  mAttackAdjustAngleIndex 0 <档位> [期望旧值]` — 档位枚举未全表 (已知 0/1/4), 想要
  自定义中间角度需钉表本体或走 GuardTurnSetHook (FUN_141F755C0 detour, mod 已有,
  TURNCLAMP 钳制已验证) mod 侧任意角。改完需重载任务/换区 (TIML 属性绑定发生在加载)。
- 实验改动已回退: entry 105 索引恢复 0 (部署 md5 e8d4fd1a6143d0491555cd8c8ac429b1)。

### 2026-10-06 (18) 一文字长版合体动作 divide_full: 3衔接+小跑+斩击段 headless 拼接 (工作副本 MHW-Valor-divide-full.blend)

- **目标**: 省 FSM 节点 — 长一文字整链 (rigit_up→right_down→left_up→divide_long) 合成**单动作**,
  divide_long 可退役。用户给的链式语义: 动画 A 结束于 (x,y,z), 动画 B 局部坐标 (a,b,c) →
  B 在游戏中播放于 (x+a,y+b,z+c)。
- **结构实测 (_probe_align/2/_probe_handles, headless 全数值)**:
  - divide_long(0..126) = 前奏24帧(小跑+突进, 根位移 0→-4.12 过冲→-3.86 回收) + 斩击段;
    divide_short(0..114) = 前奏12帧(小步, 0→-0.68) + 斩击段。
  - **short[18..114] ≡ long[30..126] 逐帧位差 0 (27839 采样 max 0)** — 斩击段是同一份数据;
    short[12..17] 是修剪过渡带 (帧12 MhBone_012 差 0.375 rad), short[0..11] ≈ long[0..11]
    (两次烘焙, 非拷贝)。
  - 源曲线形态: 每 2 帧一键、99% LINEAR 插值; 边界键齐全性决定补键需求。
- **构建 (_build_divide_full.py, 幂等, 工作副本)**: divide_full(0..165, frameCount 165) =
  rigit_up[0..12]@0 + right_down[0..12]@13 + left_up[0..12]@26 + **long[0..29]@39(小跑+突进)** +
  **short[18..114]@69(斩击段)**。骨骼内容 == divide_long 逐帧原样 (long 段 merged 39+f 对齐,
  斩击段 bit 同源), 接缝 = 原版相邻帧, 无 pop。
- **根位移锚定**: 每段平移 offset = 上一段末帧世界位置 − 该段首帧局部位置 (切段 loc(start)≠0,
  直接套游戏链式公式会在接缝瞬移; whole-action loc(0)≈0 时两者等价)。锚定后 4 个接缝
  位移步进全 0.00000; 整条世界路径连续, 终点 -3.74m (原版长版 -3.86m, 差异 = short 斩击段
  位置曲线自身漂移 -0.23 vs long -0.34, 11cm 级)。
- **坑(本轮踩的)**: ① 补边界键时 offset 别在预计算和 co 赋值处**加两次** (run 段末帧 29 无键
  → 补键 → 双倍 offset → 接缝 7.3cm 瞬移; 三衔接段末帧 12 本来有键所以没暴露);
  ② 拷贝键手柄类型改 **FREE** 冻结坐标 — AUTO_CLAMPED 在 update() 时按新邻居重算,
  段边界处邻居变了会微改样条 (0.02 级); LINEAR 键手柄无效, 冻结无副作用;
  ③ FCurve.evaluate() 返回单 float, 四元数要 4 个 array_index 各自 evaluate。
- **验证全绿**: 全帧 0..165 × 全通道 48804 采样 0 违例 (>1e-4); 030/047 常量逐字一致;
  `_verify_exporter_divide.py`: valid=True, transforms=82, rootTracks=2, frameCount=165,
  警告码 {7:7, 80:80, 6:6, 2:2, 44:44} 与 5 个源动作**逐项相同** (已局内验证批次同签名 = 背景噪声);
  `_preflight_tree.py`: 153 actions, 0 problems; 端到端 `_repro_wiring.py` ENTRY=103 →
  正式导出 OK, 产物 entry103 tracks=82 frameCount=165 rootTracks=[(-1,4),(-1,3)],
  030/047 keyless 常量 bufType1, 000-loc bufType4 (与已验证 divide_short→103 同形态)。
  产物存证 `_divide_full_export_test.lmt` (可直接改名 w03_00.lmt 试玩 entry103, 或按 FSM
  规划接别的 entry 重导)。
- **MASTER 未动**; divide_full 只存在于工作副本 MHW-Valor-divide-full.blend (GUI 若开着旧
  会话需 File>Revert, 坑⑭)。接线命令: `ENTRY=<n> ACTION=divide_full OUT=... blender -b
  MHW-Valor-divide-full.blend --python mhgu/_repro_wiring.py`。

### 2026-10-06 (19) 衔接+小跑合体三连: *_before_divide_long (同工作副本, _build_transitions_long.py)

- 新增 3 动作 (frameCount 42): `rigit_up_before_divide_long` / `right_down_before_divide_long` /
  `left_up_before_divide_long` = 各衔接原 0..12 帧 + `divide_long[0..29]` 小跑段 @13..42
  (小跑锚定到衔接末帧位置, 接缝位移步进 0.000000)。之后接斩击段由 FSM 决定 (斩击段
  已 bit 同源于 divide_short[18..114] / divide_long[30..126], 见 (18))。
- **衔接末帧姿态与 divide_long 帧0 逐位一致 (三对 seam-pose mean=max=0.00000)** — 同一烘焙
  族切分, 任何衔接→小跑都无姿态 pop。
- 验证: 每动作 12470 全帧采样 0 违例; exporter valid=True 82轨+双Root+frameCount=42,
  警告码与已验证批次逐项相同; 全树预检 0 problems; 端到端导出 OK (left_up 变体 → entry103
  产物 tracks=82 frameCount=42 形态同前)。产物存证 `_trans_long_export_test.lmt`。

### 2026-10-06 divide_slash 接入 entry 339: 左手修正 + [1,5] 攻击窗 + 强化射击帧摘除 (已部署)

- **divide_slash (100f, 用户手搓) 已按 10_apply_rules_divide.py 规则处理并部署**:
  732→284 fcurves, tether/frameCount=100/starType 补齐, 030/047 = 106 左手常量
  (030.scaleY=2 / 047.scaleY=1), 000 只留 location, 291 条打标。二进制回读验证
  (KBRAction 反导入): BoneFunction.030 scale=(1,2,1) / 047=(1,1,1) ✓。
- **规则脚本两处加固** (10_apply_rules_divide.py): ① min-2-keys 末帧单键改向前补
  f0-1 (向后 +1 会顶爆 frameCount, verify 拦下 — valor_sheath 会话同款坑); ② 0 键
  空曲线删除 + verify 防护 — **divide_slash 的 MhBone_251/253 共 14 条 fcurve 在用户
  ARP 烘焙里就是空的** (divide_long 同名骨是全程烘满的), 空曲线让导出器 bounds()
  抛 `Vector(): sequence size is 0` 全树导出崩; 删除=缺轨走游戏默认 (与已局内验证的
  MhBone_252 缺旋转向例同机制); 若局内脚部 IK 异常 = 源动作缺这些通道的烘焙。
- **entry 339 TIML**: blend 侧本就带 10-02 时代的射击摘除 (点火对/slinger/claw/
  camera/EG09 全零) + 旧一文字短窗 [17,22]; 本次 apply_valor_attack_config.py
  PLAN[339] 改 ([(1,6)], 5) → **EC v2 [1,5] + CTO mFlag [1,5]**, 盘上验证 ✓。
  EC 路径新增 `exact` reframe 分支 (非零键数==窗口数时原地改帧, 旧逻辑撞
  assert len(nz)>=2*windows)。**CTO 保持部署现实 = 单配置 node 63** (59/72 双配置
  配方从未落进这条部署线, 部署/导出/回读三处一致); 若局内 [1,5] 中段出现伤害空洞
  (node 63 疑似反击态门控的老问题), 下一步才是换 59/72 重克隆。输入窗维持
  [17..26) buffer→open (101f 动作内有效; 手感不对再调 INPUT_PLAN)。
- **部署链**: 全树导出 (mhgu/_export_tree.py, 新工具 = 纯导出不改接线,
  补 _repro_wiring.py 会顺手重接线的坑) → 部署 → 打标。导出 vs 部署逐 entry 对照
  (_diff_entries.py): 43 个 entry 字节层差异全部是 addcto-EOF 追加 vs 连续重排的
  布局假阳性, 解码级 (轨道+键值) 逐 entry diff 仅 339 frameCount 99→100。
  部署 md5 68a89b8e0000d226e02be51b7f1994d8; 备份链: blend bak-20261006-190258
  (raw)/190449, lmt bak-20261006-pre-divideslash (旧 99f)/192331-valorcfg。
- **338-346 强化射击族**: 用户确认整族无用可改; 目前仅 339(divide_slash)/
  340(divide_long, 窗 [29,34] 维持) 接了自定义动作, 其余槽保持原版, 接线时照
  339 配方 (摘除已在脚本里, PLAN 加窗口即可)。divide_slash_long/short_left_up/
  right_down 6 个变体动作 (38f/26f) 仍是 raw ARP 态, 接线前需跑
  RULE_ACTIONS=... 10_apply_rules_divide.py。
- **GUI 开着 MHW-Valor.blend 的话必须先 File>Revert** (坑⑭, 本次无头保存过两轮)。

- **(2026-10-06 晚续) divide_slash "加速/播完还在动/乱飘" 根因 = 源动作半倍化 (user 确认误操作)**:
  身体全部骨骼+000 重心键只到 f50(间距1), 只有根位移被拉到 f100(间距2) — 局内身体
  0.83s 定格、根位移继续推满 1.67s = 加速感+定格滑行。修复 = `_x2_divide_slash_body.py`
  身体侧 fcurve 时间 ×2(handle 同步缩放, object 层不动), 尾部 >100 键删除 + 单键向后补
  (000 X/Z 的 min2@51 键 ×2 后落 102), 幂等守卫(body 已到 100 则跳过)。部署后 124 条
  通道铺到 f100(修复前 92 条止步 f52)。**攻击窗随加倍平移: 设计帧 1-5 → TIML [2,10]**
  (PLAN 339=[(2,11)])。部署 md5 d9642f43ebacbf7332fa471d39ebed9f; 备份链新增 blend
  bak-20261006-200137-prebodyx2 / lmt bak-20261006-prebodyx2 + 200259-valorcfg。
  **GUI 导出报 bounds() 空序列 = 旧会话内存里还有 251/253 空曲线** (修复在无头侧落盘,
  会话没 Revert 过) — File>Revert 后即消失; 无头 x2 已代替 GUI 里未保存的手工 x2,
  旧 GUI 会话勿再保存覆盖。导出器警告形态变化备忘: body 铺满后 MISSING_LAST_FRAME
  72→6, 出现 MISSING_FIRST_FRAME 72(身体键从 f1 起烘, ×2 后首键 f2, Fix 级背景噪声)。

### 2026-10-07 divide_valor_sheath 拼接: 纯复制定案 (user 指令, 今后所有拼接默认如此)

- **任务**: `plw_LongSword_100_111_Armature_remap.short` 第 9 帧起尾插进用户自制
  `divide_valor_sheath` (0..33, 310 曲线, m_mesh Armature)。用户手贴会粘到错误骨骼,
  要求脚本做。**定案 (user 指令): 拼接 = 纯复制** — 骨骼曲线 + 物体级曲线 (世界坐标)
  原样平移拷贝 (shift=24, 源 f9..f58 → f33..f82), 不做根运动焊接、不加数值偏移、
  不动重心保持; 接缝世界坐标由用户手工对齐。第一版的 obj 位移焊接 (+0.37X) +
  000 X/Z 尾部保持 (-0.125/-0.23) 游戏内 = 持续左飘 + IK"脱绑"观感 (身体被推离已
  踩定的脚), 已整体回退为纯复制。
- **接缝帧确定法**: 比较 dst 末帧姿态 vs 源 (cut-1)/cut 两帧的姿态差 (共享通道 L2,
  四元数 2*acos(|dot|)), 取小者。本例 d9 << d8 (多数骨骼小一个量级) → dst@33 本就是
  按源 f9 对齐的 → 源 f9 落 f33 (SEAM_GAP=0)。撞帧处理: 旋转类保留 dst 键 (差值一帧
  过渡); 世界坐标类用源值覆盖 (纯复制语义)。用户 0..33 段自带的 000 X/Z 摆动未动,
  若游戏内收刀段仍漂, 下一个对齐点就是它。
- **LIMITS 坑**: 两边动作的 fcurve 都挂 LIMITS (FreeKinetics bufType/boneFunction
  标记, 同时钳数值) → **fc.evaluate() 是钳过的值, 偏移/对齐计算必须读原始 keyframe
  co.y** (COG 偏移曾因此算错)。另: 边界键生成路径曾把 offset 加了两次 (插入时一次、
  shift 循环又一次), 恰好被撞帧去重/保持键替换完全掩盖未成害 — 生成键的代码要单测。
- **常量源曲线 (单键@f0) 要核实不要跳过**: 251/253 IK 两边都是同一组恒等常量
  (loc 0 / quat 单位, dst 外推保持值一致 → 无断); 252 旋转两边都无; 247-250/252
  位置全程动画, 尾部 49 键到 f82。IK 数据完整 — "IK 不绑"观感 = 身体滑移, 非数据缺失。
- **拼接后导出适配 (必做三件)**: ① `freehk.frameCount` 手改 (33→83;
  A_LMT_FRAME_COUNT_LOW = 它过期); ② 6_tag recipe 重打 (本例 303 条与用户原标
  **零差异** = 幂等确认); ③ 不在 recipe 的通道删除 (454 条新曲线全部出局: 身体骨骼
  location/scale + 070-085 物理孪生 — 原生布局不跟踪, 游戏物理驱动; 删后
  transforms=81 与基线一致)。
- **预检**: `mhgu/_verify_exporter_divide.py -- divide_valor_sheath` → valid=True,
  transforms=81 rootTracks=2; 噪声族 {UNEXPORTABLE 7, NONKEY 43, BASIS 2} = 基线同款;
  UNSYNCHRONIZED/MISSING_LAST 非致命 — 导出器 completeRawKeyframes 按 timingSet
  evaluate 重建缺键 (fix 模式), 短曲线尾部常量外推即正确值。
- **Blender 坑**: `keyframe_points.remove()` 循环删崩 ("Keyframe not in F-Curve") —
  一律 clear()+add() 整表重建。
- **审计**: 0..82, 310 曲线 (拼后 764 − 删 454), 键 39799 = 4352 + 35447 逐条吻合;
  物体位移 f33 = (-0.2424,-0.9429) = 源 f9 原值 (留给用户对齐), f82 = 源 f58。
- **工具**: `mhgu/_splice_divide_valor_sheath.py` (主拼接), `_splice_plain_copy.py`
  (纯复制重做), `_splice_finish_export.py` (frameCount+recipe 打标), `_splice_seam_check*.py`
  (接缝测量); 备份 `MHW-Valor.blend.bak-20261007-020924`。
  **splice_actions.py 惯例更正**: 其 docstring "B 物体位移按 A 末端偏移"的焊接种
  只适用于循环类衔接 (A 末姿态 ≈ B 首姿态); 跨动作拼接一律纯复制。

### 2026-10-07 (2) 伤害路由全面审计: CTO→col ATK 表 dump 工具 + 当前部署态 + 突刺 MV 实测 (user-verified)

- **背景**: 用户交换强化射击↔气刃斩2 的 TIML 后伤害跟着换 = 10-02/03 "CTO=伤害窗"
  定案的预测行为 —— 伤害**数值**在 col ATK 行, TIML 只带伤害窗 (CTO mFlag) +
  路由指针 (ノードID→col 名字表 unk4→(bank, ATK行)); 换 TIML 换的是引用不是数值。
- **新工具 `_cto_report.py`** (repo 根, 裸二进制解析无需 Blender): 部署 vs 原版
  LMT 逐 entry 对比 CTO 的 ノードID[0] + mFlag 窗。哈希事实: CTO type hash=
  956269290 (`nTimelineParam::CollisionTimelineObject`), 变换 ノードID[0]=
  2996226662 / mFlag=150806694 (FHK TIMLDatatypes.txt 存十进制无符号)。
  **坑**: `nWwiseTimeline::EventCollision00` 名字同样含 "Collision", 过滤必须
  按类型全名精确匹配, 否则全表误报。
- **部署态审计 (2026-10-07, 游戏 w03_00.lmt)**: entry **106 = 整套刚1 TIML**
  (CTO node 63 窗 [43,58], EC ReqNo C=10 全套, 142f); entry **140 = 强化射击族
  TIML** (点火对 mTrigger f34=1024+f36=1 / mSlingerShellAttach / claw /
  EventGroup09, **无 CTO**, 187f); entry **346 = 原版106 的 CTO 原样** (node 54
  気刃斬り２, [33,41], 121f, 还残留射击族 mTrigger f0=2048 无敌门键);
  141/142 = CTO node 56 (気刃斬り１行) [47,57]; 143 = 双节点 CTO (84@f0→101@f32)
  窗 [13,19]+[33,37]; 339/340/343 = node 63, 窗 [17,23]/[29,35]/[31,40]+[58,71]
  (10-03 树侧配方, 各比当日记录长 1 帧)。**若刚1 仍走 entry 140, 它当前无数据侧
  伤害窗** (140 装的是射击 TIML); 刚1 配方现居 106。
- **当前伤害表** (`col_research/_atk_table.py` = CTO 路由→ATK 数值一键出表):
  突き row10 MV12 / 気刃斬り１ row15 MV14·CondMV26 / 気刃斬り２ row17 MV30 /
  気刃3 三段 14/19/34 / 大回旋 row19 MV38 / 見切り攻撃 row25 MV12·CondMV26
  (106刚1 + 339/340/343 全借此行) / divide141/142 借気刃斬り１行 / divide143
  两窗借居合斬り1/2 行 / 居合斬り1/2 rows43/44 MV18/13 / 居合気刃1
  無/白/黄/赤 rows46-49 MV17/19/31/39。**强化射击 9 连定案 (user 2026-10-07)**:
  気刃スリ族 = FSM 节点 66-74 (気刃１/２/３スリ F/L/R), 动作 id 104-112 ↔ LMT
  entry **338-346**; 原版 9 个**全部无刀身伤害** (338-346 无 CTO; col 名字表
  61 条无任何射撃条目; 伤害全在投石投射物, 弹药数据不在 wp03.col; 实验旁证:
  341 整套 TIML 复制到突刺只有音效震动)。区间内原版唯一带 CTO 的是 335/336/337
  (張り付き claw 攻击, 不是射撃)。部署现状 9 槽里 4 个有刀身伤害: 339/340/343
  (10-03 树侧 CTO, 借見切り攻撃行 MV12) + 346 (用户换入気刃斬り2 行 MV30);
  **338/341/342/344/345 仍无刀身伤害**, 补法 = CTO 三件套 (_add_tree_cto_1003.py
  PLAN 加行或 GUI 复制)。已核 per-slot 锚点: 105→340(一文字长)/106→339(一文字短)/
  108→343(无双斩) —— **非纯线性序**, 其余槽位一一对应未逐个局内核对。
- **公式定位定案**: ATK 行 `Motion_Value` = 伤害公式的**动作值乘区**
  (MV/100 × 总攻击力 × 斬味倍率 × 会心 × 部位吸收(怪侧) × 愤怒/任务补正),
  非固定伤害也非最终值。同行其余字段: Fixed_Damage 全 0 未用; Part_Modifier
  恒 1.0 (部位吸收实际在怪侧数据; **大居合四行 = 0 例外, 语义未钉**);
  Elem/Status_Modifier 恒 1.0 (属伤不在此行); Stun/Exhaust/Mount/Soften 全 0;
  HitLag(秒, 已实测) / Energy_Gen(命中炼气增量) = 附带字段非伤害乘区。
  **Cond_MV = 条件动作值替换** (気刃斬り1 14→26 / 見切り攻撃 12→26 /
  気刃踏み込み 18→28), 大概率气刃等级加成 (居合系白/黄/赤直接分行同机制),
  触发条件位未钉 —— **気刃斬り1 实际最高 MV=26, 不是 14**。
- **MV 实测闭环 (user-verified 2026-10-07)**: 突刺 row10 MV 12→100 →
  局内伤害 ~8.3 倍正确。**col 覆盖首例部署**:
  `nativePC/hm/wp/wp03/collision/wp03.col` (此前游戏目录无 col 覆盖, 读包内
  原版)。**用户指令: chunk-extracted = readonly 基线永不改, col 编辑一律放
  游戏目录副本** (基线 md5 d8cb2d4f)。col 编辑流程: 只做等长数值编辑 (长度
  20408 不变) + 改后结构校验 (61 names/13 banks/59 ATK 全解析 —— 3 字节插入
  崩游戏前科, 每次改完必验)。

### 2026-10-07 (3) 强化射击逐招专属伤害行: 空名字条目 + 孤儿 ATK 行配方 (343 无双斩 user-verified)

- **零重建器配方闭环 (user-verified)**: 无双斩 (FSM 108 / LMT 343) 两段伤害窗
  [31,40]+[58,71] 从借見切り攻撃行 (node63/MV12) 改为专属行 MV100, 三步:
  ① col 名字表空条目 **names[43] (node 85)** 原地填 `cgp=8/move=22` (只改
  19 字节定长尾部, 文件长度 20408 不变); ② 孤儿 **ATK 行 22** (Type0/MV1 占位)
  → `Type=1 / MV=100 / HitLag_a=b=0.10` (Cond_MV/MindEye/EGen 全 0);
  ③ 部署 LMT 343 CTO ノードID `63→85` 单键 4 字节原地补丁。局内两段全打
  MV100; 几何 cgp=bank8 与原借行同档 = 范围手感不变、变量只有伤害的干净 A/B;
  刚1/一文字 (仍借 row25) 零波及; 全表 CTO 回归仅 343 一行变化。
- **空名字条目 (关键发现)**: 名字表 61 条里有 **6 个游戏从未使用的空条目** —
  node **85/90/94/98/99/100** (name 空, cgp=-1, move=-1)。填它们 = 等长编辑
  零结构改动; **名字字符串必须保持空** —— 写名字会改变条目长度 → 后续条目
  全部平移 = 结构手术; 游戏按 unk4 (node id) 路由, name 只是标签可空。
- **孤儿 ATK 行**: 名字表只引用 59 行中的 52 行 → 孤儿行 **22/31/42/45/50/54/58**
  无任何条目指向, 征用 = 等长编辑。22/42/45/54/58 = Type0/MV1 占位行 (需
  Type→1 + 字段重设); 31/50 = 现成 Type1 (遗留 MindEye=1, 行 31 另有
  Cond_MV=48 —— 用前清零防条件替换)。
- **6 槽计划定稿**: 每个强化射击槽 = 1 个空名字条目 + 1 个孤儿行, MV/HitLag/
  EGen 逐招独立且互不影响原版。343=node85→row22 已占用; 剩 5 槽用
  node 90/94/98/99/100 (正好够)。第 7 行才需要追加条目 (届时才写重建器)。
- **工具**: `_patch_cto_node.py <lmt> <entry> <newNode> [oldNode]` — CTO
  ノードID 单键补丁 (键 20B: 值 u32@+0 / 帧 f32@+12; assert 恰好命中 + 长度
  不变; 对 .bak 副本双向验证: 63→85 成功、旧值不存在正确拒绝)。
  `col_research/_atk_table.py` 改读游戏目录实况 col (chunk-extracted 基线
  只读, md5 d8cb2d4f)。
- **部署态 (2026-10-07)**: col 覆盖 = 突刺 row10 MV100 (测试) + row22 MV100
  (无双斩) + names[43] 已填; LMT 343 CTO→node85。备份 `w03_00.lmt.bak-
  20261007-pre343mv` / `wp03.col.bak-20261007-pre343mv` (游戏目录)。
  **树侧待办**: blend 里 343 CTO action 的ノードID 键仍是 63 —— 下次 blend
  导出会冲回, 导出前在树侧改 85 (或届时 6 槽 PLAN 一起重打)。

### 2026-10-08 LMT 槽位约束破案: entry 345/344 拒绝一切非原生导入内容 (user 实测闭环) + 加载器/游走器/校验器反编译

- **现象与定案 (全部冷启动 user 实测)**: 强化射击 9 连尾部槽位 112/110 换上任何
  非原生导入内容必挂——192_remap@345 = 播放即崩 (AV 恒定 0x2233DE0/DF0, 有时
  先卡黑屏), divide_short@345 = **连存档都闪退** (加载期初始化), chunk-G0 裸
  原版字节@345 也崩; 而 **原生导入重编码随意换** (345←native-332 实测稳, 344
  同理); 对照组 entry 346 (action 111) **随便改都不崩**。104-109 槽 (338-343)
  也宽容 (divide 系长期在用)。**原因未明**——class 归属、animLen、TIML 内容、
  easing 字节、col、mbd、mod DLL 全部排除后仍成立, 结论 = 引擎里有按 entry
  绑定的静态约束, 345/344 被锁死在"原生导入血统"上。
- **操作纪律 (血泪教训)**: ① 改 LMT 必须**完全冷启动**才生效 (LMT 启动时读;
  一次 mid-session 补丁 2 分钟后就测, 结论被旧数据污染整晚); ② 二进制手术
  (splice+append) 结构本身会引入第二层崩溃——内容与 0.0.1 全等的手术文件
  依然崩, **凡动内容一律走树导出**; ③ 正向二分 (从已知好的 0.0.1 逐步加变更)
  优于反向二分; ④ 每轮只动一个变量。
- **MHW-Valor.blend 已持久化状态 (2026-10-08)**: 345 ← `LMT::w03_00::332` +
  TIML ← 344 同款节点 (animLength=133, 332 动作 134f 的合法配对——**TIML
  animLength > frameCount = 进档闪退级硬伤**, A3 实测), 343 CTO 树侧键
  63→85 已改 (**"导出前改 85"待办销账, 树导出原生携带, 部署侧补丁不再需要**)。
  备份链: `225042-prefwd-persist` / `233844-pre345timl` / `223314-pre345test`。
  部署 LMT = 0.0.1 原文件 + 343 CTO node85 补丁 (md5 27fa9926), user 确认可玩。
- **class 表 (0x142EF5C30, 113 行, stride 0x20, classPtr@+8) 全名单已枚举**:
  真·气刃/强化射撃类在低 id——0=cKijinSlash3, 1=cKijinStepSlash, 10=cKijinSlash2,
  43/44/45=cKijin1ComboShotBase/F/R, 46/47/48=cKijin2ComboShotBase/F/L,
  49/98/100=cKijin3ComboShotBase/L/R, 65=cKijinStab, 66=cKijinUpper,
  67=cJumpSlashLanding, 75=cFinishAttack, 110=cWpOn(拔刀), 111=cWalkWpOn(拔刀走),
  **112=cWalkWpOff(收刀走)**。用户实机观察: 110/111/112 = 気刃3 衔接的三方向
  强化射撃 (右转/前向30°/?)——**class 名归属与实机观察的矛盾未解** (名字按
  vtable 区间归属有漂移风险, RTTI 被剥; 实机行为是 ground truth, 345/344 严格、
  346/104-109 宽容为实测事实)。110 实测同样拒绝自定义内容 (user 补测)。
- **LMT 加载器反编译 (dump.c 行 7025621 起)**: 签名 "LMT\0"+ver95+piVar6[2]==
  _DAT_1435454ac 校验; **原地重定位**: entry offset、track bufferOffset(+0x10)、
  lerpOffset(+40, 仅类型∉{0,1,2,3,6,8,9})、timlOffset(+0x58) 全部 +=文件基址;
  **头部无需连续** (逐 entry 经偏移表独立重定位——多槽 splice 后头部散落不致崩,
  但 splice 结构本身另有问题, 见上); Flags2(@+67) bit0(→flags u32@+64 的
  0x1000000)=跳过轨道重定位, bit7(0x8000000)=跳过 TIML 处理。
- **TIML 校验器 FUN_14231f730 / 重定位器 FUN_14231f7e0**: 每个 type 行的 hash
  (@typesOffset+i*0x18+0x10) 必须在全局注册表 (_DAT_1451c5c20/_DAT_1451c5c28)
  里, 且行内 **u0 (@+0x14) 必须等于注册表项 +0xC** —— 任一不中 = **整个 LMT
  加载失败**。TIML 变长记录格式: 头 40B + types 行 24B + transform 行 24B +
  键 20B, 指针字段 = 每行首 u64。
- **键游走器/插值器 FUN_142233e90 / FUN_142234010**: 按 **bufferType 步长表**
  (0x143505C40 起 stride 0x18: {size, timeGetter, extractor}) 逐键游走, 前进方向
  终止条件 = **delta==0 哨兵键** (全部文件已验证在场), 后退以数组基址为界;
  size 表与 FreeKinetics 完全一致 (bt3=16/bt4=8/bt5=4/bt6=8/bt7=4/bt11-13=4/
  bt14=6/bt15=5, bt1/2/10=0 keyless); bt3 timeGetter 读 u32@[rcx+12], **bt4 读
  u16@[rcx+6] (=0x2233DE0), bt5 读 u8@[rcx+3] (=0x2233DF0)——当日全部 8+ 次崩溃
  恒定落点 = 游走指针出界后的 getter 访问, 卡黑屏与闪退是同一游走失控的两种
  结局** (我方文件哨兵/步长全部干净, 触发点在别处未钉)。bt1/2 哑 getter =
  0x14035BE60 (xorps xmm0; ret 恒 0), keyless 轨游走立即终止, 安全。
- **mbd (w03_00.mbd, 16,499B) 部分解码**: magic 18091001, "Default" 链名 +
  "w03_00" 文件名 + 变长记录 {asciiz 名 ("w03_00"/"co00_01"...), u32, u32,
  float, u32, i32, u32-link} = IK/链图; **截断覆盖实验 = 行为零变化 → 游戏对
  该文件不敏感, 非槽位约束源**。
- **动作库对比 (0.0.1 vs 当前 MHW-Valor, 逐 entry 字节级 + 动作库 hash 级)**:
  463/464 entry 内容一致 (345 除外), **全部已接线动作内容零差异** ——工程文件
  没坏, 10-06/07 的"改动"实际只是 6 个槽的接线挪动 (192_remap 140→106+345,
  valor_sheath 306→342, 原生动作互相换位)。非接线差异仅 3 项: CTO.083 (63→85,
  本会话), FK_grip_preview_auto (grip-sync 插件预览, 无害), divide_valor_sheath
  (新增未接线, grip-sync 源)。
- **工具沉淀**: `_bytediff_good_bad.py` (逐 entry 归一化字节 diff), `_deep_diff.py`
  (轨道头/basis/lerp/TIML 值级 diff), `_delta_check.py` (delta 语义全轨校验),
  `_fwd_stepA*.py` (正向二分脚本族, 0.0.1 基底无头改接线+导出不存盘),
  `_dump_action_inventory.py` (动作库清单+内容指纹 JSON),
  `Reverse/xref_to_rva.py` (rel32 xref 扫描)。
- **未决**: 345/344 严格性的机制本体 (per-entry 静态数据在引擎何处、校什么);
  342 上 valor_sheath 81 轨残次导出的重做; 346/110 的自定义容忍度补测;
  DLL (ValorGauge/LuaEngine) 摘除状态待恢复验证。

### 2026-10-08 接线+节点属性整体恢复 0.0.1, 导出物 md5 级等同闭环 (user 实测定案)

- **背景**: user 判定当前树接线混乱, 要求把 0.0.1 (10-06 00:47) 的接线恢复进最新 MHW-Valor.blend。上一条 (35f49d4) "改动只是 6 个槽的接线挪动" 的诊断只对了一半 —— **导出对比证明 per-entry 节点属性也被实验一起改掉了**, 只恢复连线导出物仍差 41B。
- **恢复分两层**: ① 接线: 8 entry (106/140/306/342/343/344/345/346) 的 LMT 动作线 + 7 entry 的 TIML data 组 (343 仅 LMT 线, 其 TIML 组两边同名同内容; E433/435/437 本来就无 TIML 数据线, 非损伤)。② 节点属性: **TIMLDataNode**(unkn1/unkn2/animLength/loopStartPoint/loopControl → 导出 TIML 块头 dataIx0/1/animLen/loopStart/loopCtrl, freeHKDataNodes.py basicStructure) + **LMTEntryNode**(loopFrame/byteflag/byteflag2 → entry 头 loop_frame/byte_flag0/1, flagMake bit7=首元素)。症状样本: E106 animLen 被设 142 (见切值), E342 带着 valor_sheath 的 214/loop50/ctrl1, E140/E346 byte_flag0/1 互换, E306/E342 loopFrame 50↔-1 镜像。
- **定案 (user): 动作数据 + TIML 数据相同 ⇒ 导出 LMT 逐字节相同、md5 一致** — 恢复完成后 MHW-Valor.blend 与 0.0.1 各跑正式导出器 (exportNodeToFile, GUI 同码路径) 产物 **md5 相同 (e8d4fd1a, 4,011,220B, 464 entry)**。"导出物 md5 对比"从此可当树状态一致性校验 (比逐 entry diff 便宜); 差异溯源路径: `_bytediff_lmt.py` 把差字节归 entry 区域再结构化 diff TIML, 41B 级小差异基本全是块头字段/键值, 动作 fcurve 本体零差异。
- **192_remap (刚1) 曾被孤儿清理删除** (当前文件与 blend1 均无, 接线摘除后成的孤儿): 从最新幸存备份 `bak-20261007-233844-pre345timl` append 回 (313 fcurves 与 0.0.1 同版) + fake_user 防再清。**append 依赖追踪会连带拖入同库动作** (本次 9 个带 fake_user 的 .001/.002 重复, 零接线无害留守)。注意 blend1 是清理后状态, 找动作用带日期的 .bak。
- **E343 CTO 键 85→63 (user 批准回退, "不知道改了啥了")**: 该键不在节点属性上, 在 E343 接线的 `CollisionTimelineObject.083` 动作 fcurve `FreeHKTiml.mB296CA66[0]` @f0 的键值里 (10-07/08 树侧已把它 63→85, = f2a4e70 那条"树侧待办"实际已被做完)。**已部署文件未动, node85 MV100 补丁仍在生效; 但下次从本树导出并部署 w03_00.lmt 会把 343 打回 node63 路由 (10-03 借見切り攻撃行配方伤害)** — 要回 85: 树侧把该键改回 85 再导出, 或对部署文件重跑 `_patch_cto_node.py`。
- **工具沉淀 (valor-remap-pipeline/mhgu/)**: `_dump_wiring.py` (树接线 dump), `_extract_wiring_json.py` + `_restore_wiring_from_json.py` (0.0.1 接线快照 → 带全表校验恢复; 缺动作数据块即中止不保存; 按快照节点名+动作名双重校验复用旧节点), `_restore_nodeprops.py` (节点属性提取/恢复, EXTRACT/SAVE 环境变量门, 含 VERIFY), `_revert_343_cto_key.py`, `_bytediff_lmt.py` (差字节归属 entry + TIML 结构化 diff), `_probe_192remap.py` / `_probe_timl_sets.py` / `_append_192remap.py`。0.0.1 基准快照: 根目录 `_wiring_001.json` / `_nodeprops_001.json`, 基准导出物 `_wiring_001_export.lmt` (md5 e8d4fd1a); 改前全状态备份 `MHW-Valor.blend.bak-20261008-wiring-restore` (实验接线+实验属性)。
- **GUI 提醒**: 无头改完 blend, 开着的会话必须 File>Revert (坑⑭)。

### 2026-10-09 调试工具升级: Ghidra MCP + x64dbg MCP + 键鼠注入 (强化射击双悬案的前置)

- **动机**: 气刃3强化射击崩溃 / 删342强化射击后局内仍能强化射击 两悬案久攻不下 (纯静态推断+文件A/B无法定位), user 拍板先升级工具链: 逆向=Ghidra MCP, 调试=x64dbg MCP, 输入=Agent 直接注入键鼠操作游戏 (截图看画面)。
- **ZCode MCP 配置位置坑: `.mcp.json` 是 Claude-Code 惯例, ZCode 不读**。实际位置: 工作区 `<repo>/.zcode/config.json` → `mcp.servers` (用户级 `~/.zcode/cli/config.json`)。两 server 已写入 `.zcode/config.json` (`.mcp.json` 同步一份备用)。**MCP 在会话启动时连接, 新配置要重开会话才生效**。
- **x64dbg MCP (duty1g/x64dbg-mcp-server v1.3)**: 原生插件 (Zig), dp64/dp32 已在 `tools/x64dbg/release/x64|x32/plugins/`; 随 x64dbg 启动自动开 HTTP (127.0.0.1:9094), Bearer token 在 x64dbg.exe 旁 `mcp_config.json` (换 token 两处要同步); 84 工具 (断点/单步/内存/调用栈/FindPattern/ExecuteDebuggerCommand 等)。已实测: 启动 x64dbg → initialize 握手 OK。源码 `tools/x64dbg-mcp-server/` (SKILL.md = 工具文档)。
- **Ghidra MCP (LaurieWired/GhidraMCP 1.4)**: 官方仅支持 ≤11.3.2 (12.1.x 支持卡在 PR), user 定案装 11.3.2 → `tools/ghidra_11.3.2_PUBLIC` (本机 Java 26 可启动, analyzeHeadless 验过)。插件 = 解压即装: **Windows 用户设置目录 = `%APPDATA%\ghidra\ghidra_<ver>\`, 不是 `~/.ghidra`** (Unix 惯例; 首装就栽在这 —— 装错处 = 插件在 Configure 里永远不出现, 且设置目录里连 application.log 都不会产生) → `%APPDATA%\ghidra\ghidra_11.3.2_PUBLIC\Extensions\GhidraMCP\ (**必须扁平, 不带 `Ghidra` 子目录** —— 多一层永不被扫描, 手动平铺解压+重启实测可用; 官方路径 = Install Extensions 对话框右上 `+` 从 zip 添加 → 勾选 → OK → 重启)`。**启用: 插件分类 = `Analysis` 不是 Miscellaneous** (在 Misc 里翻不到就是这原因); 扩展装好后首次开 CodeBrowser 弹 `New Plugins Found!` → Yes → 勾 GhidraMCPPlugin → OK。**8080 只在 CodeBrowser 工具开着时存活**。2026-10-09 闭环实装: 8080 监听 + mcp 实调 get_current_address 真实响应。副产物: 误勾 GnuDisassembler 被装上 (unbuilt 纯源码包, 无害可留)。bridge = `tools/ghidra-mcp/bridge_mcp_ghidra.py` (stdio MCP ↔ 插件 http://127.0.0.1:8080; 27 工具 decompile/rename/xref...), 依赖 `requests` + **`mcp<2`** (mcp 2.x 把 FastMCP 改名 MCPServer, bridge 直接 ModuleNotFoundError, 必须 pin)。下载走代理 `localhost:1080` (curl -x)。
- **重大代价**: `Reverse/MHW-reverse.gpr` 是 12.1.3 分析的工程, **Ghidra 不支持旧版开新版程序库** → 11.3.2 里要对 `MonsterHunterWorld_dump.exe` 重新 import+auto-analysis (可 analyzeHeadless 后台跑); 12.1.3 保留当 GUI 备用 (用户目录按版本独立, 可共存)。Reverse/*.py 直接读 dump exe/.c 文件, 不受影响。
- **键鼠注入 `tools/game-control/gameio.py`**: 扫描码级 SendInput (游戏读原始扫描码, 虚拟键窗口消息无效; 与 ValorGauge counter-repeat 同通道, user 已证可达 MHW) + PIL ImageGrab 截屏 + GetForegroundWindow 焦点查询。CLI: `tap/hold/release/type/mmove/mabs/mclick/mdown/mup/shot/focus/pos`。桌面实测全通: 鼠标滑动+nvim 打字+截屏读回。坑: ① 打字前等目标窗口完全起来 (nvim 启动吞了前 6 个键); ② 注入走前台焦点, 打游戏前确认游戏窗口在前台 (gameio.py focus); ③ 独占全屏截屏可能全黑 → 游戏改无边框窗口; ④ git-bash 里 `(notepad &)` 的 notepad wrapper 坏的, 用 `cmd //c start <app>`。


- **computer-use 操控 Ghidra/Swing 经验**: Swing 无 Java Access Bridge 时 a11y 树只剩标题栏按钮, 全靠截图+坐标点击; 小复选框必须先 zoom 精确定位 (差几像素就点空/点错行, 本次两次点到邻行); zoom 只裁最新一张 raster, 连续 zoom 前必须重新 screenshot; 列表行点击=选中, 复选框要点方框本体; Swing 文件选择器可点 File name 输入框直接敲完整路径+回车; 对话框弹出会使 app 级捕获失效 (invalid_target), 重新 observe 或全屏 screenshot 即可。

- **11.3.2 工程重建完成 (headless 导入 90MB dump)**: `analyzeHeadless.bat <project_dir> <project_name> -import <exe> -log <log>`, git-bash 里 **Windows 路径必须单引号** (裸反斜杠被 bash 吃掉变 `MHW-artsReverse`); headless 进程名是 `java.exe` 不是 `javaw` (grep 关键字写错差点误判分析进程死亡); 90MB MonsterHunterWorld_dump.exe 全量 auto-analysis 28 分钟 (20:14→20:43), `Analysis succeeded` + `Save succeeded` 后程序落在工程根。GUI 侧 computer-use 全程代操作: Tip of the Day 顺手关掉开机提示, 双击程序即开 CodeBrowser → 8080 恢复 → MCP `search_functions_by_name('14118c320')` 返回 `FUN_14118c320` 与笔记地址吻合, 数据链路实锤。屏幕自动锁定会让截屏管线超时 (capture timeout = 先怀疑锁屏)。headless 与 GUI 抢工程锁, 导入前必须完全退出 GUI (taskkill 后还要删残留 .lock/.lock~, 否则 headless 卡锁)。
### 2026-10-09 强化射击(気刃スリ族)专档立档: SPIRIT_SHOT_Research.md

- 强化射击 9 招的全部结论/修正史/验证方法已迁入 **`SPIRIT_SHOT_Research.md`**(根目录),
  本文件此后只记增量。核心定案: 动作id→LMT 非线性映射表(104→338/105→340/106→339/
  107→341/108→343/109→342, 以 DLL MOTIONTRACE 为准); 射击=类代码×TIML 按类门控——
  104/109 类读 PMC(mTrigger 为首要信号轨,338 已零化待测), 105 类不生成投射物,
  **108 类 TIML 免疫**(343 無双斬零化 PMC 照射, 只能换 ARP 来源动画或 DLL 拦);
  EventGroup09 只是射击族签名/音效轨不是投射物开关; 刀身伤害走 CTO 借行, 与去射击互不干扰。
- 方法论沉淀: TIML 二分法(删整条→零化单键, 一轮一测) + 结构化对比(parse_timl 字典级,
  字节 diff 差异簇=±尺寸增量即纯指针平移) + MOTIONTRACE 运行时映射, 三件套已两次定位成功。

### 2026-10-10 LS 防御(格挡) TIML 配方定案: PMC mFlag 单键 0x90080000, 无 mTrigger (user-verified "终于有了")

- **配方**: PlMotionCommon mFlag = 单键 f0=0x90080000 (b31+b28+b19), **不设 mTrigger**, 整动作常驻。
  bit28=block-valid(11f1c38 全武器扫描+GS/306 双重印证); b19 为 GS 独有贡献位(所有 LS 尝试中从未出现过)。
- **依据链**: user 删大剑 w00_00 entry6 的 PMC(PlMotionCommon.250)→大剑防御失效 ⇒ 防御由当前动作的 PMC 携带;
  原版 GS6 PMC = 单键 0x90080000 无 trigger(chunk-extracted/chunk/hm/wp/wp00/mot/w00_00/w00_00.lmt entry6) = 参考实现;
  306 已验证格挡 0x12400006(b28+b22+b25+6) 与 GS 的公共核 = b28。
- **弯路目录(勿重蹈)**: ① b26 单独=无声无敌帧; ② b24 单独=无呈现; ③ b22+b25+低16_6+2048 = 突刺**原生普通标志**
  (vanilla 105/112 自带, 不是防御配方——被当"112 配方"连抄四轮); ④ 105.txt 配方(0x87400006@f0 18f, 10-01 验证过)
  10-10 复测失败(当日 DLL 状态差异嫌疑); ⑤ 完整 306 TIML + 192_remap 迁移动作 = 失败(动作侧嫌疑未排)。
- **两个坑**: ① **mTrigger 2048 = 无敌帧 no-hit 门**, 命中在仲裁上游被排除 → 格挡永远到不了呈现层
  (GS 无 trigger; LS 每轮都带着它); ② **ValorGauge.dll 只对 LS 激活**, 其 clang-latch/SHEATHREASSERT(吞 3:37)
  会污染 LS 防御测试 —— GS 删除实验是干净环境(武器门关), **LS 测试必须无 DLL 对照**。
  (plugins 里 ValorGauge.dll=10-04 生效中, "ValorGauge.dl l"=10-06 带空格已失效, 勿混淆。)
- **现状**: 0.0.2 树 105+345 均已移植(345=PMC.348 新建 mFlag 轨, 保留 snowshovel/flag4 辅助轨);
  部署备份 w03_00.lmt.bak-20261010-2041-pre-guard-port; 干净树实验版=_blend_export_mhw_origin_105b24.lmt 系。
- **user-verified 收官(10-10 晚)**: 345(192_remap **迁移动作**)**开着 DLL** 局内出防御 —— 迁移动作嫌疑与
  mod latch 嫌疑双双排除, 配方跨动作/跨武器通用; **一武器可多防御招并存**(突刺105/收刀见切/刚1-345)。
  105 突刺作为实验载具已**回归原版 PMC**(mFlag 三键 0x02400006/0x02000006/6 + mTrigger 2048@f0), 防御只保留在 345。
  最终部署 = _blend_export_final.lmt, 备份 w03_00.lmt.bak-20261010-2100-final; 测试期 12 个备份 blend 已清。
- **窗口化已验证(user 10-10 "搞定")**: off-key 拆窗生效, 345 = `mFlag f0=0x90080000 → f12=0`(0.2s 窗, 恢复无标志态),
  **带角度防御的时长可任意调节**(挪 off-key 即可); 部署备份 w03_00.lmt.bak-20261010-2112-pre-345-win12。
- **待办**: 收刀防御(收刀态见切 rewrite)与本法互不干扰性未测。

### 2026-10-11 勇气纳刀(3:98) WASD 转身角度乱 = GUARD 转身钳制误伤 (user 排除法定案, 已修)

- **症状/定位(user 排除法)**: 改名 DLL 不加载 → 勇气纳刀 WASD 转身正常; 装回 → 按方向键角度乱。
  日志实锤: `GUARDOPEN act=3:98` 后 `TURNCLAMP yaw 180 -> 103.572 (mode 0, start 73.5721, maxTurn 30)`
  — 游戏的转身请求被钳回"进架势瞬间朝向 ±maxTurn"。
- **机理**: guard.txt `guard 86,98,99 ...` 使 98 是 watched id → DoActionHook GuardOnActionAccepted(98)
  开窗(捕捉 startYaw, maxWindowSec 6s, 动作切换才关) → GuardTurnSetHook 门条件只有 `g_guardActive`,
  整个 3:98 架势期间钳制全程生效。该钳制是 98 还是"特殊纳刀防御测试架势"年代的设计("架势中不能乱转"),
  98 变成勇气纳刀持刀态后与原版 WASD 转向直接冲突。GUARD 其它通道(方向角兜底/血量回滚/吞反应)不动角度。
- **修复(部署 md5 975fcdd7)**: GuardConfig 新增 `turnIds`(默认 {86}) — GuardTurnSetHook 先读实时
  controller+0xAC/+0xB0(= player+0x6274/6278), 当前动作 ∉ turnIds 或 set≠3 → 原样放行不钳。
  guard.txt 新配置行 `turnIds <ids逗号分隔>`(缺省=86; `turnIds 0`/解析失败 = 全局关钳; Shift+F6 热重载生效)。
  86 短促无转向, 整窗钳制保留不动; 98/99 持刀架势转身恢复原版。部署 guard.txt 已加 turnIds 86 行+注释。
  注意: GUARDOPEN/GUARDCLOSE(3:98) 日志行照旧出(防御窗口仍在, 只是钳制按 id 圈定), 勿当新 bug。
  待 user 局内复测 WASD 转身。
- **user 待定项(2026-10-11)**: 勇气纳刀 360° 随便转偏宽泛(原版一般只有前/左/右转向); 等评估 mod
  实际游戏影响后再决定限转向平衡难度 — 机制已具备: guard.txt `turnIds` 加回 98 + 调 maxTurn 即可,
  Shift+F6 热重载, 无需改代码。user 已局内验证修复: WASD 转身正常。

### 2026-10-11 (2) 计数招 86 见切(无敌帧) -> 112 刚见切(防御帧) 切换 (user 决策, DLL 侧完成, 待局内验证)

- **决策**: 见切的"无敌帧判定"(entry140 b26 no-hit 窗 + bit15 链)退役, 计数招改为 **3:112**
  (LMT 345 = 192_remap 刚1, TIML 已带防御帧 PMC 0x90080000 f0->f12, user 10-10 验证过格挡成立)。
  防御帧 vs 无敌帧的优势 = **带角度**(引擎原生格挡仲裁), 更贴合 MHGU 勇气太刀。数据侧(动画/TIML/
  F6 试招)user 已全部就绪(日志: 3:112 局内可播, motion=49497=0xC159=entry345)。
- **DLL 改动(部署 md5 cc81737a, 备份 .bak-20261011-pre112)**:
  - **sheath-rewrite**: toId 默认 86 -> **112**(配置文件同改: `112 0 3 0.15 0 11`, nohit=0 —
    防御走原生格挡, 不再注入 no-hit)。收刀 R2 {3,11/12} 红刃+气刃>0.15 -> 3:112。
  - **guard.txt**: `guard 112,98,99`(+86 移出), `armIds 0`(112/98/99 全有原生防御点, 方向判定
    归引擎; **"没有"必须写 0, 留空=旧的全 id 行为**), `turnIds 112`(112=短促反击, 钳转身;
    98/99 持刀架势依旧不钳 — 10-11 事故结论不变)。吞反应/回滚削伤/耐力/气刃通道照旧
    (= 3:98 同款处理)。
  - **刚1 gauge scope**(无条件 0.15 净消耗): 武装点 DoActionHook `actionId==86` -> `==112`;
    结算触发2/3(帧56/换招)同步 112, "left86"日志改"left112"。**bit15 叮恢复触发对 112 永不
    成立**(bit15 由原版 action-86 机制置位) — 按用户"0.15 无条件"设计本就该如此, 注释已记。
  - **保留未动**: PollModTriggers 的 86/101/102 原版触发链(bit15/flash/bit250 -> 反击窗+force)。
    force-action.txt 已是 `3 112`, 所以原版见切/居合叮 -> force 出 3:112 刚1, 链路自洽;
    86 是原版看破斩, 行为回归原版(不再被 guard 借用改写)。
- **语义变化提醒**: 见切槽(收刀 R2 计数)的"成功判定"从 mod 侧 bit15 轮询改为引擎原生格挡路由
  (GUARDREACT 35-38 latch + 吞反应), 反击窗不再由叮触发(counterOnBlock 保持 0 — 112 本身
  就是反击招)。若日后要"防住追加奖励", guard.txt `option counterOnBlock 1` 现成可用。
- **防住接招 (user 定: 接 3:77, 原 86 见切链的反击招)**: sheath-counter.txt 加 `arule 112 3 77`
  (备份 .bak-20261011-pre112)。机理 = 112 防住时引擎本就会装载防御反应 3:37
  (日志 GUARDREACT latched 实锤), 112 进 sheath-counter watch 后走 SHEATHREASSERT 吞反应 +
  SheathArmHitRule -> force 3:77 (与 98 的"防住 force 3:100"同一条链); 背面被打连接由
  arule 自身路径同样进 3:77。**有意不配 bit222 规则**: 112 自带攻击帧, 事件位按动作异义,
  斩击命中可能误触发。Shift+F6 热重载即生效, 无需重编。
  空挥(窗内无攻击)不触发 -> 112 自己播完(防御窗 f0-f12 + 斩击段 f48+)。


- **MCP 开工顺序 (两个 server 都是'寄生'在宿主进程里的, 非常驻)**: ghidra 8080 只在 Ghidra GUI + CodeBrowser 工具打开时存在; x64dbg 9094 只在 x64dbg 进程活着时存在。ZCode 在**会话启动那一刻**连接, 连不上 = 该 server failed = 工具不出现。所以固定顺序 = 先开两个程序 (Ghidra 要进到 CodeBrowser 里打开过程序) → `bash tools/check_mcp.sh` 双绿 → 再开/重开会话。新会话里 MCP '都不可用' 先跑这个脚本查后端, 别怀疑配置。
### 2026-10-12 FSM→LMT 映射定案: LinkMotion_W03 通道 + 南风焓mod数据侧机制全拆 (局内验证差一步, 见文末协议)

- **现场**: nativePC 当前 = 南风焓 2020-10-14 原版发布文件 (wp03_action.fsm 40KB/59节点 md5 06702f3d;
  w03_00.lmt 3.76MB md5 1c9e8676; 均与当前原版 117KB/90节点/3.6MB 不同 = 老版本基线+他的改动)。
  `ValorLS.d ll`/`ValorGauge.d ll` 空格禁用 (纯数据侧运行), `LuaEngine.dll` 活+TCP 28991。
  工作区分析副本+两份 xfs.json dump: `nfh-analysis/`。
- **FSM→LMT 原生直连通道 = 节点 process 参数 `nPlFSM::LinkMotion_W03 { MotionNo:i32, MotionNo_Phase1:i32 }`**
  (XFS def hash 0x19b8aa01; 另一类 = `nPlFSM::ActionSet_W03 { ActionNo }` hash 0x4e5f1d2f)。
  **进 MOT 节点 → 引擎直接播 LMT[MotionNo], Phase1 接第二段** — 不经过动作层/类表/请求。
  原版 s0000-s000c + '152or132' 全是它; 原版组合衔接的真实现 = **ACT节点 --按键条件--> MOT节点(直播
  LMT entry) --无条件()--> 下一ACT节点**。实测原版: 突き(79) --△--> MOT119 --> ACT80(上捞, 80→119 ✓
  与 action-list 互证); 居合斬り(101) 八出边: R→MOT363 / R+△→MOT364 / △→MOT365 / ○→MOT366 /
  R+○→ACT86(见切) / △+○→MOT362 / △+○+→→MOT361 / △+○+←→MOT360。待機２(1012) --△--> MOT131。
  MOT 节点出边全是空条件 = 动作播完自动走。
- **ACT 节点进节点 = 发 DoAction{3, ActionNo}** (09-22 已证"node-entry beats request"), LMT 映射走
  动作层类表 0x142EF5C30→类工厂硬编码 (action-list.md), FSM 数据里不携带 LMT 号。两条通道正交。
- **运行时对象 (dump .c 实锤)**: 工厂 FUN_140b200c0 分配 0x10B: vtable@+0=0x1430B8E20,
  **MotionNo@+8 / MotionNo_Phase1@+0xC, 构造清 -1**; 反射注册 FUN_140b24cb0 (名字串 0x1430bb118/
  0x1430bb138, SUB_140240720 型 stub accessor); 静态注册 FUN_1400a3190 (类名 0x1430b84e0 +
  hash 0x19b8aa01 → DAT_144f1a188, 每武器一个 LinkMotion_Wnn)。消费方=纯虚调用, 静态不可见
  (vtable+0x288 播 motion 调用点 2113 处无法筛) — 下次会话硬件断点读 [实例+8] 一步抓到。
- **南风焓"非原生节点"真相 (用户问点的答案): 他没发明新映射, 全是复用**。59 节点全部改名"南风焓"
  (mName=作者标签, 与 09-12 结论一致); 给一批原版无节点请求 id 补 ACT 节点 (98/99/100/16/6/9/
  35-41 等, 用于勇气路由条件) + 克隆原版 s000c-363 为自己的 MOT 节点 [55] (101 --R--> MOT363)。
  条件属性全用原版名 (R/△/○/回避/納刀中/気刃斬りII/III/見切り回避成功後の攻撃ヒット...), 无私有属性
  (他的 gauge/狩技在 DLL 里, 空格禁用后数据侧剩纯路由)。
- **他的新招 = 原地替换既有 LMT entry 内容, 零新槽位**: 两边 LMT 都是 164 非空 entry; 差异 =
  21 个 entry 内容改动 (109 大回旋/140 见切/154/156/158 跳斩族/162/170 気刃突刺/174 兜割/
  308-312 特殊纳刀族/338-342,344,346 强化射击族/351) + 文件尾追加 163KB (指针落入追加区的:
  154/156/158/306 勇气纳刀/346/351, 其中 351 fcurves+TIML 整体搬家, 306 加了 EffectParameter1)。
  entry 363 (他 MOT 节点播的) 与原版逐字节相同 = 播的是原版动作。
- **我们的新招配方 (数据侧, 免 DLL 免类注册)**: FSM-Studio 原生支持 (model.ts motion kind 节点,
  inspector 编辑 MotionNo/Phase1): ① 动作写进 LMT 任一 entry (FreeKinetics 管线已有); ② FSM-Studio
  加 motion 节点 MotionNo=<entry>; ③ 源节点加链接 (如 R/自定义可用条件) → MOT 节点 → 空条件链回。
  开问题: MOT 直播时动作层 (controller set:id) 是否/如何跟随 (局内 probe_log 一看便知); 判定/COL
  路由 (EventCollision ReqNo) 是否照常 — 待局内验证。
- **LuaEngine 现场工具链 (本次跑通)**: TCP 28991 单向无回包 (LuaEngine/send_lua.py), 命令在
  **main_loop_begin 帧钩子**(游戏线程)执行, 输出用 io.open 落文件读回; TCP lua 状态跨调用持久
  (全局表就是-state)。全局表有 RunFsmAction(set,id)/RunLmtAction(0xC000|entry) 强制原语 (部署版
  DLL 比工作区源码新, 源码里没有); 引擎 Player.Action: fsm=player+0x6274/78, lmtID=anim+0xE9C4
  (=entry|0xC000), derive=player+0x6284/88, frame=anim+0x10C; player=GetAddress(0x1450139A0,{0x50})
  (ValorLS 老链 0x145073ED0 本版本已失效, 读到字符串)。首验: idle fsm=3:0 lmt=0xC001=entry1 ✓。
- **⚠️ 事故 (2026-10-12, 我造成的)**: TCP 裸调 `AddChronoscope(单参)` → handler 内 strlen(NULL) →
  main_loop_begin first-chance AV → x64dbg 放行后 second-chance → **游戏进程崩溃退出**。教训:
  TCP chunk = 帧线程, pcall 接不住硬件异常, 未知签名的 C++ 注册函数绝不裸调 (先在工作区源码/示例里
  钉签名, 或干脆只用 GetAddressData/SetAddressData 这类已验证 API)。
- **探针现状**: `nativePC/Lua/action_probe.lua` 已改为文件日志版 (fsm/lmt entry/frame/derive 变化
  → `E:/MHW-arts/nfh-analysis/probe_log.txt`; 原版备份 `nfh-analysis/action_probe.lua.bak`)。
  需聊天框 `reload action_probe` 或重启游戏生效。
- **下次会话验证协议 (10 分钟)**: ① 重启游戏进场拔刀; ② 聊天框 `reload action_probe`; ③ 打小居合
  (3:101) 后按 R → probe_log 出现 `entry=363` 且 fsm 仍是 3:101 = MOT 直播实锤; ④ 要抓消费方:
  x64dbg attach → Lua 取 player→mgr(player+0x7D20) 附近内存搜 qword 0x1430B8E20 (MOT 实例 vtable)
  → 实例+8 硬件断点(读) → 进任何带 MOT 节点的招 (突き+△ 即可) → 断点回溯即消费者; ⑤ RunLmtAction
  /RunFsmAction 语义测试 (从 1-2 个 entry 开始, 别再裸调未知函数)。

- **✅ 2026-10-12 局内实锤补完 (user 局内 + probe_log)**: 小居合(101) 按 R → probe_log 三连:
  `3:101 entry=307` → `3:16 lmt=49515 entry=363 frame=0.0` → `3:16 entry=104 frame=42.0`。
  **LinkMotion 通道闭环**: 0xC16B=0xC000|363 真出现在 anim+0xE9C4, 363 播满全长 42/42 帧后骨架才
  交给后继动作 (104=気刃斬り1 的 motion)。链条 = node50(101) --R(id8)--> node55 MOT363 --空-->
  node24 ACT=16 (DoAction{3,16}, 他的"气刃斬り1连段态", motion=entry104; 原版同位置=特抜1001,
  user 在原版 FSM 里核对一致)。新钉机制三条: ① MOT motion 播满才交还骨架; ② 后继 DoAction 在
  MOT entry 同帧就发 (fsm 提前变 3:16) — 即 MOT 节点=播motion+立即放行路由, motion 本身压住骨架
  到自然结束; ③ entry 可在动作内段内切换 (3:7 先 115 后 101)。user 连按 R 又测出气刃链
  16(104)→67(106)→68(108)。新映射数据: 67→106/85→101/81→102✓/7→115→101; **set-1 行的 lmt 字段
  是 0x6000 bank** (24752=0x60B0 → entry176 = 身体/共有 LMT), 0xC000 只是武器 bank 标签 —
  "motion id = entry|0xC000" 修正为 "entry|bank", bank 按动作集而异。nfh 条件表全用原版属性
  (練気ゲージ判定(気刃斬りII/III/大回転斬り + オーラレベル白以上 + R+↑←→↓ id28-31 等),
  见 nfh-analysis 可复跑脚本。

- **长按检测之谜解开 (2026-10-12, user 问 "他怎么做到原生长按的")**: 南风焓 node[1](ACT=98 勇气纳刀)
  出边里有一条 **`R押してない` → node[2](ACT=100 收刀)** + `スタミナ切れ → node[2]`(没力气强制收刀)。
  保持 = 按住 R 时全部出边条件不成立 → FSM 停在 node[1],306 的 loop=129 呼吸循环兜着;松开瞬间
  `R押してない` 点火 → 100 → entry 312 收刀动作。**`R押してない` 是引擎全局注册的输入属性**
  (.rdata 属性串 0x1430B9BE0, 与 レバー前キープ 0x1430B9C48/スタミナ切れ 0x1430B9E60 同块),
  输入系统每帧无条件写 (cFSMPlBase, registration accessor=SUB_140240720 stub, 09-22 已证) —
  **原版 wp03 FSM 的 29 个属性里 0 个按键状态族 = "太刀没有长按检测"只是原版 FSM 没引用**,
  他加一条引用就激活, 纯数据无 DLL。组合键限制(09-22)只管 combo→请求分类 (R+↑ 族), 单键否定态
  不是 combo 不受限。否定态全家桶 (.rdata): **△押してない / ○押してない / R押してない /
  アイテムボタンを押してない** + レバー前キープ 方向族。
  **对我们的直接收益: 勇气纳刀 195(松开收刀)/196(保持按下构) 的 DLL GetAsyncKeyState 检测可退役**
  — FSM-Studio 两节点 + 一条 `R押してない` 边 = 纯数据长按, 与 nfh 同配方。

- **受击反应路由机制 (2026-10-12 user 局内实测, probe_log 7× 复现)**: 勇气纳刀(98)中被怪打 →
  每次都是 `3:98 → 3:38(lmt=1,无bank) → 3:69(entry=109) → 3:100(312)`。**受击反应不走 FSM 条件** —
  伤害系统在受击瞬间直接发动作请求 {3,38}(零入边请求进入, 与按键条件无关); 38/39/40=受击硬直族
  (原版无节点只播硬直), 35/36/37=另一族(本次会话 3:37 出现过一次, motion=entry212, 有专属受击动画;
  他的接线 35/36/37→node46 ACT=105, 105 零出边=播完即止)。哪族被请求由伤害系统按受击时玩家状态选,
  **FSM 只能接管"反应之后"**: 他给 38/39/40 补节点 + cond id18(OR 状态组,近恒真) → 69(大回旋),
  且他改短了 entry 109 内容(159→60帧, LMT diff 清单内) = **"勇气纳刀被打中→自动大回旋反击"是故意设计**,
  69 完 → id13(アクション終了) → 100 回架势。我们的"被击→自定义动作"配方 = 同款(补受击 id 节点+边)。
- **9-22 分类器结论要修 (user 实战反馈)**: user 称左右袈裟"很容易出来"(右袈裟=推摇杆+登龙键=
  他的 id27 `R+△+レバー`→83) — 若属实则 **`R+△+レバー` 在 LS 上活** ⇒ "分类器只写原版 wp03 引用过的
  组合"过宽, 分类器静态表(113条)大于原版引用集; R+↑(09-22 实测死)与 R+△+レバー(疑似活)并存说明
  表内容无规律可猜, 只能逐个实测。注意: 本次 probe 会话 0× 3:82/3:83/3:87 — kesa 结论还差一次
  带 probe 的实测确认(做一次左右袈裟看 3:82/3:83 是否落日志)。
- **新数据**: 3:11 上下文相关选 motion — 常规=entry104, 居合系连段中出现过 entry=166
  (49318=0xC0A6); 3:37→entry212(受击动画); 3:88→entry6; 3:12→113→104 段内切换;
  set-1 还见 0x1000 bank (1:362 lmt=4104=0x1008) — bank 标签至少 0xC000(武器)/0x6000/0x1000 三种。

- **A 组受击测试第一轮结果 (2026-10-12 局内, v2/v3 探针)**: 勇气纳刀保持段吃高伤怪攻击 13+ 次,
  **HP 全程零变动(连削韧都没有)** — H1(GP窗口 20f 边界)与 H3(方向)被"无论什么时机/方向都无伤"
  排除; **"零伤吸收 → 38 族, 削韧防御 → 35-37 族"(user 假说)成为唯一存活候选**(user 的 37 经验
  来自我们 112/345 防御帧招 = 有削韧; 今晚 38 = 无伤)。因果方向未定: 无伤导致 38, 还是同一 TIML
  配方同时产生两者。**他的 306 零伤配方候选成分 (与原版 306 全量 track diff)**:
  新增 `PlMotionCommon mFlag1 f0=0x50000000 (b30+b28)`、
  **`PlMotionCommon mStaminaParamIndex f0=256`(原版 306 根本没有这条轨!)**、
  `PlMotionInput mFlag2 (2键)`; mFlag 开头加 b31+b26 GP 窗(f0-20); 删除了 PlMotionVisual 两轨 +
  EffectParameter1 WorkNo00。
  - **注意**: mStaminaParamIndex 也可能只是配置架势本身的耐力消耗(他 readme: 保持要耐力, FSM 有
    スタミナ切れ出边) — 未必是"格挡伤害转耐力"。判别: 受击瞬间盯耐力条有没有额外跳降。
  - 决定性实验: 把这套成分(mFlag1 0x50000000 + mStaminaParamIndex 256 + mFlag GP窗)烙进我们
    112/345 的 TIML → 换文件实测: 若削韧消失且反应 37→38 翻转 = 因果链闭合 + 白拿无伤防御配方。
  - 38 的请求方代码未钉: 可 x64dbg 条件断点 DoAction(0x140269CE0) cond `[rdx+4]==0x26` 抓调用栈。
  - 探针盲区修正: v3 加了 slot1 身体控制器监控(player+0x6924/6928), 普通受击硬直若走身体不再漏。
    普通招式(A3)中受击的样本仍未取得。

- **✅ A 组定案 (2026-10-12, user 混合实验收口)**: 受击反应族 = **格挡的伤害结果**, 与动作 id/时机/
  方向无关。三向对照表: ①零伤格挡(他 306 成分) → 3:38/39/40 → 他的FSM接69大回旋反击; ②削韧格挡
  (我们 0x90080000=b31+b28+b19) → 3:35/36/37 → 他的FSM接105死胡同(user 实测"被打进入105");
  ③无防御 → 身体控制器 1:339 族 + 全额伤害(武器 FSM 完全不参与, 1570行 -72 真伤样本)。
  **因果闭合证据**: 同一个他的 FSM, 换我们的 LMT → 37→105, 换他的 LMT → 38→69。
  **零伤格挡配方 = 他 306 的新增成分**: mFlag1=0x50000000(b30+b28) + mStaminaParamIndex=256 +
  mFlag 开头20帧 b31+b26(GP窗); 与我们配方唯一结构差异 b19(削韧) vs b30+staminaParam(无伤) —
  最小成分位未bisect(V1 b19→b30 / V2 +staminaParam / V3 全套), 但直接抄全套已可用。
  35-37 vs 38-40 语义 = "有削韧格挡反应" vs "无伤格挡反应"; 普通受击走身体控制器, 武器FSM看不到。
  **⚠️ LMT 换文件实验必须完整重启游戏** — 任务重进不清 LMT 缓存, 混合态会造成"防御失效"假象
  (user 差点被吓到)。

### 2026-10-12 (2) R+方向之谜: 组合键开关疑似在 PlMotionInput TIML 轨里 — 9-22 结论待翻案

- **user 决定性实验**: 他的 FSM 不动, 只换 LMT(他的↔我们的↔原版): R+方向**只在他的 LMT 下成立**
  (R+方向→3:87→entry170; 无方向→3:76→entry154); 改名排除法排除了 efx/col/wav 等全部其他文件
  → **开关就在 LMT 里**。9-22"LS 分类器不写 R+方向"的结论被此实验推翻至少一半。
- **嫌疑轨 = nTimelineParam::PlMotionInput** (动作输入耦合, 我们从没挖过):
  - entry 106(気刃斬り2 的动作): mFlag/mFlag2/mFlag4 与原版逐键相同, **唯独 mFlag1 同帧异值**:
    vanilla f44=0x0C001000/f54=0x0C003000 → 他的 f44=0xCFFC10CC/f54=0xCFFC30CC (保留原版
    b27+b26+b12, 叠加 b31+b30+b25..b22+b20+b7+b6+b3+b2; 他还删了 f24 键)。
  - entry 170(気刃突刺): 原版无 mFlag1/mFlag2 轨, 他整套新增 (mFlag2: 256/768/336/1008...)。
  - entry 154: mFlag 整条重写。他的 306 也有新增 PlMotionInput mFlag2。
- **单变量测试文件已造**: `nfh-analysis/w03_00_rdir_test.lmt` = 原版 LMT + 仅 entry106 mFlag1
  三键(f24→0, f44/f54→他的值), 其余全原版。部署(他的 FSM + 此 LMT, **完整重启游戏**)后
  気刃斬り2 中按 R2+方向: 出 3:87 = mFlag1 位即组合键开关(9-22 翻案 + 解锁配方);
  不出 = 开关在他 LMT 的其他内容里, 回头二分。
- 若成立: **方向派生/自定义组合 = 改动作 TIML 即可, 无需 DLL** — 对我们的自定义招意义重大。

- **✅ B 题定案 (2026-10-13 局内实锤)**: `w03_00_rdir_test.lmt`(原版+仅 entry106 mFlag1 三键
  f24→0/f44→0xCFFC10CC/f54→0xCFFC30CC)部署后, probe_log 三连:
  `3:16(104)→3:67(106)→3:87(170)`(R2+方向,两次)/`→3:76(154)`(无方向)。
  **PlMotionInput mFlag1 位 = 方向组合输入开关, 9-22"LS 分类器不写 R+方向"结论翻案** —
  当年失败根源是原版动作没带使能位。他的 mFlag1 值 = 原版位(27,26,12)∪新增(31,30,25,24,23,22,20,7,6,3,2)。
  待细化: 最小位集 bisect(候选 b31+b30, 试 0xCC001000 即可); 该机制是否同样门控
  レバー前キープ/回避+↑ 等其他输入属性未测。**对我们: 自定义招要方向派生, 改自己 TIML 的
  PlMotionInput mFlag1 即可**。新映射: 3:87→170✓/3:76→154/3:73→153/3:99 共享 306 的 motion
  (frame=74 中段采样)/3:6→3/3:80→119→107 段内切换。

- **C 组准备 (2026-10-13)**: ①154/156/158 的 `mFlag2=256` 原版本就有(非他加) — 他在这仨 entry 改的是
  PlMotionInput 输入位+追加新动画(=他的自定义招本体), 不是无伤成分; "y回无伤"疑似他 DLL 时代的
  伤害钩子行为, 数据侧无伤配方目前仅 306 一份。②motion 节点伤害判定测试件:
  `nfh-analysis/nfh_action_mot306.fsm` = 他的 FSM + node55(101--R-->MOT) MotionNo 363→306
  (xfs_parse+XFSWriter 重写, 回读验证 MotionNo=306, canonical 重排尺寸不变属预期)。
  部署(他的 fsm 测试件 + user 自己的 LMT, **完整重启**) → 勇气纳刀→△→R → MOT 路径播 user 的
  306(带防御帧) → 吃刀: 削韧+35-37→105 = MOT 带 TIML(新招配方可行); 全额伤害+身体硬直 =
  MOT 仅视觉, 伤害需 ACT 节点复用。攻击半边后续可再指 140/109 对怪打。

- **✅ C 组定案 (2026-10-13, user 自制实验, 比 planner 版本更彻底)**: user 新增 **motion 节点
  (id370) + 新 LMT entry 370** —— 306 数据拷入 = **防御帧生效**(受击 3:37 + -1 最小削韧,
  削韧配方在 MOT 路径完整工作); 110 数据拷入 = **攻击触发**。**LinkMotion 路径带完整 TIML
  (攻击+防御), 纯数据新招配方全线贯通**。**MOT 本质 = 往当前动作注入骨架动画, 不是动作状态**:
  fsm(动作层)停在进入前动作(如 3:16)而 0xE9C4 显示 MOT 的 entry —— 两层分离, 非 bug。
  MOT 招的四条已知代价: 无动作身份(派生/气刃表/反应上下文/规则系统都看不到)、反应按底层动作算、
  底下动作的 motion 在 MOT 播完后从 0 重启、联机同步未知(单机无影响)。工具箱定稿:
  改既有槽位(nfh式,有身份,牺牲原版招)=主力; 新 MOT 节点+新 entry(加法式,不牺牲,适合 MHGU
  迁移招, 300+ 空槽)=加法。user 修正: 无伤防御非 DLL 行为, 今日全部测试无外力(除探针)。

- **联机 MOT 同步假说 (user, 2026-10-13)**: 网络好的时候其他玩家动作衔接自然、无明显断裂 →
  游戏大概率同步的是 **fsm id + motion id**(轻量状态)+ 客户端本地模拟插值, 不传动画本身
  (卡普空动作游戏惯例)。MOT 注入会设置 motion id(0xE9C4 可见 0xC000|entry), 因此 MOT 招
  联机可见性可能比预想乐观, 待实测(进联机任务让队友看)。
- **下次任务 (user 定)**: ①无伤格挡实现 — 把 306 零伤成分(mFlag1=0x50000000 +
  mStaminaParamIndex=256 + GP窗)烙进我们自己的防御招; ②按键按住数据侧化 — 勇气纳刀
  195/196 的 DLL GetAsyncKeyState 检测退役, 换 `R押してない` 条件边(FSM-Studio 两节点)。

### 2026-10-13 (2) R+↑ 使能位写进我们自己的管线 (blend→export→验证通过)

- **FreeKineties TIML 数据模型 (逆向自 addons/MHW-FreeKinetics 源码)**: 每个参数类别 = 独立 action
  `TIML::<类别>[.NNN]`(.NNN 是 Blender 去重号, **不是 entry 号**!); entry↔TIML 映射在 nodetree
  `w03_00` (FreeHKNodeTree): LMTEntryNode.entryNum → "TIML Data" 输入 → TIMLDataNode(多输入) →
  TIMLActionNode.input_action; 类别由 action.freehk.timelineParam(Raw) 定(Raw='m6FCCD10E'=PlMotionInput)。
  通道 = fcurve data_path `FreeHKTiml.m<hash>`; **打包: LMT u32 = (idx1<<16)|idx0**(bigFlags 双半字)。
  哈希表: dump_timl_frames.py load_names() 读 FHK 的 TIMLTimelineParam/TIMLDatatypes.txt;
  PlMotionInput=0x6FCCD10E, mFlag=0x08FD20A6, mFlag1=0x6E63FBC7, mFlag2=0xF76AAA7D。
- **345 patch**: blend=MHW-Valor-0.0.2-345-valor-spirit1 (entry345 node → 动作 valor_1, 142帧,
  PlMotionInput action=`TIML::nTimelineParam::PlMotionInput.157`)。改 mFlag1 四键:
  f36 idx0 0x1000→0x10CC / idx1 0x400→0xCFFC; f56 idx0 0x3000→0x30CC / idx1 0xC00→0xCFFC
  (= NFH 使能值 0xCFFC10CC/0xCFFC30CC)。blend 已原位保存(备份 .bak-pre-rdir)。
- **无头导出模板** (照 _export_verify_1003.py): blender -b <blend> --python: 开文件 →
  scn.freehk_tether=bpy.data.objects["m_mesh Armature"](本 blend 有3骨架) → tree=w03_00 →
  LMTFileNode.filepath=OUT → importlib MHW-FreeKinetics.operators.export_ops →
  exportNodeToFile(outn, addon.preferences)。新进程跑(避 structureCache)。
- **产物已验证**: `nfh-analysis/w03_00_rdir345.lmt` entry345 PlMotionInput mFlag1 =
  f0=0/f36=0xCFFC10CC/f56=0xCFFC30CC/f110=0 ✓。待 user: 部署(替换自己 mod 的 w03_00.lmt,
  完整重启) + FSM 侧在播 345 的节点上接 R+↑ 边(09-22 的旧边若还在则免接线) → 招式中 R2+↑ 实测。
  窗口 = f36..f110(约0.6~1.8s)。最小位集 bisect 未做(先抄全套,通了再抠)。

- **✅ R+↑ user-verified 贯通 (2026-10-13 终)**: 首测"无效"根因 = **位与边不在同一动作**(边挂在
  ACT=70/67/77/72(播 entry 110/106/155/156)上,位打在 345)——**PlMotionInput 使能位只对当前
  播放动作生效**。修复: 部署 LMT 上 106/110/156 的非零 mFlag1 键全部 OR 0xC3FC00CC(106:
  f24/f44/f54/f120 → 0xC7FC10CC/0xCFFC10CC/0xCFFC30CC/0xC7FC10CC = 全动作窗口;备份
  w03_00.lmt.bak-20261013-rdir-106110156)。user 复测正确: 斬り2 中 R2+↑ → MOT 节点[81]播
  entry362(12帧带判定小招) ✓。155 无 PlMotionInput 轨需造轨(ACT=72 边暂死); 345 位保留
  (valor spirit 1 接边即用)。**配方定稿: 招式要接方向派生 = 该招 entry 的 PlMotionInput mFlag1
  非零键 OR 0xC3FC00CC(blend 侧改 fcurve 或 LMT 侧 setval) + FSM 节点接 R+方向边**。
  blend 侧注意: entry 的 PlMotionInput action 经 nodetree 查(勿信 .NNN 后缀); export 用新进程。

- **✅ PlMotionInput mFlag1 位映射破案 (2026-10-13, 全武器普查+wp05对照)**: **mFlag1 = 16 个
  2-bit 字段**(b0-b31, 三态 00=关/01=基态/11=全开), 原版所有值都是"若干字段01→激活窗翻11"。
  **f8(b17:16) = R+方向组开关**: wp05(原生R+↑←→↓,FSM各6条边,R+△/○为0)的方向招家族增量里
  唯一新字段 = f8(00→11); LS 原版 profile 无 f8 ✓。方向本身由摇杆状态决定, 无 per-direction 位。
  NFH 的 0xC3FF.../0xC3FC00CC = 大撒网(f1,3,8,9,10,11,12,15 全置11), 其中仅 f8 为方向必需(待
  单字段实验确认: 106 OR 0x00030000 即可测)。**最小配方(待验证): mFlag1 | 0x00030000 只开方向组**。
  已知字段: LS106 原版 f6/f13(01→11, vanilla组合族), wp05 family B f1/3/13, family A 全开+f8。
  entry 11(気刃斬り1) 独占 b18/b19(全LS唯一) — 语义未钉。345 已双侧还原(刚一不需要方向)。

- **✅ 强格挡位破案 (2026-10-13): PlMotionCommon mFlag b30 = 強ガード(零削韧)判定位**。wp06=盾斧
  (FSM '盾'x7 + G/強G反应节点全点名: 34=弾かれ, 35/36/37=GリアクションS/M/L=通常格挡(削韧),
  **38/39/40=強GリアクションS/M/L=强格挡(零削韧)**, 零入边引擎请求, 出边接反击(→84/83/107) —
  "强格挡接反击"是CB原生行为, nfh 的 38→69 同款)。CB 格挡动作 TIML: entry28 mFlag=0xD0000000
  (b31+b30+b28)+mFlag1=0x10000000(b28)+staminaParam=10; entry60/64/65 mFlag=0x42000226→0x42000006
  (b30+b25+b22族)。三方交集(原版CB/NFH306/零伤行为)= **b30**; 我们削韧配方 0x90080006(b31+b28+b19)
  无 b30。mStaminaParamIndex 排除为关键成分(CB 用小指数 10/11, 非NFH的256)。
  **测试件已部署**: 345 格挡窗(f0-f12) mFlag 0x90080006→0xD0080006(+b30), 同时 106/110/156 rdir
  位重打(备份 w03_00.lmt.bak-20261013-pre-b30, md5 5814bffb)。user 待测: 刚一防御窗吃刀 →
  零削韧+反应38 = b30 定案; 仍削韧+37 = b30 需配合成分(则试 +mFlag1 b28)。
  blend 侧 345 已同patch(f0 idx1 0x9008→0xD008)。**真实 345 PlMotionCommon action = `.348`**
  (dedup 名, 必须走 nodetree 定位); 其格挡配方 = 0x90080006 (b31+b28+b19+b2+b1) f0-f12。

- **✅✅ 零伤格挡配方定稿 (2026-10-13 user-verified)**: 345 格挡窗(f0-f12)三件套
  **mFlag b30 + mFlag1 b28 + mStaminaParamIndex=10**(复刻长枪 entry28 签名)→ 被击进 **3:39
  (強Gリアクション中)+ 持续掉耐力不掉血** = 长枪强格挡(Instant Guard 族)行为完整复刻。
  b30 单独不够(mFlag 加 b30 仍 37),mFlag1 b28 + staminaParam 是必要成分(或组合生效)。
  S/M/L tier 由伤害/招式决定(我们中档=39,NFH 上档=38 同族)。
  **管线教训(重要)**: FreeKinetics exporter 对新建 fcurve 通道静默丢弃 —— 新关键帧默认
  BEZIER 插值不在 interpolationMapping=['CONSTANT','LINEAR','QUAD','CUBIC','QUART','EXPO','SINE']
  里, exception 被吞。**往 TIML action 加通道必须设 kf.interpolation='CONSTANT'**。
  ExportTIMLTypesNode 可直接进程内调用调试(ErrorHandler(prefs,prefs))。
  昨日遗留清单更新: 无伤格挡 ✅(今日); 按住检测数据侧化(R押してない)待做;
  反应族对照表: 34 弾かれ / 35-37 GリアクションS/M/L(削韧) / 38-40 強G S/M/L(零伤+耐力)。

- **✅✅ 35-37 vs 38-40 族区分定稿 (2026-10-14 user 双向验证)**: 同一个格挡核上, 决定家族的只有两个位:
  **PlMotionCommon mFlag b30 + PlMotionCommon mFlag1 b28 = 強G 开关**(两处都在 → 3:38/39/40 强格挡,
  零伤 + 掉耐力;任缺/全无 → 3:35/36/37 通常格挡, 削韧 + 耐力不动)。实证链: 345 加三件套→39;
  删 staminaParam(b30+mFlag1b28 保留)→ 仍 38-40 仍掉耐力(**掉耐力跟随强格挡状态本身, 与
  staminaParam 无关**); 全回滚(0x90080006, 无 mFlag1)→ 35-37 + 耐力不动 ✓。
  **命名勿混**: mFlag.b28 = 格挡有效核成员(0x90080006/0x12400006 都有); mFlag1.b28 = 强格挡标记 —
  同号不同成员。staminaParam=256/10 对强格挡判定非必需(其真实作用待查, 可能微调消耗)。
  S/M/L 档由引擎按伤害定, 数据面不选。原版参照: 长枪 wp06 格挡动作 = mFlag b30 族 + mFlag1 b28
  (entry28: 0xD0000000 + 0x10000000 + stamina=10), 盾斧/长枪强格挡行为同源。
  基线存档: 346-valor-sheath.blend = 345(0x90080006 纯削韧)/306(0x12400006 无防御) 的原始态。

### 2026-10-14 (2) 长枪防御位词汇表 (user 二分 + lance.blend 解码)

- user 在 lance.blend 删 `TIML::nTimelineParam::PlMotionCommon.069` → 强化防御消失 → 该 action 经
  nodetree 反查 = **entry 120(カウンター溜め/76, loopFrame=150)** 的本体 TIML。
- **长枪格挡位词汇表 (PlMotionCommon mFlag, 32-bit)**:
  - 通常格挡 (entry 60/64/65): 0x42000226→0x42000006 (b30+b25+b9+b5+b2+b1) → 反应 38-40
  - 強G小 (entry 28): 0xD0000000 (b31+b30+b28) + mFlag1=0x10000000(b28) + staminaParam=10
  - **强化防御/カウンター溜め (entry 120, action 76, 27入边)**: **0xB0400002 (b31+b29+b28+b22+b1)
    + mTrigger f0=0x800(2048门)** → 格挡成功引擎请求 79-82 カウンター突き S/M/M/L(零入边);
    79-82 = 反击突刺档位(82 カウンターLガード 出边又见 強ガード→138)。**特征位 b29 = 蓄力反击族**
    (全武器唯一), 无 b30(不走38-40反应)。
  - 钩爪反击 (163/164): 0x42400002/0x42400003 (b30+b25+b9+b2+b1[+b0]) + **164 mFlag1=0x8000(b15)**
    + staminaParam=11 — b30 族(強G类)。160=0x10400000(b28+b22), 161=0x80400002(b31+b28+b22+b1)。
  - 151: 0x10000002 (裸 b28+b1, 最简格挡核, 无b31)。
- **位语义图景**: b28=格挡有效核(通用) / b31=格挡族公共 / b30=強G族(38-40反应) / **b29=蓄力反击族
  (79-82请求)** / b19=太刀刚一风味 / b25+b22=核组件 / mFlag1: b28(CB式强格挡标记)+b30(NFH)+b15(钩爪164)。
  每种防御类型 = 特征位组合; 新防御形态挖掘 = 按位表对照各武器防御动作。
- 151/160/161/163/164 在长枪 FSM 无节点(全引擎请求/跨状态机 — 钩爪挂上后非武器状态机, user 观察一致)。
- 全武器 mStaminaParamIndex 词表: {0,1,2,5,6,9,10,11,12,13}; NFH 306 用 256(词表外)。

- **76 防御解剖结论 (2026-10-14)**: 76=カウンター溜め class=`nHmAction::Wp06::cChargeStartStab`
  (类名嵌 vtable+0x20, 长枪类表 175 条可整表读类名), TIML=0xB0400002(b31+b29+b28+b22+b1)。
  **格挡成功→反击突刺 = 长枪类代码行为**(反击 = 独立类族 77-80: cChargeCounterS/M_StabUp/L_Guard;
  81=cChargeEndStab; user 观察的 79-82 转移实为 77-80 族), LS 的 W03 类无此代码 → b29 抄位只得
  "格挡有效"无反击 (user 实测: 格挡成功, FSM 不切换)。类方法无 32 位掩码可扫 = 旗在运行时拆成
  武器结构单 bit 位域(同 0x2D0 模式), 钉读取点需长枪角色动态断点。
  **LS 等价物(纯数据)**: 强格挡位(b30+mFlag1b28) 引擎必发 38-40 请求 → LS FSM 补 38/39/40 零入边
  节点 + 出边接反击 = CB/长枪"強G→反击"的 LS 版(nfh 38→69 / 长枪→84/83/107 同模式)。
  类表类名读取法: 类表 wp 行 +0x40 → entry*0x20 → +8 classPtr → +0 vtable → vtable+0x20 = ASCII 类名。

- **✅ 按住R数据侧接线完成 + IDProperty 双层坑 (2026-10-15)**: user 已在 FSM 加 R押してない→收刀边;
  实测松开无效 → 对照发现 **我们的 306 (valor_sheath_start) 整个缺 PlMotionInput 类别**(自制动作
  没建该轨; R/R押してない 属性随动作的 PlMotionInput 使能位写入 — 和方向组合同机制!)。
  **修法 = 给 306 补 PlMotionInput action + TIMLActionNode + 连到 TIMLDataNode**, 内容照抄
  NFH 306 档(他的按住/松开实测工作)映射到我们循环帧(f50/f60): mFlag f50=0x10000000
  f60=0xC0030000 / mFlag1 f60=0x0C000000 / mFlag2 f50=0xF00000。
  **⚠️ FreeKinetics IDProperty 双层坑**: 新建 action 若用 `a['freehk'] = {}` + 键赋值,
  **属性访问 (a.freehk.starType) 与键访问 (a['freehk']['starType']) 解析到不同层** —
  键层='TIML_Action' 属性层='None' → extend 的 starType 检查失败 → 类别被静默省略
  (export 尺寸不变、无报错)。**必须按 importer 原始写法纯属性赋值**
  (`a.freehk.starType = "TIML_Action"; a.freehk.timelineParamRaw = ...`;
  timelineParam/transx 等 EnumProperty 传字符串、失败即 skip);
  导出验证 = 306 categories 应含 PlMotionInput。

- **✅✅ 按住/松开 R 数据侧完全落地 (2026-10-15 user-verified)**: 306 PlMotionInput **恒定全开**
  (f0 即满值: mFlag=0xC0030000, mFlag1=0x0C003000, mFlag2=0xF0, CONSTANT) + FSM 的
  R押してない→100 边 → **按住 R2 = 架势保持(强格挡+掉耐力行10), 松开 = R押してない点火→100→312收刀**。
  早前失败根因 = 抄 NFH 档时两个值错位(mFlag 0x10000000↔0x00010000 半字错位、mFlag1 丢低半
  0x3000)→ R/R押してない 属性未使能。**结论: 输入属性(含否定态)全部随动作 PlMotionInput 使能位
  写入 — 与方向组合同一机制**。原版 98 类自带"松开→收刀"处理(R押してない 边与原生行为殊途同归)。
  现部署状态: 306 = 强格挡三件套(mFlag 0x52400006 + mFlag1 b28 + staminaParam 行10) + PlMotionInput
  恒定全开; 345 = 原始削韧态; 106/110/156 = 方向位。用户可调项: 掉耐力行号(换 staminaParam 值)、
  松开目标(改边指向 195 节点或其他)。ValorGauge.dll 仍禁用中(数据侧验证完毕, 恢复=改回名字)。

- **⚠️ 入场保护窗 (2026-10-15 深夜, 未测)**: 恒定全开档会拆掉架势入场保护 — 特殊纳刀按键含 R2,
  R 属性 f0 即活 → R边(→70 踏込斬り)首个 checkpoint 立刻点火 (user 实测: 按特殊纳刀直接打出
  気刃踏步斬)。**NFH 档位的前段关闭(f0=0)就是入场保护窗**。已恢复分段档: 306 PlMotionInput
  f0=全关(入场保护) / f50=partial(b16) / f60=full(0xC0030000) — 已部署未测(备份
  w03_00.lmt.bak-20261015-post-const)。明日首测: 特殊纳刀应正常进架势(不再直接70),
  按住保持+掉耐力, 松开→100→312。若仍 70: 查 mTrigger checkpoint 标记(原版306 f73=0x400,
  我们的 306 未必有对应标记 — checkpoint 时机可能与 mTrigger 绑定)。

### 2026-10-15 TIML 窗口编辑器 (FreeKinetics 扩展) + 0.0.1/0.0.2 导出验证链 (timl-editor/)

- **方向定案 (user)**: TIML 编辑不做独立应用, **改 FreeKinetics addon** 复用其 TIML
  导入表示 (TIML action → `FreeHKTiml.m<hash>` fcurve; bigFlags 类 mFlag/mFlag1/
  mFlag2/mTrigger 按 array_index 0/1 拆低/高 16 位, 导出 parseBigFlag 按整数帧合并,
  transition ↔ Blender 插值枚举 5=EXPO/6=SINE/1=LINEAR; Keyframe.back/period =
  controlL/R — 4.3 实测存在)。blend 检查点 = MHW-valor-blender/ 各版本 (user 手工
  逐步改对的现场, 差异多处, 验证以逐 entry 全量 diff 为准, 不假设小 diff)。
- **交付 1 — 窗口编辑器** `timl-editor/timl_window_editor.py` (已装入 addon 并挂
  `__init__.py` modules; N 面板 "FreeHK" 3D/Dope Sheet 双挂点): 五功能 = 无敌窗
  (b26+自动清窗内 b28+补 mTrigger 2048 门, 门已有则不重加) / 霸体窗 (b24) / 防御窗
  (b28 角度格挡+自动清窗内 b26, 无门键) / 攻击窗 (EC.mFlag 整值 0-3) / 按键位窗
  (PI 三轨任意位)。只动目标位保留同键其他位; 整数帧强制; 新建 PC.mFlag 轨种基线
  0x02400006; 新键继承邻键插值/back/period。ops: `freehk.timl_{iframe,armor,guard,
  attack_window,input_paint,gate,list_windows}`, action 用名字 StringProperty
  (**Operator 不支持数据块 PointerProperty**, 注册会炸)。
- **交付 2 — 导出验证链**: `_export_blend_lmt.py` (BLEND/OUT 环境变量, 与 GUI 按钮同
  exportNodeToFile 路径) + `TIML-Studio/dist/timl-diff.js` (逐 entry 窗口级+轨道级
  diff; 解析器 = TIML-Studio 27 测试字节级验证过的 TS 实现, npm test 全绿: 解析 161
  块/规范序列化字节对照 vanilla/回环/配方/保存校验 append-only + 回读断言)。
  **0.0.1 vs 0.0.2-345 diff 已出** (`timl-editor/_verif_diff_001_002.txt`): 5 个
  entry — 105 摘 b26 / 140↔345 类别块互换(345 得 b28 [0,12) = 手工 0.2s 防御帧) /
  342 攻击窗+2048门 / 306 输入窗重排 + PC.mFlag 0x12400006→**0x52400006 (b30 新增)**
  — 与 hist-MHW-Valor.md 手工轨迹逐条吻合。
- **验收 (2026-10-15 通过)**: 0.0.2 拷贝上五类 ops 各一次 → 同会话导出 → diff 仅
  entry 105 且逐键符合操作 (`_acceptance.lmt/_acceptance.log`)。**流程坑: ops 改
  内存 blend, 导出必须同会话或先保存** (分两次起 blender = 改动全丢)。
- **addon 两 bug 修复 (2026-10-15)**: ① `freeHKActionNodes.parseBigFlag` 合并双通道
  按 "低通道全帧→高通道全帧" dict 插入序产出键列 — **单边通道加键导出乱序键** (游戏
  要求有序; 手工 dopesheet 单边加键同样踩), 修复 = `sorted(channelFrames.items())`;
  回归: 未编辑 0.0.2 重导出 md5 一致 (f724d191)。② 编辑器关闭键继承 "当前值清目标位"
  不写 0 (写 0 会清同通道其他位, 在别的位窗上挖洞)。
- **位义更正 (推翻 09-13 笔记一处)**: 原版见切 140 `mFlag f0=0x80400000` = **b31+b22**,
  不是 b26 — nibble 读错 (b26=0x04000000, 0x00400000=b22); b26/2048 配方本身不受影响
  (312 局内验证过)。原版见切 TIML 无 b26 = 无敌在 FSM/动作层, 与 09-18 "b26 清零无效"
  互证。140 原版带 mTrigger f45=2048 (b31/b22 窗内) — 2048 门与 b26 并非死绑, 语义
  待再挖。TS 侧顺带产出: TIML-Studio (浏览器版骨架+验证器, 现役角色 = 导出比对器)。

### 2026-10-15 (2) 位语义表 + 多类别防御/按键轨 全量进编辑器 (timl-editor 增补)

- **BIT_SEMANTICS 表** (timl_window_editor.py, track hash → 类别 PC/PI → bit →
  (标签, 置信度 确证/推断/未解)): 面板"位语义速查"栏 + 窗口清单 + op 报告全带标签;
  新语义直接改表即全局生效。TS 侧同步 (names.ts piBitLabel/pcBitLabel,
  timl-diff.js 报告带标签; tsc CLEAN, 27 测试绿)。
- **多类别防御**: 面板除 b26 无敌/b24 霸体/b28 角度格挡三按钮外, "任意位窗"
  (轨 0-6 = mFlag..mFlag6) 覆盖其余类别 — b27(霸体?未解)/b31(GP?未解)/
  **PC.mFlag1 b30+b28 零伤格挡配方**(他版306 0x50000000, 配 mStaminaParamIndex=256)/
  **PC.mFlag2 = 事件位写入轨**(b15 = 见切成功锁存/纳刀态信号, 强假设;
  0.0.2-345 的 PlMotionCommon.345 实际带 mFlag2 b15 [20,open), list 已能解出)。
- **多类别按键**: PI 轨选择器扩到 mFlag/mFlag1/mFlag2/mFlag3-6 — mFlag1 =
  组合键开关(R2+方向, b12+b26/b13+b27 对, 推断); mFlag2(b4-b9,b22/b23 未解);
  PI.mFlag 补 b9/b15/b0 伴出位标签。
- **op 改名/扩展**: `freehk.timl_bit_window`(track 0-6 + bit + on) 取代
  timl_input_paint 成为通用位窗; 无头验收重跑 = diff 与金标准逐字节一致
  (`_verif_002 vs _acceptance.lmt` 仅 entry 105 预期变化)。
- 带标签 0.0.1/0.0.2 报告已重建 (`_verif_diff_001_002.txt`): 306 段现在可读出
  "连段(b16)[50,open) / 特殊(b17+b31)[60,open) / 组合键开关 mFlag1 全家 [60,open) /
  PC.mFlag1 零伤格挡? b28 [0,open) / PI.mFlag2 未解 b4-7 [0,open)" — user 手工
  0.0.2 的 306 改动全景。

### 2026-10-17 R+方向组合键解锁簇: 部署还原闭环 + 结论入编辑器 (user-verified)

- **丢簇复盘**: 10-13 的使能位当时只 OR 在**导出物**上 (备份 bak-20261013-rdir-106110156);
  blend 里 entry 106/110/156 的 PlMotionInput action (`.149/.153/.163`) 一直是基线值。
  10-16/17 从 blend 重导出后簇消失 → 局内 R+方向失效。**教训: TIML 配方必须落在 blend 侧,
  导出物后打补丁活不过下次导出。**
- **本次还原 (MCP 会话内, 打开的 blend = MHW-Valor-0.0.2-345-r+up-dash, 与
  valor-spirit1 同内容副本)**: 三条 PlMotionInput action 的 `FreeHKTiml.m6E63FBC7` 双 fcurve
  (array_index 0/1 = 低/高 16 位) 同帧改值, 106: f24=0xC7FC10CC/f44=0xCFFC10CC/f54=0xCFFC30CC/
  f120=0xC7FC10CC; 110: f38/f58; 156: f5/f14/f46 (完整表入 timl-editor/README)。
  导出 (globalCacheClear + exportNodeToFile → 全新文件防 inject) → timl-diff 验证:
  vs `w03_00.lmt.r+up_dash` (md5 26e43fd4) 106/110/156 **零差异**, vs 旧部署全文件仅此
  3 entry 差异 → 部署 (md5 d3d6212c; 备份 w03_00.lmt.bak-20261017-pre-rup-restore)。
  **blend 已保存 = 簇首次落进 blend, 以后导出自带。**
- **FSM 侧 + 361-363 衔接动画**: user 已接好且局内验证无误 (R+↑ → MOT 衔接 361-363 播对),
  本会话未动、不要再动。
- **结论入库**: FreeKinetics `timl_window_editor.py` BIT_SEMANTICS PI.mFlag1 重写 —
  10-12 的 "b12/b26 组合键开关" 假设更正为**基线位**; 解锁 = 位簇
  {b2,b3,b6,b7,b18-b25,b30,b31} 整簇 (确证, 参考 106 增量恰为 14 位), b27/28/29/b13 =
  中段次级位 (未解)。timl-editor/README 补配方表; TIML-Studio 仅同步 timl-diff 标签
  (该目录只留验证工具链)。
- **bisect 遗留 (10-13)**: 单字段最小配方仍未验证。字节级事实: 工作参考值里 **b16/b17=0**,
  10-13 笔记 "f8(b17:16)=方向组" 的字段序号对不上参考值 — 候选实为 b19:18 (掩码
  0x000C0000) 或别的字段, 抠位实验时先重排字段序号。最小化前部署维持全簇 (NFH 撒网值)。

### 2026-10-17 (2) 勇气纳刀強格挡耐力: mStaminaParamIndex 定案 (user 局内 A/B + 反编译)

- **起因**: 306 复制线 (gp-valor-sheath.blend, 306 格挡转強G) 掉耐力极慢; 旧好工程
  (no-change-306-to3840) 快。timl_headless dump + jsondiff 定位: 差异 = E306 缺
  **mStaminaParamIndex (0x85A14934)** 轨。GUI"耐力档"显示的 -1 是配方输入滑条 (-1=不动),
  不是数据现值 — 该坑已加 GUI 现值标签堵死。
- **修复 (MCP 会话内, spend-stamina.blend)**: ①E306 PC 补 mStaminaParamIndex ix0 f0=10
  (CONSTANT); ②mFlag f0 0xD0000000→0x52400006 (对齐已验证定稿); ③从旧工程 append
  PlMotionInput306 (按住R分段档, freehk 属性+6 fcurve 完整) 并接 E306 的 TIMLDataNode。
  无头 dump+jsondiff vs 旧工程 E306 = **0 差异**。user 局内: 耐力正常 ✓。
- **副作用 (user 发现并接受)**: PI306 的 f50/f60 输入接受窗打开后, 架势中 R→気刃踏込斬り、
  △+○→袈裟斩 等 FSM 链接恢复点火 (=旧好工程的完整行为; f0 保护窗仍防进架势瞬间出招)。
- **档位 A/B (user 局内实测)**: **9 ≈ 7 > 8 > 10 > 1 ≈ 11 (慢), 256 = 完全不掉**;
  306 最终选 10。345 完全不掉 = 配置使然 (mFlag 0x90080000 无 b30, 无 mFlag1.b28 →
  普通 35-37 格挡路线, 本来就不掉耐力), 想掉 = GUI 強G 配方 (起0止12) 一键三件套。
- **代码依据 (反编译实锤)**: ①mStaminaParamIndex 是正式 TIML 参数 (名字串 0x1433C6648,
  注册于 FUN_14191f7d0), 采样值写运行时参数块 **+0xa8** 槽位; 邻座 +0x94 mSuperArmorTime/
  +0x98 mHyperArmorTime/+0x9c mNoHitTime/+0xa0 mDamageHitNo/+0xa4 mScrHitNo/
  +0xac mStaminaRecoveryRate — 即 PlMotionCommon 采样值块。②plp (pl_param, 玩家参数)
  有社区已命名表 **"Stamina Time Reduce mCore"** (17 行: Unk1-3/HammerBowCharge/Unk4-7/
  **LancePowerGuard**/ClutchDrain/ClutchCounter/Unk9-14) = 掉耐力查表本体; 256 越界→不掉
  (NFH 选 256 即为无掉耐力)。行值未读出 (本地提取的 plp 是 Iceborne 加密态) —
  MHW Editor 打开 plp 读 mCore 块即可钉死每档数值; 二进制内无硬编码表 (值从 plp 加载)。
- **GUI 工具增补 (FreeKinetics e2a48f1 之后未提交)**: `freehk.timl_stamina_set` 仅写耐力档
  (不动位窗) + 面板现值标签 + describe_windows 耐力行; 自测全过。

- **mCore 表真值 (2026-10-17 解密读出)**: plp 为 Iceborne 加密 (Blowfish ECB, key=
  `j1P15gEkgVa7NdFxiqwCPitykHctY2nZPjSaElvqb0eSwcLO1cOlTqqv`, 4字节字序交换后解/解后换回,
  上游 Synthlight/MHW-Editor Util/EncryptionHelper.cs 同款); 解密产物
  `_pl_param_decrypted.plp`。Stamina_Time_Reduce_mCore @文件偏移 0x250 (1-based **18 行**, 行序以上游
  Generated/Structs/PlData/PlPlayerParam.cs 为权威): 1:Unk1=8 / 2:Unk2=30 / 3:Unk3=50 /
  4:HammerBowCharge=0(不掉) / 5:Unk4=0 / 6:Unk5=50 / 7:Unk6=70 / 8:Unk7=100 /
  9:**LancePowerGuard=150(最快)** / 10:Unk8=50 / 11:ClutchDrain=50 / 12:ClutchCounter=10 /
  13:Unk9=20 / 14:Unk10=40 / 15:Unk11=60 / 16:Unk12=1 / 17:Unk13=60 / 18:Unk14=100。
  user A/B 大信号吻合 (9 最快=长枪强格挡档; 8>10; 1 慢; 256 越界不掉), 两处待复核:
  表值 7(70)<8(100) 与 user 记的 7>8 相反 (体感误差?); 10/11 表值同为 50 而 user 记
  11 慢 — 复测可用 12 (=10, 明显慢档) 做对照钉死基制。
  chunk-extracted = 全 chunk 合并提取 (用户确认), 表值即当前冰原生效值。
  解密脚本 = `plp_decrypt.py` (根目录)。

- **加密逆向坐标 (2026-10-17 反推验证)**: plp 等 userdata 的解密 key 非破解所得 —
  游戏资源字典明文携带。key 表 = .rdata **0x1431E0000** 起 (条目 = magic 4字符 +
  RSZ 类型名 rXXXParam + 成员名 + 可选 key; plp 条目 = plp/mParam/<key>/rPlayerSkillParam,
  34 处代码引用); Blowfish P 数组 (π 常量) = .data **0x144BE0580** (唯一份),
  引用它的 key-schedule init @0x1421F81CE。同表可枚举全部加密类型与其 key
  (plip/plsp/… 各有自己的条目)。BSwap = Blowfish 大端字序 vs LE 缓冲的适配。

### 2026-10-17 (3) TIML 参数注册表破局: PlMotionCommon 全 65 项提取 (name→槽位→类型码)

- **钥匙 = FUN_14191f7d0** (PlMotionCommon 注册函数, 反编译行 5174891-5176565): 每参数一条
  描述项 {名字串VA(.rdata 明文), 类型码 6/10/12, owner, **owner+槽位**}。TIML 数值轨的值
  动作期间被采样进运行时参数块槽位 — **名字即语义的一半, 槽位读者即另一半**。
- 全 65 项 + 作用标注已入 **timl-editor/TIML_PARAM_REGISTRY.md** (含方法论:
  数值轨=槽位读者追踪 / bigFlags 位轨=player+0x8938 事件位读者枚举 / mTrigger=事件码分发;
  复现脚本 extract_timl_registry.py)。高价值自证名: mNoHitTime/mSuperArmorTime/
  mHyperArmorTime (=b26/b24 位窗的数值版, 可直接填时长), mMotionSpeed (动画速度),
  mWeaponMotionNo/StartFrame (武器动画覆盖/起始帧, 接 09-20 纳刀研究), mTurnSpeed (转身钳制)。
- 其余类别 (PI/Visual/Collision/GameParameter) 注册函数待定位 (共用分配器
  FUN_142171fe0, xref 可枚举); 加密 key 表 0x1431E0000 见上文加密逆向坐标。
- mStaminaParamIndex 是本方法论首个全流程定案 (注册表→plp 查表→局内 A/B)。

### 2026-10-17 (4) 气合声时机定案: mTrigger = 行位掩码触发器 + E345 喊叫重定时 (部署待局内验证)

- **nWwiseTimeline 触发语义闭环 (E170/E345 双侧核对, 全文见
  timl-editor/TIML_PARAM_REGISTRY.md "mTrigger 触发语义定案" 节)**:
  mTrigger bigflag 键值 = ReqNo 行位掩码 (bit0=A … bit4=E, 值 1/2/4/8/16),
  键帧 = 触发帧; 行序 A~E = 二进制轨序 = blend fcurve 插入序
  (patch_timl_defense.py dump 直接给 A~E 轨名)。
- **E170 (気刃突刺, 246f) 两声喊叫 = B(145)@f6 + B(145)@f141**; A(115)@182/235 =
  收招气音, C(135)@98 一声疑似气音。3 段式: 起手喊/中段喊/收招气音。
- **E345 (valor_1, 142f) "有两声但时机不对"病灶**: 旧触发 {36:C, 90:C, 111:E(155),
  131:B, 155:A} — **f155 超出动作长永不触发**, f111/f131 落收招死区 (主发力爆发
  f30-54, 二段发力 f113, 二刀挥砍声 f130)。
- **重定时已部署**: 新触发 {36:C, 50:E, 113:C, 122:B, 138:A} — ReqNo 值全未动,
  只挪触发帧。两声无论来自 C×2 (→f36/f113) 还是 E+B (→f50/f122) 都压在发力点。
  顺带可验 155 变体是否出声 (f50)。局内 A/B 待 user 验证。
- 操作链: blend EG01.207 mTrigger 双通道键同步移动 (ix0/ix1) → 存盘 (blend bak
  pre-shout-retime) → exportNodeToFile 全量导出 → **与部署文件字节 diff 仅 11 字节**
  (两簇 = 345 mTrigger 双通道) → 部署 (bak 20261017-2100-pre-345-shout-retime)。
  全量导出前先 cmp 部署文件是防回归的有效手段 (blend=部署母本时 diff 应接近零)。
- **v2 修正 (2026-10-17 user 局内反馈)**: 第二声应为 f40 (初版 f50 偏晚) →
  E345 mTrigger 终版 {36:C(4), 40:E(16), 113:C(4), 122:B(2), 138:A(1)}; 与 v1
  仅差 1 字节。**副产物定案: E 行 ReqNo 155 局内出声 = 有效喊叫变体** (v1 里
  f50 那声被听到并判定时机), "111~145 之外换变体"猜想成立; C(135)@36 同闻。
  部署备份 w03_00.lmt.bak-20261017-2115-pre-345-shout-f40。
- **v3 终版 (2026-10-17 user 裁定)**: 第二声 (E 行/155) 两版试位 (f50→f40) 都不对,
  **直接删除** → E345 mTrigger 终版 {36:C, 113:C, 122:B, 138:A} 五键 (含 f0 基线)。
  部署备份 w03_00.lmt.bak-20261017-2120-pre-345-shout-del2nd。**删键坑**: 文件缩短
  16 字节后所有后续偏移前移, cmp 字节对比失效 → 全 entry TIML 语义 diff (464 entry
  只有 345 变, 433/435/437 报错为空槽两边同款噪声)。155 可闻结论不变 (v1/v2 都听到)。
- **喊声载体翻案 (2026-10-17 深夜, user A/B 定案)**: user 手动删 345 的整个
  **EventGroup02.086 → 灭喊** (EG01 完整在场), 撤销恢复 → **345 的喊声由 EG02 驱动,
  行 A/B/C = 1119/1409/1019 才是喊叫请求号**; 此前对 EG01 的 v1/v2/v3 改时机/删键
  全部无效 (用户听到的两声从未移动)。v4 终版: EG02 mTrigger {f2=2(B=1409 拔刀喊),
  f130=4(C=1019 二刀喊)}, **f72 事件已删** (= user "第二声", 先说应在 40 帧/主爆发
  f30-54, 后裁定删)。EG01.207 保持 v3 {36,113,122,138}, 作用待 isolated;
  "155/135 可闻"结论撤回 (那两声实为 EG02 f72/f130)。E170 的 EG02 {22,84,191}
  与"两声"同构旁证。教训: 类别角色未 isolated 前, "改了没反应"不能当语义否定。
  部署备份 w03_00.lmt.bak-20261017-2135-pre-eg02-f72-del。
- **345 声音组件全景 (2026-10-17 深夜 user 辨认, 数据入 TIML_PARAM_REGISTRY.md)**:
  金属摩擦 = EventGroup06.182 (6 行 56xx + 位掩码触发 {1,20,31,60,148,157});
  **刀风声 = GameParameter.206 (无 mTrigger, 连续参数轨被音频逐帧采样** — 强度曲线
  30→15 / 28→30→15, 灭声=曲线归零或删 action); 踏步泥土 FX 自带声 =
  EffectParameter1.106 (3 槽使能窗 f5-20/f60-65/f140-150 + id 3199→3198, 另两槽
  2018/2019 单帧点 f69/f73)。GameParameter = TIML 第三形态 (连续参数), 触发帧
  编辑模型不适用。工程撤销栈跨 GUI/MCP 共用易缠 — 以磁盘为准 open_mainfile 重载。
- **v5 音效重排 (2026-10-17 深夜, user 指令: 踏步→50, 金属声+刀风→40-55)**:
  EG06.182 金属声 6 触发收成 3 个 {f41→C(5655), f47→D(5620), f53→F(5660)}, 删
  f60/f148/f157; GameParameter.206 刀风曲线重整 (40 前归零, 40-55 峰值, 尾部原样:
  A{0,40:30,55:30,125:15} B{0,40:28,48:30,55:25,155:15}); EP1.106 踏步窗
  f60-65→f50-55 (拔刀步/收招步不动)。部署备份
  w03_00.lmt.bak-20261017-2200-pre-345-sfx-rearrange (= user 辨认实验版, EP1 整缺,
  v5 已带回全量 13 action)。**新坑两条**: Blender keyframe_points.add() 不返回键
  (取 [-1]), remove() 会使兄弟引用失效 (整条 fcurve 删了重建最稳);
  patch_timl_defense dump 只显示 dataType=1, float/bool 轨要 struct 原始解析。
- **v6 (2026-10-17 深夜 user 反馈二轮)**: v5 把 EG06 金属声收 3 触发进 40-55 →
  **游戏内金属声全灭** → v6 还原原版 8 键 {f1:4, f20:8, f31:32, f60:2, f148:16,
  f157:8, f176:0} 恢复。⚠EG06 触发语义存疑 (非行位掩码? 行序配对? A/B/C 哑行?),
  再动之前先单触发 A/B。刀风声按 user 情报 (破空声资源开头几帧静音) 起音点提前:
  GameParam A/B 曲线 = 30 前硬静音, f31 瞬起, 窗口 30-45, 尾部原衰减。
  **新坑: 重建 fcurve 后必须设插值** — 导出器 interpolationMapping =
  [CONSTANT, LINEAR, QUAD, CUBIC, QUART, EXPO, SINE], 默认 BEZIER 直接
  ValueError 崩导出; bigflag 轨用 SINE (vanilla 惯例), 数值曲线 LINEAR 可用。
  部署备份 w03_00.lmt.bak-20261017-2230-pre-345-eg06-restore。
- **v7 基线 + 声音编辑器入插件 (2026-10-17 收尾, user 裁定"声音后续研究")**:
  未验证改动全部还原 (EG01→原版 {f36:4, f90:4, f111:16, f131:2, f155:1} (E 行=155 是 user 的变体实验, 保留),
  GameParam 曲线→原版{30→15}/{28,58:30,155:15}), 部署 = v7 验证正确基线
  (喊声-1 + 踏步50-55 + 其余原版; 备份 pre-345-sound-baseline)。
  **timl_sound_editor.py 已入 FreeKinetics 插件 (main 4cb534e)**: mTrigger 触发
  解码/增/删/挪 + ReqNo 行值编辑 + 类别角色速查, 双通道同步+插值护栏
  (mapping=[CONSTANT,LINEAR,QUAD,CUBIC,QUART,EXPO,SINE]); 会话内往返测试通过
  (add@f200→del 净零, move/set_row roundtrip), 部署文件未动。
  无头入口 freehk.timl_snd_*; 用法见 timl-editor/README.md 新节。

### 2026-10-18 纳刀状态强制切换逆向: weapon+0x23C0 特殊纳刀序列标志 (SHEATHWATCH 验证待做)

**背景**: valor_sheath_to_evade (entry 312, blend valor 链 130 帧) 尾部 ~f121 被游戏强制切到
收刀站立, 玩家无法在动作中进入走动。TIML 路线已排除: PlMotionInput 位窗 (f10 开 b8/b9=0x300,
f96 全量 0x3C39) 部署确认在游戏文件里 (typeCount=7) 但局内无效果。

- **原版 entry 312 = 90 帧动作**, PlMotionInput.mFlag 渐进开窗: f40=0x1410(b4,b10,b12) →
  f50=0x3C30(+b6,b7,b12,b13) → f55=0x3C39(+b0,b1,b5)。**全程无 b8/b9** — valor 部署版的
  f100=0x3C39 就是抄的原版终态掩码。b8=翻滚/b9=走动的位义猜测对此动作不成立。
- **312 无动作类**: set-3 类表 (0x142EF5C30, 113 项) 只覆盖 id 0-112, 312 跑兜底类 →
  PlMotionInput 取消窗位大概率没人消费。类表里有现成行走类: **111=cWalkWpOn(持刀走),
  112=cWalkWpOff(收刀走), 104-110=cWpOn/cWpOff(拔刀/纳刀切换)** — DLL force 路线的现成目标。
- **收刀期望裁决器 = FUN_1421237B0 (大更新器) 中段** (decompile 定案):
  - `weapon+0xA50` 或 `weapon+0x23C0` (byte) 任一非0 → **每帧强制 FUN_142150d10(td,0)=纳刀期望**;
  - set-3 动作 {5,6,9,10,69(0x45),87(0x57)} → 纳刀期望; **其余全部 set-3 id (含312) → 拔刀期望**;
  - **极性修正: arg/latch 0=纳刀, 非0=拔刀** (09-25 SHEATHPROBE 收刀态实测 +0xC71=0 吻合;
    09-20 笔记里 "+0xC71: 0=拔刀/非0=纳刀" 写反了, 以本条为准)。
- **+0x23C0 生命周期**: 置位 = 特殊纳刀状态设置器 (0x1421443F8, `+0x23C0=sil`) 与阶段推进
  (0x142144E04, 门控 `新阶段 > weapon+0x23B4`); 阶段参数经**事件类型7** (bit2 门控) 装载进
  weapon+0x23B0/B4/B8 (FUN_142124B20); **唯一清除点 = 武器全量重置 FUN_142124590** (换区/重置)。
  → **主嫌疑: valor 链第一招接 306 (原版特殊纳刀 entry) 置起 +0x23C0, 自定义 342/312 走不到
  原版序列完成路径 → 无人清除 → 312 全程被强判纳刀期望 → 踢回收刀站立**。
- **SHEATHWATCH 已实装 (mod build, md5 ba465327130a8b30da1f3e4e2a5e51c5)**:
  Present 线程边沿监视器, 只在 weapon+0xC71/+0xA50/+0x23C0/+0x23B4/+0x23C3 或当前动作变化时记
  `SHEATHWATCH act=<set:id> c71= a50= 23c0= 23b4= 23c3=`。走一遍 valor 链看 312 期间 23c0 是否=1。
- **部署注意**: 此前插件目录里的 `ValorGauge.dl l` (带空格) = user 刻意停用的版本, 本次测试
  部署了激活的 ValorGauge.dll, 测完可自行改回停用命名。
- **改进方向背景 (user 2026-10-18)**: 南风焓 mod 的方案 = 把该动作拆成两个 FSM 状态, 第一段
  结束时玩家按了键就不进收刀状态; user 因要保留原生状态、可动 FSM 节点少, 倾向 DLL 清标志 +
  force 走动路线 (候选 {3,112} cWalkWpOff / {1,0} set-1 locomotion, 刚1 同款 force 通道)。

- **SHEATHWATCH 实测结果 (2026-10-18, user 触发 312 ×3, 日志 31 行)**:
  **+0x23C0 假设证伪** — 全程 c71=0 a50=0 23c0=0 23b4=0 (含三次触发), 无标志可清。
  两个新事实:
  1. **"312" 实际借原版动作 id 100 播放** (原版 100 的类硬编码播 LMT312; 98=持刀保持态播
     306, 同族 98/99/100/37 = sheath-counter 的 mine 监视列表)。日志 act=3:100 ×3 = 三次触发,
     每次后面**直接跟 1:0** (收刀 locomotion) = 动作结束回收刀站立是原生收尾, 不是中途强踢。
  2. **c71 全程 0** = 玩家在整个持刀保持态/纳刀系动作期间, 游戏本来就判"纳刀" (裁决器对
     98/100/101/102 有专属 case: 置 weapon+0xAB8=1; 09-20 笔记的 "+0xAB8 锁存" 读方仍待钉)。
  → "121 强制收刀站立" 的剩余嫌疑: ① 动作 100 原生类按自己的节奏收尾 (原版 312 只有 90 帧,
  类的结束逻辑没适配 130 帧新动作); ② 尾段修复里的 030 scaleY 台阶 (f121=2→f122=1, 挂点
  切收刀表) 在 121 触发可见的刀上背+状态切换。
  **下一步实验**: A. 030 scaleY 台阶挪 f129/f130 再触发, 看回 1:0 的时机是否跟着挪
  (挪 = 台阶触发; 不挪 = 类自己收尾)。B. 走动功能直接 DLL 规则化: 3:100 期间 motion 时钟
  (anim+0x10C) ≥96 帧 + 按方向 → force DoAction {1,0}/{1,4}/{1,8} (c71 本来=0, set-1 收刀
  locomotion 是原生合规目标; force 通道 = 刚1 同款)。

- **原版 A/B 实测 (2026-10-18, user 把 nativePC 的 w03_00.lmt 改名停用 → 游戏读包内全原版)**:
  原版链 = 3:98 → 3:99(=LMT306 保持, 272f, **loopFrame=129 循环点**) → 纳刀状态进入 ✓ →
  超时 → 3:100(=LMT312, 90f) → **c71 脉冲 0→1→0** (拔刀→回纳的载具状态握手) → 1:0 收刀站立
  (原生可走动)。×3 次模式一致。
  自定义文件 (motion+TIML 全自定义, 130f): 无脉冲、不进纳刀状态。
  → **两个端点已测, 缺的状态进入在数据里 (动作 or TIML 待分离)**。
  注意: +0xC71 不是常驻状态位 — 拔刀攻击 (3:79 等) 期间也恒 0, 只在状态翻转瞬间脉冲,
  不能当"当前拔刀/纳刀"指示器读 (此前"极性修正"条目作废, c71=过渡脉冲)。
  纳刀状态的本体指标另有其人 (候选: weapon+0x2368 构架标志 / +0xAB8 锁存), 下轮 SHEATHWATCH
  应把 +0x2368/+0xAB8 加进监视列表。
- **中间实验方案 (定位动作 vs TIML)**: blend 里把 entry312 接原版312动作 + 保留自定义 TIML,
  导出部署测脉冲; 有 = 动作侧 (查原版312的 030/047 scale 键型, 按自定义时序复刻: f96 切收刀表);
  无 = TIML 侧 (把原版 PlMotionInput 渐进窗 f40=0x1410/f50=0x3C30/f55=0x3C39 + mFlag b24 +
  GameParameter raw 轨搬到自定义时序)。原版 312 TIML 全轨快照已可从 chunk-extracted dump。

- **SHEATHWATCH v2 原版签名定案 (2026-10-18, ×3 次一致)**:
  ① 3:98 begin 期间 **ab8 0→1** (裁决器对 {98,100,101,102} 每帧置 +0xAB8=1 的代码侧行为,
  实测吻合); ② ab8=1 贯穿 99(306保持) + 100(312); ③ c71 在 3:100 中段脉冲 0→1→0
  (拔刀→回纳); ④ **序列结束回 1:0 时 ab8 清 0**; ⑤ **2368 全程恒 0** — 306/312 链不用
  构架标志 (那是 IB/気刃突刺构架的), "纳刀状态"的本体 = 保持动作自身循环 (原版306
  loopFrame=129) + ab8 标记, 没有独立标志位。
  → 结论: 原生"可走动"不是任何 TIML 位窗给的 — 保持态的走动 = id99 类自己的构架步,
  312 后的可走动 = 动作结束自然回 set-1 locomotion。id100 的类是攻击类, 不实现移动取消,
  **任何 TIML 窗都给不了 312 尾段走动** → DLL force 是唯一路线 (motion≥f96 + 按方向 →
  force {1,0}/{1,4}/{1,8}; 若要刀提前上背, 030 scaleY 台阶同步挪到 f96)。
  备选: {3,112} cWalkWpOff (类表实收刀走类)。

### 2026-10-18 (2) 自定义招伤害引用定案 + col 路由工具化 (apply 脚本 + TIML 编辑器面板功能)

- **伤害路由一句话**: CollisionTimelineObject ノードID 键值 → wp03.col names[unk4==值] →
  {cgp=CLGM bank(判定盒), move=ATK 行(伤害)}。**改引用 = 伤害+判定几何一起换** (名称表
  条目绑定两者); 只想换伤害需配 col 空闲名称条目 (未做)。EC00 mFlag 窗值: 2=伤害槽,
  1=挥空槽 (apply_valor_attack_config 实证)。
- **大居合 4 版本 = 气刃等级变体 (user 定案)**: 308=無/309=白/310=黄/311=赤 →
  node 86/87/88/89 → ATK[46]/[47]/[48]/[49] = **MV 17/19/31/39**, 全 bank9。
  每版单ノードID键 = 全部伤害窗共用一行; 308 的第二 EC 窗 [35,42] 是 g1 挥空,
  "第一段/第二段"在数据上同伤。
- **五招 MV 平衡定案 (user 局内验收)**: 刚1(156) node 60→89 (=居合赤 39; 原=跳2连二段
  ATK[20]=16)。一文字三版 339/340(原見切り攻撃 node63 ATK[25]=12)/342(原斬り下がり
  node56 ATK[14]=22) → node 54 **気刃斬り2(lmt106) ATK[17]=30**。无双斩(343) →
  **双节点分段**: f0=88(黄31) + f58=89(赤39), 第二刀窗 [58,70], 第一刀窗 [31,39] —
  多段不同伤的原版先例 = LMT155 (ノードID f0=48→f25=60 引擎逐帧采样自动切换)。
  几何连带: 156/343 bank8/0→bank9 (居合盒变大), 339/340 bank8→bank0 (普通斩盒变小),
  342 bank0→bank0 不变。
- **`_dmg_task/apply_damage_refs.py` = 二进制幂等重放** (blend 每次导出后跑; 期望值
  既收旧也收新=skip): 5 处ノードID u32 值改写 + **343 双键结构编辑** (2-key 数组
  align16(EOF) 追加 + track xoff/xcount 重指, apply_valor CTO 克隆同款手法)。
  自动 .bak + 回读验证。键记录 20B: value@0 / bounce@4,8 / frame f32@12 / interp+easing@16。
- **blend 侧已入树**: MHW-Valor-0.0.2-damage-modify.blend 的 FreeKinetics fcurve 已带
  5 处引用 (ノードID = `FreeHKTiml.mB296CA66`; バンク = `mBF9000B3` 仅记录不路由),
  导出源头从此自带, 二进制脚本降级为一致性校验。TIML 轨道在 blend 里以哈希为 data_path,
  编辑器 `_track_name` 只给 mFlag 族起名。
- **FreeKinetics 新功能 timl_damage_editor.py** (live 于 Blender addons 目录
  `.../4.3/scripts/addons/MHW-FreeKinetics/`, 仓库留档副本 `_dmg_task/`; window_editor
  面板对 CollisionTimelineObject action 显示"伤害引用"分区; `__init__.py` 已挂载):
  - `freehk.timl_dmgnode_set` (帧上有键改值/无键加键) / `freehk.timl_dmgnode_del`
    (保底 1 键); scene props `freehk_timl_node` / `freehk_timl_col` (col 路径, 默认
    部署 wp03.col, mtime 缓存解析 node→名称/bank/ATK→MV 实时预览)。
  - 新键 interpolation/back/period 复制帧前最近键 — **BEZIER 会 ValueError 崩导出**
    (interpolationMapping 只认 CONSTANT/LINEAR/QUAD/CUBIC/QUART/EXPO/SINE), 帧强制整数。
  - 导出键序列化 = `[int(co.y), back, period, interpolationMapping.index(interp)]`
    (freeHKActionNodes.frameParam); mFlag 族双通道 (array_index 0/1 = 低/高16位)
    由 parseBigFlag 按整数帧合并。
- **冒坑记录**: ops.report({'ERROR'}) 从脚本调用会抛 RuntimeError (守卫测试时中断脚本,
  非缺陷); 热加载 = 反注册旧 _classes/_SCENE_PROPS → importlib.reload → 重注册,
  重启 Blender 后经 __init__ 干净加载。

### 2026-10-19 (3) 卡肉(hitstop)完整定案: ATK 行 HitLag_a/b 是本体, GS wp00.col 实测 (user 的 128↔277 交换实验解释)

- **卡肉 = 当前判定路由到的 ATK 行 `HitLag_a`/`HitLag_b` (秒, 成对设, HitLagSpd 保持 1.0)**。
  LMT 动作曲线本身不携带卡肉; LMT entry 里"相关的配置" = TIML CollisionTimelineObject 的
  ノードID 路由 (blend fcurve `FreeHKTiml.mB296CA66`) → col names node → move → ATK 行。
  **交换 LMT entry = 连 TIML 一起换 → 路由落到别的 ATK 行 → 卡肉/伤害/窗全变** —
  user 交换 GS entry 128↔277 后"卡肉基本没了"的完整机理。
- **GS wp00.col 实测梯度** (chunk-extracted/chunk/hm/wp/wp00/collision/wp00.col,
  79 names/5 banks/77 ATK; 至今未部署 — nativePC/hm/wp/wp00/ 只有 mot/): 普通连段
  0.15-0.18 / 溜め斬りLv1/2/3 = 0.15/0.20/0.40 / 強溜め斬りLv3 0.50 / 強なぎ払いLv3 0.70 /
  **闘魂斬りFinish Lv1/2/3 (node79/80/81 → ATK71/72/73) = 0.60/0.70/0.85** (网传"真蓄卡
  0.2s"严重低估; 满蓄 0.85s ≈ 全游戏顶配; MV 144/210/264, PartMod 1.1/1.2/1.3)。
  Finish 多段行 74-76 = Lingering1+Frame_Delay4+HitLag0.1 (尘土小判定走多段配方, 非主卡)。
- **两 entry 定位** (MHW-greatsword.blend live 内省): entry 277 = 真蓄族 ~300f
  (CollisionTimelineObject.060 ノードID[0]=81 单键 + obj 窗 f46-58 + EC00 ReqNo 窗
  f19-25/94-108/277-283/295-301) → Finish Lv3 行 = 0.85s; entry 128 = ~150f 縦斬り族
  (ノードID=1, obj 窗 f36-46, EC00 窗 f37-43/111-117/143-149) → ATK0 = MV48/0.15s。
  交换后 277 槽 = 0.85→0.15 且 MV 264→48。注: blend 里 entry 277 的 motion action 名带
  `::272` 历史标签 (工作副本有改动史), 身份以 TIML 路由为准。
- **实用**: 调卡肉不用动 LMT — col_tool setval 目标行 HitLag_a/b; 或改 names 表该招的
  move 指针借别的行 (cgp 不动 = 几何不变)。GS 要改先把 wp00.col 拷到
  nativePC/hm/wp/wp00/collision/ (目录名严禁空格)。LS 自定义行现值: 行22(node85)/
  行42(无双斩一段)/行45(二段) 全 0.1s — 想要力量感可提 0.15-0.4 (LS 原版上限 =
  空中气刃突刺 0.40)。
- **Blender live 内省法** (本次全程走 MCP): TIMLActionNode 无 animation_data, 轨道在
  `node.input_action` (bpy action, 名 = `TIML::nTimelineParam::<track>.NNN`) 的 fcurves,
  data_path 哈希: ノードID=mB296CA66 / mFlag 位包=m08FD20A6 / EC00 ReqNo 也在 m08FD20A6。

### 2026-10-19 (4) 卡肉引用链深挖: "如何引用 HitLag" 定案 + 运行时布局 + 消费方静态负结果

- **A/B 实验定案 (user)**: LMT 还原 (动作/窗/路由全原样) + 仅把 GS 真蓄 Finish 三行
  HitLag 0.6/0.7/0.85→0.1 (nativePC/hm/wp/wp00/collision/wp00.col 行71/72/73, 首次部署
  GS col) → 局内真蓄卡肉消失、伤害不变 → **卡肉消费点 = 行数据本身**。回答"如何引用":
  **TIML 只负责选行+开窗** (CollisionTimelineObject ノードID 逐帧选行, EventCollision00
  开窗), 命中层在打中瞬间从"当前行"取 HitLag — 它不是 TIML 字段, 是行属性。
- **HitLag 运行时偏移 (Load 反编译推定, 未消费验证)**: cAttackParamPl Load
  FUN_14027d880 = 逐字段流式反序列化, 读入顺序=文件字节顺序。用 MV+0x20/PartMod+0x28/
  Stun+0x68/Exhaust+0x6c/Soften+0x1c8/Elem+0x1d0/Status+0x1d4/Mount+0x1d8/Flinch+0x240/
  MindsEye+0x232 十锚点对齐 → **HitLag_a=+0x1F0, HitLag_b=+0x210, Spd_a=+0x1EC,
  Spd_b=+0x20C** (文件侧 0x6F/0x83 与 setval 地址精确吻合)。+0x1FC..0x207 是 8 字节洞
  (fUnk18 直跳 fUnk19), 尾部疑似两组子结构 (+0x1E8 / +0x208 各含 {?,Spd,Lag,?,?}) —
  消费方可能持子结构指针读 disp8。
- **静态负结果 (强)**: 全 .text 扫描 (Reverse/scan_hitlag.py; movss/movups/movaps/VEX/
  整型 mov/lea, disp32+SIB 两形态) = **+0x1F0/+0x210 零内存读取** (0x1F0 的 6637 处字节
  匹配全是 `mov r,0x1F0` 立即数); disp8 文件布局假说 (+6F/+83 双读) 11 个候选全被反编译
  排除 → **消费方不直接按已知偏移读行**: 块拷贝进命中上下文 / 子结构指针 / 另有布局。
  行 vtable 0x142F12DE0 仅被 0x14027D3xx-0x14027D7xx 构造器簇引用; col ATK 节解析器
  (标签 "ATK\0" @0x14019b5265 → FUN_14019b5220) **不在 9-08 反编译导出里** (区段未分析,
  要看它只能 capstone 裸拆)。
- **下一步 = ATKTRACE 一枪定案** (ValorProbe 已装, ValorProbe.atktrace.txt 已设 73):
  GS 装备 → Shift+F9 → 全堆扫 vtable 找行 (多武器同 idx 取最后一个, **用 ATKHEX 行73
  +0x20 = 0x43840000 (264.0f) 验 GS 身份**) → PAGE_GUARD 20s → 打一发真蓄 →
  ValorProbe.log 的 RIP+偏移就是真消费链 (大怪/小怪各一发可顺带钉死 a/b 槽选择条件)。
  拿到消费函数后, DLL 强制卡肉 (自定义招免 col 行直接注入 hitstop) 就有了落点。
- **工具注**: Reverse/scan_hitlag.py — 从 .c 抽的 FUN_ 名是**完整 VA 不是 RVA**, 比较前
  先统一单位 (此前一轮"零命中"全是单位错位假阴性); disp32 扫描必须过滤立即数假阳性;
  lea rip-rel 按 [REX]8D[modrm&0xC7==0x05][disp32] 解码, 目标=下条指令地址+disp。
- **row 73 ATKHEX 局内实锤 (同晚)**: 守卫会话的 hex dump 显示 +0x1F0/+0x210 =
  0x3DCCCCCD (**0.1 = 当时部署的 HitLag 改值** ✓), +0x1EC/+0x20C = 0x3FC00000
  (**1.5 = HitLagSpd, 真蓄行不是 1.0**), +0x1E8/+0x208 = 0.02 — **REM Load 布局 =
  运行时布局定案** (MV@+0x20 等 ATKSCAN 读数随之可信)。静态"零读取"悖论加深: 数据
  确在 +0x1F0/+0x210, 但标量扫描零命中 → 消费方读法在已扫编码之外 (66/F2 前缀 SIMD、
  间接寻址、块拷贝) — 等局内 RIP 定案。
- **ATKTRACE 首轮 0 访问复盘 + 修复**: ① **VEH 自拆漏洞** — PAGE_GUARD 被 OS 在任意
  异常时自动清位, 旧代码对同页邻居行异常 (行 stride 0x2D0, 一页 ~5 行) 直接
  CONTINUE_SEARCH 不恢复 → 游戏读任何邻居行、甚至第二轮 ATKSCAN 的全行 dump 都会
  拆掉守卫; ② 双击两轮扫描叠跑, 第二轮重扫时行 73 已被流送释放 (148→143 行) 守卫
  落空。修复: 邻居异常也走单步恢复 + 计数 (session over 行报 neighbor faults)、
  会话窗 20→30s、扫描中/会话中重按拒绝、**热键 Shift+F9→F12** (裸 F9=CSharp loader,
  F10/F11=probe 自用 diff capture/直方图; F12=Steam 截图键, 附带截图无害)。
  新 probe 构建成功; 游戏运行中 DLL 被锁无法覆盖, 退出后拷
  ValorGauge/build/Release/ValorProbe.dll → nativePC/plugins/。
- **第二轮 ATKTRACE 复盘 (md5 6ab30d8f)**: 0 访问 + neighbor faults=1 → ① 守卫机制
  实际是**一次性**的: AtkApplyGuard 开头 `if (!g_atkGuardedPages.empty()) return;`
  让单步恢复永远空操作 — 任何第一条异常之后 guard 位 (OS 异常即自动清位) 永不回来,
  这工具从出生起就只可能记到第一条访问; ② 守卫着的行 73 在 30s 大剑战斗 (87/88/73
  TCS 连段, tracker 的 "Long Sword action changed" 标签是写死的, 门控只是结构判定,
  GS 动作照记) 中**零被读** → 命中路径不读行对象本体 (烘焙副本假说加强)。
- **ATKTRACE 重构 (md5 79bd84b4)**: ① AtkGuardRowPages 真正恢复 guard 位 (修 one-shot
  bug, VirtualProtect 失败也记日志); ② **多实例守卫** — idx 全部匹配实例 (≤8) 全部
  武装, arming 行逐实例报 mv (区分 LS/GS 同 idx 副本); ③ 邻居异常记 **ATKNEI**
  (rw/rel/rip/stk, cap 200) — 邻居 reader 的 RIP 是消费方线索; ④ **自检读** — arm 后
  自读 +0x20/+0x1F0, 应出现 2 条 ATKACC i0, 没有即机制坏 (SEH 独立函数防 C2712);
  ⑤ 单步恢复改 AtkReprotectAll (全部受守页重打 PAGE_GUARD)。
- **第三轮判读表**: 自检 2 条 ATKACC 出现 = 机制通; TCS 时出 ATKACC = 行就是命中期
  被读, RIP 直接定案; 只有 ATKNEI = 通用行遍历者 (loader/烘焙), 顺 RIP 挖; 全零 =
  命中读的是加载期副本 → 下一招 = 区加载前武装抓 bake 读。

### 2026-10-19 (5) 卡肉消费链全闭环: HitLag 行 → 实体字段 → 动作慢放 (全函数+偏移定案)

- **第四轮 ATKTRACE (md5 9ccf4aa6, HEX 扩到 +0xE00) 命中**: 327 条 ATKACC。行 73 运行时
  真被读, 但卡肉槽的读者是:
  - **安装器 = 0x141f75530** (未命名叶子函数, 紧贴 FUN_141f755c0 前, 跳表后;
    `InstallHitstop(rcx=实体, rdx=卡肉块指针)`, rdx = **row+0x1E0(a块) 或 row+0x200(b块)**,
    两块结构对称: +0x08=冻结倍率(a:fUnk16/b:fUnk19), +0x0C=HitLagSpd(a/b 同名),
    **+0x10=HitLag_a/b(秒)**, +0x14=fUnk17/20, +0x18=fUnk18/21。写入:
    实体+0xB2F8/+0xB2FC = 冻结倍率 (movss ×2), +0xB300 = HitLagSpd, +0xB304 = 0,
    +0xB308 = fUnk20, **+0xB310 = HitLag(主倒计时)**, +0xB314 = fUnk21(副倒计时),
    +0xB30C(byte) = 0。
  - **分发器 = FUN_142035720(实体, 行, kind)**: kind=1 → plVar6+0x3C(a块), kind=2 →
    plVar6+0x40(b块), kind=1/2/3 另调 **FUN_142039010(实体, row+0x228, row+0x230)**
    (uUnk22 + No_Sharpness 第二参数组)。kind 由消息发送方定: 玩家反应消息分发器
    **FUN_142042140 case 0x2B / 0x70** 转 {行=param_2[2], kind=param_2[3]};
    FUN_142033570 硬编码 kind=1。**a/b 业务语义未钉死** (大/小怪?) — 实用上无所谓:
    vanilla 两块恒等值, 自定义行两块都设即可。a/b 判别实验 = 一局内大怪小怪各打几发,
    看 ATKACC 出 off=0x1f0(a) 还是 off=0x210(b)。
  - **整行复制器 = FUN_14027d4d0** (经 FUN_1412da870 → 0x12DF537 → 0x1FBD244 调用),
    逐字段通读全行 (命中结算前打包工作副本)。
- **实体字段语义 (反编译定案)**:
  - **FUN_142131cc0 (动作步进)**: `步长 = dt × [实体+0xB2F8]` — **慢放本体**;
    +0xB2F8 = row+0x208 (TCS=0.02 → 2% 爬行, 普通招=0.0 → 全冻结)。
    **col 模板字段名纠正: fUnk16/fUnk19 = 卡肉冻结倍率; "HitLagSpd"(1.0/1.5) =
    +0xB300, 疑似收招追赶速度, 不是冻结倍率**。
  - **FUN_141f5f340 (tick)**: +0xB310/-=dt、+0xB314/-=dt (+0x68=dt), 归零 →
    +0xB2F8 复位 1.0。
  - FUN_1417c4090 (实体 ctor): +0xB310=0, +0xB2F8=1.0 初始化。
- **DLL 注入卡肉的路** (自定义招免 col): 直接写实体字段即可 —
  `实体+0xB2F8 = 冻结倍率(0=全冻), +0xB2FC 同值, +0xB310 = 秒数`, 结束由原生 tick
  自动复位。attacker/victim 双方实体都有这套字段 (玩家攻击时双方都卡)。
- 工具注: 该叶子函数 Ghidra 未命名 (.c 无 FUN 定义), bisect 归属会错标成前面的
  FUN_141f752d0 (它 0x141f75517 就 ret 了, 0x75518-0x7552F 是 switch 跳表数据);
  落在"未分析区域"的 RIP 要 disasm 裸验证。

### 2026-10-19 (6) 刚1 卡肉 0.6s 定案 + LS col 空闲资源表 + 无双斩弱命中音问题挂起

- **刚1(LMT 156)卡肉 = wp03.col ATK 行 49 HitLag_a/b → 0.4** (0.6 试过一版, user 判定
  太夸张 = 缺特效托底, 定稿 0.4 = 原版 LS 最高值/空中气刃突刺档; MHGU 逐帧参考 18帧
  /30fps = 0.6 仅作上限参考)。路由链 156 → node 89 → names[50] move=49 (居合赤) 不变,
  blend/LMT 零改动。**行 49 与 vanilla 大居合红 (311) 共享 — user 判定大居合红不在
  勇气太刀出招表里 = 事实上刚1 专用**。冻结倍率 fUnk16/fUnk19 = 0.0 (全冻结),
  MV 39/PartMod 1.0/MindsEye 1 保持。
- **LS wp03.col 空闲资源表** (61 names / 13 banks / 59 ATK, 后续加招用):
  空闲 names 节点 = **98 (names[56]) / 99 (names[60]) / 100 (names[46])** (90/94 已被
  无双斩占用); 空闲 ATK 行 = **31 / 50 / 54 / 58**。克隆模板 = 行 49 (居合赤,
  MV39/PartMod1/MindsEye1/fUnk14=50/uUnk12=5/bUnk23=43/fUnk26=270)。
- **待办: 无双斩两刀的"弱命中音 + 无卡肉"问题** — user 报告 42/45 两行的命中音是
  低伤害档音效, 且触发该音效的命中不发生卡肉。下轮对比 42/45 vs 49 的全字段
  (重点 fUnk13/fUnk14/uUnk12/Attack_Type/bUnk23 一类疑似音效/反应分级的字段),
  以及 fUnk16/19 冻结倍率 (0.1s HitLag 本身只有 3 帧, 可能"感觉不到"与"没有"并存)。

### 2026-10-19 (7) 无双斩迁入削除居合行: 弱音修复 + 卡肉 (user 方案: 居合出招表已废, 直接占用)

- **改法**: 343 路由 f0: 90→**91** / f58: 94→**92** (patch_timl_defense setval —
  setval 子命令仍在只是用法文本未列; expect_old 防呆)。node91/92 = names[52]/[53]
  "削除：居合気刃2白/黄" (开发组废弃空挂, vanilla 164 条 TIML 零引用, blend 扫描
  同样零引用)。**names[52]/[53] cgp 10→9** (保住无双斩调好的居合大判定盒; 手工
  偏移字节补丁: walk 0x18, name nul+1, 19B 定长块 cgp@+0, unk4@+15 做 assert 防呆)。
- **行 51 = hit1**: MV 31 / PartMod 0 / HitLag 0.2 (继承); **行 52 = hit2**: MV 150 /
  PartMod 2.0 / HitLag **0.35**。冻结倍率 fUnk16/19 = 0 全冻。
- **弱音根因候选 (42/45 vs 51/52 diff)**: 42/45 独有 **bUnk23=255** (真招全 40/43/44,
  疑似命中反应/音效表索引, 255=无效→回退弱音) + Flinch=0 + fUnk14=0 + MindsEye=0 +
  bUnk15=0/uUnk25=0。迁入居合行后全部继承强打击值 (Flinch7/fUnk14=50/MindsEye1/
  bUnk23=40)。若局内弱音依旧, 下一步对比 fUnk7(0 vs 100)/No_Sharpness。
- **⚠️ blend 警告**: MHW-Valor-0.0.2-hitlag.blend 的 entry343 **没有 CollisionTimelineObject
  轨道** (老血统副本; 343 的 CTO 当初做在 damage-modify blend + 部署二进制) —
  **从该 blend 导出会丢 343 路由**。343 的 TIML 一律以部署二进制 + patch_timl_defense
  为准, 或先把 CTO 轨道移植进 blend 再导出。
- 剩余空闲: node 93(行53)/98(行-1)/99(行-1)/100(行-1), 行 31/50/54/58。

### 2026-10-20 col_tool 写坏 col 事故 + 修复 + 无双斩终版 (hit1 気刃斬り声纹/hit2 居合)

- **⚠️ col_tool.py 两个写毁文件的 bug (已修, 用前确认已含修复)**:
  ① `cmd_setval` 的 `d[o:o+4] = new` — 对 u16('S'/'H') 字段 new 只有 2 字节, Python
  切片赋值**把文件删短 2 字节**且后续全部左移 (u8 会删 3 字节); 本轮 3 次 u16 写 =
  行 51 区域共丢 6 字节, 行 52+ 全错位。修 = `d[o:o+len(new)]` (两处)。
  ② `pack_val` 急切构建全类型字典 — float 字段写 >255 的值 (fUnk26=260) 会在无关的
  'B' 条目上 struct.error。修 = 按需打包。
  事故证据链: 字节 diff vs `wp03.col.bak-20261007-pre343mv` (每次改 col 前先 diff 这个
  基准可立刻发现同类事故); 坏档留证 = `wp03.col.corrupt-20261020`。
- **修复方式**: 行 0-50 (含全部 10-19 编辑 + 行 49 刚1 0.4) 从坏文件头部截取完好;
  行 51 起 (含 OAP) 整段取自 10-07 备份 (该区间无 10-19 编辑, 数据完好只是移位);
  拼接回 20408 后用修好的工具重打目标字段。
- **无双斩终版**: 路由 343 f0→91(行51)/f58→92(行52), 两 node cgp=9 (居合大判定盒)。
  **hit1 = 行51: 気刃斬り1(row15) 全字段声纹克隆** (fUnk13=2/fUnk14=6/bUnk23=12/
  fUnk26=260/Flinch=0/bUnk15=0/uUnk25=1/No_Sharpness=0/uUnk12=0/Cond_MV=26) +
  {MV31, PartMod0, HitLag 0.15, Energy_Gen=0 不加气刃}。**hit2 = 行52: 居合黄声纹**
  (fUnk14=50/bUnk23=40/Flinch7) + {MV150, PartMod2.0, HitLag 0.4}。
  声纹指纹表: 気刃斬り1=bUnk23:12/fUnk13:2/fUnk14:6/fUnk26:260; 居合気刃2系=40/0/50/-1;
  居合気刃1赤(刚1行49)=43/0/50/270; 削除居合気刃2赤(行53)=44。bUnk23 = 命中反应/音效
  表索引 (255=无效, 旧行42/45 携带 255 + 全零反应字段 = 弱音根因候选)。
  **user 局内确认: 42/45 版卡肉已生效但音仍弱 → 换行 51/52 后待复测**。
  行 22 (node85) 也是 bUnk23=255 — 若 bUnk23 定罪, 该行同样要换签名。
- **备份纪律**: 改 col 前先 `cp wp03.col wp03.col.bak-<日期>-<用途>`; col_tool 的自动
  .bak 每次覆盖, 事故后不可依赖; 10-07 的 `pre343mv` 备份这次救了命。

### 2026-10-20 (2) 343 路由入 blend + 一次二进制回退复盘 + FreeKinetics 链式结构教训

- **FreeKinetics 结构教训**: TIML Action Node 在树里是**链式串联** (动作节点互相串,
  只有链头直连 TIML Data Node) — 只扫直接连线会漏掉整条链 (本轮误判"343 缺 CTO"。
  正确遍历 = 从 Data Node 出发沿 input links 递归 BFS; 343 链上 9 节点含 CTO.083,
  156 链上 13 节点)。
- **blend 现状**: damage-modify 与 hitlag 两文件字节相同 (互相覆盖过); 用户认定的
  shout-config.blend **未验证/未编辑** (Blender 实际开着的是 hitlag)。343 路由
  91/92 已写入 **hitlag.blend** 的 CTO.083 (mB296CA66 双键, 把手同步) 并保存;
  shout-config 若是主本需开它重打同款两键。
- **二进制回退复盘**: 我 patch 部署 LMT (90→91/94→92) 之后, user 从 hitlag.blend
  (当时还是 90/94) 导出并部署 → 覆盖回 90/94。教训: **blend 导出部署会覆盖二进制
  patch; 两边改动要么同步进 blend 再导出, 要么导出后重跑 patch_timl_defense**。
- **闭环验证 (金标准)**: blend 全树导出 (`_e343_hitlag_export.lmt`, 临时件可删) →
  patch_timl_defense dump 逐轨 diff 343 = 与部署二进制**完全一致** (唯一历史差异即
  两个路由键, 重 patch 后 diff 零行)。343 全链 (col 行51/52 + names cgp + LMT 双键 +
  blend fcurve) 四层一致。
- **路线决定 (user 定案)**: **col 编辑保持独立工具路线** (col_research/col_tool.py,
  两个写毁 bug 已修), **不并入 FreeKinetics** — 插件专职 LMT/TIML; 卡肉/声纹编辑
  继续走 col_tool setval, 字段语义表见 (5)/(6)/(7) 各节。

### 2026-10-20 (3) 武器刀光(残影)机制定案: TIML PlMotionVisual 的 mFlag 位窗 (user 删轨 A/B + 全表普查 + 反射注册表)

- **定性: 刀光 = `nTimelineParam::PlMotionVisual` 轨道的 mFlag (m08FD20A6) 位窗**, 不是
  EffectParameter1。user A/B: 删 E106 (气刃二) 的 PlMotionVisual.079 → 刀光消失;
  EffectParameter1 (=踏步 FX, E345 已实证) 在场无效。与"身上特效走 ADD670 代码硬编码"
  是两套系统: 刀光是 TIML 数据驱动的**窗轨**。
- **反射布局** (注册 FUN_140f29260 / ctor FUN_140f293a0 对象 0x80B / 属性表
  FUN_140f29470; 名字串 0x1431D2718 起): mWeaponEfcType.mCore[0..2] @+0x60/64/68
  (type10 int), mWeaponEfcValue.mCore[0..2] @+0x6C/70/74 (type0xC float),
  mWeaponExternValue @+0x78。类是纯数据 (虚表只有析构/反射注册), 视觉消费者未钉。
- **LS 全表普查 (blend 264 个 PlMotionVisual 实例): 零实例写过 EfcType/EfcValue/Extern
  专属字段** — 太刀刀光完全由公共 mFlag 位窗驱动, 颜色/形态由武器状态 (气刃等级)
  运行时解析 (同 0xC13x 身体光 / weapon+0xAD4 常驻光环的"按等级选 id"模式)。
- **位语义** (统计 + action-list 对照; arr0=低16 位包/arr1=高16):
  - bit11 (0x800=2048) = **主挥砍刀光窗** (气刃系全带: 104/106/108/109/110/307-311/345)
  - bit3 (0x8) = **收势余光窗** (主窗后; 突刺 105 只有它 f20-25 短窗; 200-202 多段多窗)
  - 2056 = 两窗重叠过渡; bit14 (0x4000=16384) = **空中系刀光** (149-161 跳跃气刃;
    18432=+2048, 18440=+2048+8, 16392=+8); bit4 (0x10=16) = **拔刀/纳刀光**
    (2 拔刀 f0-37 / 3 纳刀 f40+ / 9 / 312 纳刀架势)
  - 普通攻击 (4-15 系) 无 PlMotionVisual 参数; 见切 140 也没有 (蓝光走代码)
  - arr1 极少用: E335/E351 值 2 (=全局 bit33)
- **移植配方 (迁移动作带刀光)**: 目标 entry 的 TIML Data Node 挂/改 PlMotionVisual
  action, 双窗对时 (2048=主发力段, 8=收势段; 空中再加 16384), 双通道 ix0/ix1 同步、
  SINE 插值、整数帧 (编辑铁律同 10-17)。**先例: valor 作者在 E345 上就是这么干的**
  (PlMotionVisual.095: 2048@16-40 + 8/2056@70-81)。
- **事件位侧证**: FUN_141f5a8a0 `test dword [player+0x8938], 0x2800` (bit11+13) =
  挥刀窗内延长某蓄力/居合时机阈值 (DAT_145011f58+0x10B8.. 参数表) — bit11 的 gameplay
  侧消费; **视觉侧消费者未钉** (下一步候选: ValorProbe 页守卫 PlMotionVisual 实例
  +0x60.. 抓读者 RIP, 或窗内标志块观测)。
- **EffectParameter1 补充定案**: E106 的 EP1.085 携带 FX id 3190(0xC76)/3196(0xC7C)/
  2004(**0x7D4** — 与 FXNOTIFY 身体特效同命名空间!) + 两个 0→1→0 使能窗
  (m6468B7B0 f7-25 / m8A66D69C f36-40) = 踏步/落地 one-shot, 非刀光。游戏有
  EffectParameter1-4 四类 (类型 id 0x6B32DCF6/0x723B8D4C/0x53CBDDA/…, .data 槽
  0x1451C9DFC-EA4)。
- **工具注**: FreeKinetics 的类名/哈希映射在插件 `timl_param_utils.py`
  (MFLAG=0x08FD20A6, MTRIGGER=0x99897C43, PlMotionVisual=m46F3E901, EP3 类=m053CBDDA
  — blend 里名为 EffectParameter1 的 action 字段表与它一致); `struct/TIMLTimelineParam.txt`
  = 游戏类型 id→类名全表 (PlMotionVisual=1190390017=0x46F3E901)。

### 2026-10-20 (4) 刀光/红光的第二重门: 动作运行时类 (user 交换实验 + Kijin 类族枚举) — 判别实验矩阵待做

- **user 实验**: 106 (气刃二) 与非气刃招交换 (动作+TIML 惯例) 后挥砍红光消失, 即
  PlMotionVisual.079 窗在非气刃招上**不生效**。user 假设: 气刃斩家族专属特效。
- **代码侧佐证**: 每武器 set-3 动作类表 0x142EF5C30 (113 槽, 槽位=内部枚举号, 与请求
  id/LMT entry 是**三个不同编号空间**; 槽名 ASCII 在对象 vtable+0x20, 槽 enum 名在
  entry+0x18 指针的 blob+0x20): **全部气刃/居合招 = 30 个 cKijin*/cIaigiri* 类**
  (Slash1/2/3, Stab, Upper, StepSlash, SpinSlash, JumpSlash/Twin/Triple+Landing,
  cKijin{1,2,3}ComboShot*, cIaigiri{Start,Loop,V,H,WpOff}); 普通招全为其他类。
  与 09-24 定案自洽: 红刃光 id (0xC134-7) 按"动作开始时等级"由**每招类的事件处理器**
  硬编码发射 — 换类即灭, 与 TIML 无关。**PlMotionVisual 窗 = 刀光拖尾的必要条件,
  动作类 = 疑似第二重门 (消费方疑似挂在 Kijin 类族的共享基类 update/begin 里)**。
- **vanilla 天然对照组**: 気刃突刺 LMT170 (Kijin 类) 的 PlMotionVisual.112 存在但
  **全零窗** (= 类有/窗无 → 应只有刃光无拖尾弧); 普通攻击 (4-15 系) 无参数
  (= 类无/窗无)。正好缺"类无/窗有"这一格 → 就是判别实验。
- **判别实验矩阵 (blend 改完局内红刃验证)**:
  - E1 **窗移植**: 给普通招 entry (如縦斬り系, 原本无 PlMotionVisual) 加 2048@主发力
    + 8@收势 双窗 → 出拖尾 = 窗充分, 交换实验另有混淆; 不出 = **类门实锤**。
  - E2 **运动无关**: 只把 106 的动作曲线换成普通招动画, TIML 原样 → 红刃光仍在 =
    类驱动 (预期), 窗内仍有拖尾 = 拖尾不依赖动作数据。
  - E3 **同族不同招**: 把 106 的窗原样抄到 108/109 等其他 Kijin entry 并对时 →
    应出拖尾 (数据可移植性对照)。
  - 注意: 用各自的正常输入 (或 F6 force {3,id}) 触发, **类跟请求 id 走, 不跟 FSM 路径**
    (09-19 动作层/FSM 解耦); FSM 编辑改不了类。
- 类表读法备忘: 表项 0x20 = {u32 槽位, u32 0, u64 classObjPtr(.data 0x144FExxxx 区),
  u32×2, u64 enumNameBlobPtr}; 类名 = *(classObjPtr) vtable+0x20。
- **(续) user R3 实验 (101↔106 动作+TIML 整包互换) 结果: 两个方向都无红光**。
  - 3:7 侧 (普通类 + 气刃二整包 TIML+动作) 无红光 = **类门实锤** (E1 判决格已补)。
  - 3:12 侧 (Kijin 类 + 縦斬り整包) 连刃光都没有 = **数据半边也要** → 统一模型:
    **红光/拖尾 = Kijin 类 (武装 FX 处理器) ∧ TIML 视觉数据 (窗驱动时机), 缺一不可**。
    09-24 的 104↔103 实验不矛盾: 那次两边都是完整气刃 TIML 包, 光跟类走。
  - **E2' 判决实验**: 现状 (106 槽=縦斬り动作+縦斬りTIML, 请求 3:12=Kijin 类) 下
    把 PlMotionVisual.079 双窗挂回 entry106 → 红光+拖尾回归 = 模型闭合 (动作曲线
    无关, 配方=类+窗); 仍无 = 存在第三重门 (转挖类 begin/update 的 FX 处理器链)。
  - **类表槽位语义补全**: entry+0x18 指针的 enum blob 可稳定提取 113 槽武器枚举名
    (Wp03::KIJIN_SLASH2@66 / KIJIN_SPIN_SLASH@68 / KIJIN_JUMP*@72-78 / IAIGIRI*@97-101 /
    **KIJIN{1,2,3}_COMBO_SHOT*@104-111** = 强化射击融合族, 属 Kijin 家族但独立子族)。
    注意槽位=武器枚举号, 与请求 id/LMT entry 仍是三个编号空间。
  - **345 (刚1/valor_1) 疑点**: user 称其属强化射击系; 345 的 PlMotionVisual.095 双窗
    在场却无拖尾 → 若刚1 请求 id 落 ComboShot 类, 说明拖尾消费逻辑可能只在
    Slash/Stab/Spin 子族; 判定法 = ValorGauge log 里看刚1 fire 的实际 {set,id}。

### 2026-10-20 (5) 红光发射链全解: weapon+0x2398 = "气刃消耗成功"标志 (wrapper 置位 → 发射器消费)

- **E2' 部署复核**: 部署 106 里的 PlMotionVisual 一直是 101 交换进来的 074 (f32=2048/
  f55=0/f57=8/f65=0) — **窗轨在 E2' 测试时从未缺席**, 阴性结果 (类+窗+非气刃包无红光) 有效。
- **FUN_142122f90 (RVA 0x2122F90, wrapper, (weapon, cost))**: bit0 门 (player+0x8938&1)
  → 先 `+0x2398=0`; **cost>0 且 cost ≤ +0x2368(气刃量) → `+0x2398=1` 并扣量**
  (FUN_142122410, cost^DAT_142f130d0 混淆); 动作 0x56(86=见切) 额外清空气刃量。
  ⇒ **+0x2398 (byte) = 本招气刃消耗成功标志**。
- **FUN_1421224d0 (RVA 0x21224D0, 1参=weapon) = 通用红光发射器**: player!=0 &&
  player+0xC&0xE && 动作!=0x45(69) && (**+0x2398** || (动作∈{0x57(87),0x66(102)} &&
  +0x2370(等级)!=0)) → ADD670(*(player+0x8808), 动作==0x66 ? **0x7DC** : **0x7D2**, 0)。
- **调用结构**: 每招 begin 处理器家族 (0x1417FDxxx/0x1417FExxx, 每招一个微型函数):
  写 anim 标记 (0xC06A/B/C/D/0xC070/0xC0C8-CA…) 到实例+0x30 → wrapper(cost=param表槽)
  → emitter → 公共尾 0x1417845E0。家族内变体: 0x1417FE9AB (0xC06D 招) **无条件多发
  0x7D3**; 0x1417FEB40 = [player+0x7900] 索引表 0x1433831A0 选标记 {0xC0C8/0xC0C9/0xC0CA};
  0x1417FEA58 cmp eax,1 二选一 {0xC06E, 0xC083}。另有一个调用方 0x1411AE58F
  (仲裁区, 先写 +0x2398=dh 再 call)。
- **+0x2398 生命周期**: move-end 处理器 FUN_142123510 清零 (0x1421234DB); 每帧更新器
  有 cmp (0x142123C0E); 0x14211BDF5 写 dword 0.8f (+0x2398 疑似 float 复用, 字节语义
  = 非零即开); 0x14182E650 mov byte,1 (某处理器置位)。
- **红光完整判据**: 招的 begin 处理器走 wrapper+emitter ∧ cost>0 ∧ 气刃量≥cost ∧ bit0。
  E2' 阴性 = 链条断在运行时某一环 (气刃量不足 / bit0 未开 / 该请求没走这个家族) —
  现有 GAUGEEVT+FXNOTIFY 日志一轮可定位。
- **内存点位 (DLL 用)**: ① `weapon+0x2398=1` → 走自然发射; ② 直接 call 0x21224D0
  (weapon) (只读发射, 无副作用); ③ ADD670 直接发 0x7D2/0x7D3/0x7DC — **F7 fx-test
  现成可试** (fx-test.txt `0 0x7d2`, Shift+F7 重载, F7 发射), 零改动先验"单发红光"观感。
- **拖尾 vs 闪光未分辨**: 刀光拖尾弧的真正 spawn 点待 FXNOTIFY diff (vanilla 気刃2 vs
  E2' 态, 一局) 定位; 若拖尾不在 ADD670 族, 下一钩子目标 = 0x142125150/0x142125860
  (挂点表特效取位, 09-20 纳刀研究)。
- **fx-test 已扩 (部署 md5 5b9b4384, 2026-10-21)**: func 2 = 直接调通用发射器
  (native 门照走); func 3 = 写 +0x2398=1; func 4 = 武装+发射 (决定性测试); func 5 =
  清零。地址键 `glowEmit=0x21224D0` (addresses.txt 可覆盖)。日志行
  `FXTEST: fire func=.. wtype=.. arm2398=..`。**部署现状: ValorGauge.dll 现役
  (FXNOTIFY 在 mod 构建里, 抓包不用换 probe), ValorProbe → .hold-20261021**。

### 2026-10-21 (2) F7 试枪结果 + trail 规则实装: 拖尾 = +0x2398 ∧ PlMotionVisual 窗 (+ 未定位的第三判据)

- **F7 结果 (user)**: ①func 0-3 (0x7D2/0x7D3/0x7DC 直发) = **身体红光, 武器无** —
  玩家emitter直发不含武器拖尾; ②func 4 (武装+发射) = 原生招 (上挑/袈裟斩/空中纵斩)
  红光+气刃声 (0x7D2 特效自带声), user 交换进来的动作无; ③func 5/2 (清/裸发射) 无反应;
  ④**func 3 (只武装 +0x2398) 后挥原生招 = 武器拖尾红光, 无气刃声** — 拖尾是纯视觉
  每帧路径, 消费 +0x2398 (更新器内 cmp 0x142123C0E 一带)。
- **user 交换 101↔345 判别**: 两包都带各自 PlMotionVisual (095/074, 部署 dump 实证
  参数在场) + F7 武装 → **101 包 (在 3:112 上) 有拖尾, 345 包 (在 3:7 上) 无**;
  删 074 后 3:112 也无 → **PlMotionVisual 窗轨是必要数据面**。
- **模型现状: 武器拖尾 = weapon+0x2398 (运行时门) ∧ 当前动作 TIML PlMotionVisual
  窗 (数据面) ∧ [第三判据未定位]**。第三判据候选: 3:7 (拔刀纵斩, user 已排除"拔刀
  状态转换" — 全程持刀测的) 的处理器路径缺失 / 刚1 的包内其它参数 / 刚1 动作的
  武器轨迹 (030/047 状态对未写入, 09-28 已知问题)。包差异清单: 345 有 mTrigger 2048/
  mFlag2 b15/mFlag b26(无敌)/mAttackAdjustAngleIndex/CTOバンク, 少 EP1 WorkNo02 与
  EventGroup03/05; mFlag 数值系完全不同 (101=0x90080000 系, 345=0x02400006 系);
  mFlag4: 101 恒 4, 345 5→4@f14→5@f100 (106 vanilla 也是 5→4@f16 且能拖尾 → 排除)。
- **trail 规则已实装** (gauge-rules 新类型): `trail <set> <id>` — 当前动作匹配时每帧
  写 weapon+0x2398=1 (字节级, 与原生 begin 置 1/move-end 清零生命周期兼容)。规则行
  `trail 3 112` 已写入配置; **新 DLL 待游戏关闭后部署** (运行中被锁), 旧构建 F7
  func 0-5 仍可用。
- **下一步判决实验**: ①武装 + 挥刚1 (3:112 自己槽位, 095 在场) → 有拖尾 = 收工
  (规则自动化即可); 无 = ②半交换: 刚1动作 + 101包 (只换动作不换TIML) armed →
  分离"动作 vs 包"。

### 2026-10-21 (3) 拖尾消费者全解: 效果 id 0x9C4 + bit3 门 (+0x2398) — 机制闭环

- **每帧仲裁块** (master updater 内, weapon=rdi, player=[weapon+0x9B0]):
  `gate1 = [*(player+0xE490)+8] bit3`; `bl = (+0x2398 != 0) || FUN_141F5AE10()
  || (动作∈{0x57,0x59,0x5A,0x5B,0x5C,0x66} && +0x2370(等级)>=1)`
  (第二判据 = FUN_141F5AE10: FUN_141BFF6F0()→FUN_14161A160(u,0x143EF3250)∈{1,2,3};
  白名单掩码 0x803D = 动作 87/89/90/91/92/102)。
  gate1&&bl → call 0x140AE0FF0(emitter) 开; 否则 0x140AE1700 关。
- **bit3/4/5 = PlMotionVisual 三条 WeaponEfcType 槽的激活位**: FUN_140F29920(obj,n)
  测 [obj+8] bit{3,4,5,13}(n=0/1/2/3) — 与反射字段 mWeaponEfcType.mCore[0..2] 对应。
  **窗口值 bit3(8)/bit4(16)/bit5(32) = 槽0/1/2** — 拖尾走槽0 (8 窗), 拔刀/纳刀的 16 值
  = 槽1 (另一种武器特效)。**2048(bit11) 窗不被此消费者读** — 之前的"2048=主拖尾"
  判断作废: 2048 是别的用途, 拖尾只认 8 窗 (user 实测"攻击帧末尾有红尾" = 075 的
  f52-60 8 窗, 完全吻合)。
- **拖尾本体 = 效果 id 0x9C4**: FUN_140AE0FF0(emitter): 槽([emitter+8]+0x1980)空则
  `FUN_140AE27E0(emitter, 0x9C4, 0, -1)` 生成并 keep-alive (同居合架势光 +0x1A40 模式);
  0x140AE1700 = 关/释放。r14 组件 = FUN_141F62090(player) = **\*(player+0xE490)**。
- **ribbon 跟刀骨 (MhBone_254 旋转) 走**: user 实测补 254 轨后"帧末有红尾"、补前
  完全无 (刀骨静止 → ribbon 零面积)。192_remap 缺 254/252 旋转轨 (250/251/253 在),
  已从 104 原样拷入 (机制验证版, 时序未重排)。
- **三条实现路线 (任选)**:
  1. **数据路线**: PlMotionVisual mFlag 窗写 **8** 覆盖想出拖尾的帧 + 刀骨有挥扫动画
     + 运行时武装 (+0x2398, `trail` 规则/原生付费)。
  2. **纯 DLL 路线 (无需任何数据)**: 每帧写 +0x2398=1 且置 [*(player+0xE490)+8] 的
     bit3 → 任意动作出拖尾 (含无 PlMotionVisual 参数的动作; 位可能被求值器逐帧刷,
     每帧重写即可)。
  3. **直接生成**: FUN_140AE27E0(emitter, 0x9C4, 0, -1) + 槽保活 (RVA 待建地址键)。
- user 已排除: 拔刀状态转换 (全程持刀测 3:7 仍无); mFlag4 数值 (106 同模式可拖尾)。

### 2026-10-21 (4) 拖尾闭环 user-verified + MCP 导出部署流程定稿

- **user 局内确认: 刚1 (3:112) 武器拖尾红光出现, 无需 F7** — 全链 = `trail 3 112`
  规则自动武装 +0x2398 + 095 窗 bit3(8) f16-80 + 192_remap 补 MhBone_254/252 旋转轨。
  三条件模型 (刀骨动画 ∧ bit3 窗 ∧ 0x2398 门) 局内盖章。
- **部署现场**: 全树导出 (blend = MHW-Valor-0.0.2-fx-edit.blend, 321 fcurves 的
  192_remap), 备份 = w03_00.lmt.bak-20261021-fx-pre345trail。**注意当前部署 =
  fx-edit 全树导出**, 与之前部署的差异不止 345。
- **导出错位事故复盘**: user 手工导出两次都带旧 095 窗 (部署 19:36/19:46 两版均
  f16=2048), MCP 从当前正确实例导出即对 — 判定 = 多 Blender 实例/导出部署文件错位。
  **定稿流程: MCP 在当前 blend 导出 → patch_timl_defense dump 验证目标字段 → 才部署**。
- **下一步**: ①254 扫掠轨按刚1实际挥砍帧重排 (现为 104 气刃1节奏, 拖尾形状随时差);
  ②给迁移 pipeline 加"刀骨扫掠轨"规则 (vanilla 同类挥招拷贝/合成, min-2-keys +
  归组); ③纯 DLL 路线 (强写 [player+0xE490]+8 bit3 + +0x2398) 作为零数据后备 —
  对用原生动作的自定义招即插即用。

### 2026-10-21 (5) 25x 家族语义修正 (user 观察修正"刀骨254"误判): 刀身路径辅助骨链 = 拖尾采样节点

- **user 修正**: 254 在肩颈处 (非刀骨), MHGU 骨架无此链; 251 = 挥刀时"前后左右上下
  跳"的纯平移骨, 不跟刀也不跟身。数据侧吻合: vanilla104 中 252.loc 87键 Y滑移
  0.81~1.36 (沿刀身轴向) + 254.rot 114键重度旋转 (rest 位 0,0.055,0.381 = 收刀时刀身
  中段位置), 251.loc 11键稀疏平移, 253 恒等, 250 loc+rot 全动画。
- **语义定案: 250→251→252→253→254 = 刀身路径辅助骨链**, 链末端合运动 = 沿刀身游走
  并随挥砍转向的参考点 = **拖尾效果 0x9C4 的采样轨迹源**。MHGU 动作此链全死 →
  参考点静止 → ribbon 零面积 → "迁移动作无拖尾"的最终根因。
- **迁移动作补链配方**: 252.loc (merge 原生 entry 时自带 ✓) + 254.rot (**必须补**,
  从 vanilla 同形挥招拷贝+按目标挥砍窗重排) + 251.loc (管线已带, 可不精调)。
- **192_remap 现状**: 252.loc 136键 (345 原生合并带, 时序=原 vanilla 刚1) /
  254.rot 104 节奏 (10-21 拷入) / 251.loc 6键 — 拖尾可见但形状时序混搭;
  待重排 254.rot (±252.loc) 到 192_remap 身体实际挥砍窗。
- 修正 (3)/(4) 中 "254=刀骨" 的说法。

### 2026-10-21 (6) 拖尾条件终版 (user-verified): 轨存在即可, 数值无关 — 迁移管线规则定稿

- **判决实验**: 254 旋转清成恒等单键 (CONSTANT, (1,0,0,0)) → 部署 → **拖尾仍在
  (user 局内确认)**。⇒ 254/252 的旋转轨**只要求存在, 数值无所谓**; ribbon 的实际形状
  来自武器模型本体路径 (手+挂点), 与辅助骨数值无关。
- **T2 无拖尾的真正根因 (翻案)**: bones.md 迁移规则 "254 stays absent" — 管线默认
  剥掉 254 轨 → 轨不存在 → 拖尾系统拿不到绑定 → 不可见。不是数值/时序问题。
- **拖尾三条件终版**:
  1. 动作含 254/252 变换轨 (**恒等单键即可**; 原生动作天生就有, 勿删)
  2. TIML PlMotionVisual bit3 (值 8) 窗覆盖想要的帧段
  3. weapon+0x2398 武装 (`trail <set> <id>` 规则 / 原生气刃付费 / 白名单 87/89/90/91/92/102)
- **迁移管线规则定稿**: 迁移动作 merge 后补 254 (+252) 旋转轨 (单键恒等, 归组,
  帧范围 0~帧数) — 已验证足够。252.loc 由原生 entry 合并自带, 254.rot 恒等即可,
  不需要从 vanilla 拷真实动画 (省掉时序重排工作)。
- **bones.md 语义对齐**: 252=头IK/254=颈IK (仅位置) — 但 vanilla 动作带 254 旋转
  114键动画 (用途未明, 与拖尾无关 — 恒等也出), 勿据此再推断"拖尾跟骨值走"。
  当前部署 = 恒等 254 版 (blend fx-edit 内同状态, 记得保存)。
- **(10-21 晚) 新建 TIML 参数的插件元数据坑**: blend 里新建 TIML action (脚本或手动)
  必须补 freehk 元数据, 否则导出器按 A_TIML_INVALID_ACTION_TYPE **静默剔除** (还会
  触发 ErrorHandler.append 3参 TypeError 崩导出): 必设 `starType="TIML_Action"` +
  `timelineParam`/`timelineParamRaw` = 类 id (PlMotionVisual=m46F3E901) +
  `timl_reorder="BLENDER"` + frameCount=-1/unkn0=0。照抄现有同类 action 的 freehk 即可。
  节点接线 = TIMLActionNode.output 'TIML Animation' → DataNode 多输入 'TIML Animation'
  (扁平, 无需链式)。新增 4 动作拖尾配置已部署: 339/340/343 新建 PlMotionVisual 全程
  8 窗 (.200/.201/.202), 342 的 141 窗 bit3 扩到 f26-60, 343 补 254/252 恒等旋转轨;
  trail 规则加到 105/106/108/109 (共 112/105/106/108/109 五条)。
- **(续) 拖尾窗收窄 (自动检测)**: 8 窗开满全动作导致收招段一直拖尾 (user 反馈)。
  修 = 手部骨 (012/057/008/040/017) 逐帧变化量剖面 → 峰值扩展 (20% 阈值/6帧容差)
  → 8 窗 = 挥砍段 + ~14 帧 linger。已部署: 339=f0-39 / 340=f0-53 / 342=f8-57 /
  343=f56-91 / 345(刚1)=f8-70 (**估的** — 重定向动作手部剖面太平, 自动检测塌缩,
  待 user 目测挥砍帧后微调; 095 尾部 2056 键已清成 2048 防 bit3 尾漏)。
  导出部署走 MCP 流程 (导出→dump 验证→拷贝), 部署值已逐 entry 验证。
- **(续) 无双斩双段窗 (user 定帧 24-38/58-72)**: 多段挥砍 = 多对独立 (起,8)(止,0) 键,
  CONSTANT 插值下段间自动断开, 无需段间基线键。MCP 导出已加 stdout 静音
  (io.StringIO 捕获, 错误日志仍写 _fx_export.txt), 输出不再刷屏。

### 2026-10-21 (7) FreeKinetics 新增 TIML 拖尾编辑器 (timl_trail_editor.py)

- **位置**: 插件目录 timl_trail_editor.py (照 timl_sound_editor.py 模子), __init__.py
  modules 表注册。面板: N栏 FreeHK → "TIML 拖尾" (3D/Dopesheet 双挂)。
- **UX = entry 驱动** (拖尾是 entry 级配置且 PMV 参数可能不存在, 与窗口/声音编辑器的
  action 驱动互补): entry 号 → 状态显示 (动作/254-252轨/PMV/bit3段列表) →
  **一键配置** (找/建 PlMotionVisual 参数含 freehk 元数据 + 补 254/252 恒等轨) →
  起/止帧加段 / 按序号删段 (多段支持, 无双斩双段模式)。
- **实现要点**: bit3 位平面编辑 = 保留其他位 (2048 等) 只动 8 位 (base(f)=v&~8 按
  片段取值), 双通道 ix0/ix1 同步 CONSTANT; 新建 PMV = 复刻 freehk 元数据配方
  (starType/timelineParam=m46F3E901/timl_reorder, 缺它 A_TIML_INVALID_ACTION_TYPE
  静默剔除); 树遍历 = LMTEntryNode→TIMLDataNode→BFS。
- **热加载**: 运行中的 Blender 用 spec_from_file_location 挂载 + 模块级 register()
  (包属性挂 sys.modules 包对象), 无需整包 reload; 重启后自动加载。
- **无头**: bpy.ops.freehk.timl_trail_setup/seg_add/seg_del/list(entry_num=...,
  seg_add 另有 frame_start/frame_end)。
- **DLL 侧不在本编辑器范围**: gauge-rules.txt 仍需手工加 'trail 3 <请求id>'
  (面板有提示行)。
- **(续) 工具 user-verified**: user 把 timl_trail_editor.py 复制到另一 Blender 实例
  (备份工程) 实际使用, 界面/操作/产物符合预期 — 编辑器在干净环境即装即用确认。

### 2026-10-21 (8) 屏幕震动机制定案: TIML PlMotionCommon 的 mCameraShakeIndex (+mViewParamId) — 大剑真蓄 A/B + 全表普查

- **定案 (user 删轨 A/B: 删 LMT190 的 PlMotionCommon 块 → 真蓄震动消失)**: 相机震动
  住在 TIML 的 `PlMotionCommon` 参数块 (类型 hash m48D6114F), 字段
  **`m252961CD` = mCameraShakeIndex** (震动档位), 次要 **`mF4D12F36` = mViewParamId**
  (视角/FOV punch)。字段名来自 FreeKinetics `blender/timl_controller.py` 的反射注册表
  (~line 593-870, 全字段可查)。**注意删整块会连带删掉同块里的 mFlag/mTrigger 窗**
  (190 的同块还带 mFlag=6/576→512 + mTrigger 2048 键) — 改震动只动 CameraShakeIndex
  键值, 别删块。
- **大剑真蓄 (w00 LMT entry 190) 配方 = 全库最猛**: shake 键 (LINEAR)
  `0@f0 → 43@f27(打击帧) → 32@f67 → 20@f95` + view `0@f0 → 14@f6 → 2@f108 → 0@f160 → -1@f291`。
  相邻档 41/42/43 = 同款渐强家族 (蓄力斩系 146/189 用 41@f0→18@f80 滑降) —
  **index 是"强度档", 曲线在档间线性滑降 = authored 的衰减**, 不是离散预设切换。
- **档位表 = csk 文件 (16B 头: "CSK\0" @4 + u32 数量 @12; 每条 120B)**:
  每武器一张 `hm/wp/wpNN/wpNN_shake.csk` (GS wp00=80 条, LS wp03=**56 条**),
  全局 `common/camera/shake_pl.csk` = 112 条。**索引超出武器表范围落全局表**
  (99 两个武器都在用且 > 各自表长)。档 43 在 wp00/wp03/shake_pl 三张表都存在但
  字节不同 → 分表解析 (局部 A/B 未做); 实用路线: LS 先直接用 43 (表内, vanilla
  从未用过 = 原生存在的强档), 观感不对再移植 `wp00_shake.csk[41-43] → wp03_shake.csk`
  同槽 (120B 条目跨武器同构, 可直接字节复制) 部署 `nativePC/hm/wp/wp03/wp03_shake.csk`。
- **LS (w03_00.lmt) 震动现状普查** (FreeKinetics struct 解析器, PLMC=0x48D6114F/
  CAM=0x252961CD/VIEW=0xF4D12F36): vanilla 居合系 306=30/307=32/308,309,311=36/310=15,
  気刃系 200-202=8/221=10; **valor 自定义 (332-334/343/345/347-349) 全是 99** (全局表
  环境档, 弱) — 这就是"当前招式没震动"的原因。view punch: LS 原生大居合 309/311 用
  `10@f0→11@f12→-1@f130`, 346 用 10→19 — LS 侧要 punch 优先复用自家 10→11。
- **落地配方 (待做)**: LS blend 里目标招 PlMotionCommon 块的 m252961CD 键值 99→43
  (起点放打击帧, 可复刻 43→32→20 滑降), view 可选加 10→11。无需动 csk。
- **FreeKinetics 节点图坑 (本轮踩坑)**: 全部 227 个 TIMLDataNode **同名**
  "TIML Data Node" (插件不加唯一名) — `nt.nodes[name]` 永远返回第一个, 遍历必须
  `bl_idname` 过滤 + 走连线 (`as_pointer()` 去重); **动作名后缀 (.179/.030...) 是
  Blender 数据块去重序号, 无语义**, 参数身份只看 fcurve data_path 尾部的字段 hash。
  LMT 二进制侧: action.timl 即 TIML_Data (有 .types); TIML_Type.timelineParameterHash /
  TIML_Transform.datatypeHash 定位字段; LMT() 解析需 `__len__` 的流
  (BytesIO 子类), 用 marshallHeader+marshallBody (parseFile 走文件路径且不填 timl)。

### 2026-10-21 (9) 343 屏幕震动落地 + blend 分叉事故复盘 (拖尾窗差点被洗)

- **343 震动已部署 (待局内验证)**: `m252961CD` 键 `(0,0),(68,0),(69,36→43),(90,36→43),(91,0)`
  全 LINEAR (1 帧沿=等效硬窗; guard 键防从头滑升)。
  档位 36 试过偏保守 → 已升 **43** (43 部署 md5 **88a85542**, 字节 diff 仅 2 处键值;
  36 版备份 `w03_00.lmt.bak-20261021-shake36`, 产物 `_shake343_export2.lmt`)。
  部署三处一致 (游戏 nativePC + 工作区 MHW-valor/nativePC + 导出产物);
  初版备份 `w03_00.lmt.bak-20261021-prescreenshake`。
- **(续) 机制修正: CameraShakeIndex = 命中打击档位; 挥空震的真凶 = mViewParamId**
  (user 局内观察: ①大剑真蓄**挥空也震** ②343 命中点在 f69 前也照震 ③自疑"命中震是
  这类攻击默认的" — 三条全对): 命中震 = 所有攻击自带的默认打击反馈 (与本曲线无关,
  所以 index 36/43 都"感觉没效果"); 挥空震来源 = 同块另一字段 **`mF4D12F36`
  mViewParamId** (真蓄 14@6→2@108→0@160→-1@291 挂满挥剑段, 删整块时被一并删掉
  才误判成 shake index)。view param id 跨武器共用 (大剑 275/276 与太刀 309/311
  都用 10→11) → 343 直接借真蓄 id 14。**已部署 (待验证)**: `mF4D12F36` 键
  `(0,0),(24,14),(95,2),(130,0),(165,-1)` 全 CONSTANT (166 帧 anim 缩放, 14 号
  相机状态挂 24-95), md5 **582191ba**, 备份 `w03_00.lmt.bak-20261021-shake43-noview`。
  若挥空仍不震 → 移植 `wp00_shake.csk[41-43] → wp03_shake.csk` 同槽 (120B 条目跨
  武器同构) 再测; 还不行 → view id 挂载期与动作状态的耦合需逆向 (mViewParamId 消费方)。
- **(续2) mViewParamId 14 = 相机距离状态, 非震动 (user 局内确认: 无双斩时相机移远了,
  无震)** — 真蓄的"拉镜头"配套效果, 保留在 343 上。挥空震嫌疑收敛到 csk 预设本身:
  **wp03_shake.csk 43 号槽 vanilla 从未被 LS 用过 = 大概率无调校的弱预设** (wp03[43]
  有内容但 ≠ wp00[43])。**已移植部署**: 工作区 csk (valor 作者改过: 原版 56 条 +
  追加 56/57 两条 = 本表 33/34 的原样复制品, 原 56 条未动; 游戏目录此前从未部署过
  csk, 一直吃 chunk 原版) 为底, wp00[41-43] 覆盖 wp03 同槽 (三槽 LS 无招引用, 41/42
  且 wp00 内本身同字节), 部署 md5 **7b304e5c** (游戏 + 工作区)。
- **(续3) csk 移植后挥空仍不震 → 数据逆向 + mTrigger 假说 (当前主嫌疑)**:
  二进制全镜像扫描: 0x252961CD/0xF4D12F36 **零引用** (引擎不按哈希消费字段, TIML 加载
  后按位置进运行时结构体); 0x48D6114F 仅 .data 注册表 0x145000ff4 一处 (行 = {name1,
  name2, id 0x0600005a, typehash, ptr1=0x1433C6B10 (4 函数指针 0x14023D590/
  0x141921E80/0x141921F20/0x141921F80), ptr2, default 0x144F7FD10}, 行长 0x30);
  CSK 魔数引用 = FUN_1419bd950 (序列化方向, count@+0xB0/条目数组@+0xC0,
  FUN_14028c2e0=单条目写), cCameraShakeManager 单例访问器 FUN_140289a30
  (vtable+0x38, edx=0xC40)。**mCamShakeTiming/Type/Index 三元组 (0x14324dae8/b08)
  邻居是 mIsHitWall/mIsHitGround = 命中反应相机震配置, 与挥空震无关**。
  **假说: 震动由 mTrigger 触发位点燃, CameraShakeIndex 只提供当时档位** —
  GS190 mTrigger = `2048@0 → 1024@44 → 16384@49 → 1024@104` (有位事件; 2048=无敌
  族别碰), **LS343 全零 = 从未点火** → 解释全部观察 (删块=删触发大剑没震; 只抄索引
  曲线=有强度没点火太刀没震)。**已原位补丁部署 (md5 2b91842d, 备份
  .bak-20261022-pretrigger)**: 343 mTrigger 键2个原位改写 f34→f69 val0→1024,
  f36→f74 (bit10 在 69-74 点火, 与 43 索引窗 69-90 重叠; 未动 blend — Blender 断连,
  定案后回填; 无效则试 bit14=16384, 再无效 → cCameraShakeManager 深挖 + 给 ValorProbe
  加手动点火热键)。
  **TIML 键帧文件布局 (原位补丁用)**: transform 结构在 types 数组顺序位置
  ({offset,count,datatypeHash,dataType} 24B), tr.offset 字段 = 键帧数组**直接起点**
  (无头!), 每键 20B = {value:i32, controlL:i32, controlR:i32, frameTiming:f32,
  transition:i16, dataType:i16}; bigflag 双通道在二进制合并为单 u32 = `(ix1<<16)|ix0`
  (0x02400006 = mFlag[1]=576,ix0=6 实证); transition 6 = CONSTANT。
- **(续4) mTrigger 假说 user-verified: 挥空震成功!** 343 的 `bit10@69-74 点火 +
  CameraShakeIndex 43@69-90` 组合挥空也震。**屏幕震动配方定稿 (可复制到任意招)**:
  ① `m252961CD` 键窗 (LINEAR, guard 键 1 帧沿): `(0,0),(窗起-1,0),(窗起,档位),(窗止,档位),(窗止+1,0)`;
  ② `mTrigger`(m99897C43, bigFlag 双通道) bit10=1024 点火 5 帧爆窗: ix0=`(0,0),(窗起,1024),(窗起+5,0)`,
  ix1 同帧全零镜像; ③ 不碰 bit11=2048 (无敌/no-hit 族)。档位表按武器 = `wpNN_shake.csk`
  (LS 表 43 号槽原生弱 → 已移植 wp00[41-43] 入 wp03 同槽, csk md5 7b304e5c)。
  **343 触发键已回填 blend** (mTrigger ix0/ix1 = SINE, 与二进制 transition 6 一致 —
  interpolationMapping[6]='SINE'; blend 原有键删除时 remove() 偶发静默失败, 改后必须
  重查键列)。**156 同配方已部署 (user 指定窗 6-16, 动作全长 70f)**: shake
  `(0,0),(5,0),(6,43),(16,43),(17,0)` + mTrigger ix0 `(0,0),(6,1024),(11,0)`,
  导出对照 diff vs 部署 = 仅 156, md5 **c7ad5ee1**, 备份 `.bak-20261022-pre156`,
  blend 已存 (= 拖尾 + 震动的完整真相)。
- **(续5) 343 拉镜头收窄到攻击窗 (user: 效果好但要在两刀攻击时才移远)**: mViewParamId
  键改为 `(0,0),(30,0),(31,14),(72,14),(73,0),(165,-1)` 全 CONSTANT (覆盖两攻击窗
  31-40/58-71, 73 回正) — md5 **3e717e76**, 备份 `.bak-20261022-viewfullswing`。
  ⚠️ 惯性坑再现: FCurve 键 remove() 静默失败率极高 (本会话第三次), **每次重写键列后
  必须重查并补删残键** (本次 f18/f95 残留, f95 的 2 号状态会污染收势段)。
- **(续6) 成果工具化: `timl_shake_editor.py` (FreeKinetics 新模块, 已注册 __init__.py)**:
  N栏 FreeHK → "TIML 震动" (3D/Dopesheet 双挂), entry 驱动。**一键配方** =
  ① shake 强度窗 (LINEAR+guard) ② mTrigger bit10 点火爆窗 (默认 5 帧) ③ 可选拉镜头
  (ViewParam 窗 + animLength-1 处 -1 标记; 起/止=0 跟随震动窗)。无头:
  `bpy.ops.freehk.timl_shake_setup('EXEC_DEFAULT', entry_num=..., frame_start=,
  frame_end=, grade=, burst=, view_on=, view_start=, view_end=, view_id=)` + `_list`。
  **验证闭环**: 编辑器重放 343/156 手工配方 → 导出 vs 部署 = 全文件**语义零差异**
  (字节差异仅 156 块内变换顺序 — 见下)。**实现要点**: 键列重写用**整条 FCurve
  remove+new** (逐键 remove 静默失败, 且成功 remove 后旧引用已死不可复用 — 首版即栽
  在这); 新建 PM 块沿用 trail 编辑器 freehk 元数据配方 (classid=m48D6114F);
  多 PM 块 entry (如 156 的 .365/.366) 优先选带 shake 通道的。
  **变换顺序与引擎映射定案**: 引擎按 datatypeHash 哈希映射 transform (顺序无关) —
  证据: vanilla 各 entry 顺序本就不一致 + 本编辑器重建后 156 块内顺序打乱仍语义等价
  (已部署验证过的配置不受影响)。**字节级 md5 对照从此只适用于未重排的 entry**,
  跨 entry 比对用语义 snapshot (排序归一)。
- **(续7) 工具干净环境验证 PASS (user 在旧工程 `fx-edit - 副本.blend` 实测)**:
  老工程 343 带遗留全零触发键 (f0/34/36)、156 无触发通道的初始状态下, 两发
  `freehk.timl_shake_setup` 全部命中: 遗留键正确清除、缺失通道正确新建,
  导出后 343/156 的 shake/trigger/view 六通道与部署金标准**逐键一致**。
  注意: 旧工程谱系不含拖尾窗 — **从旧工程部署前必须先过拖尾 diff**
  (见 (9) blend 分叉事故); 工具验证用的导出只做比对不部署。
- **⚠️ FreeKinetics 导出坑 (Blender 4.3)**: 含负值的通道被编码为 SINT,
  `checkControlTypes` 要求 controlL/R (= `Keyframe.back`/`.period`, Blender 4.3 新增
  逐键插值参数) 为 0 — **`insert()` 建的键带默认值 back=1.70158/period=4.1 直接导出
  报错**, 且报错本身在 errorController 里二次崩 ('ExportTIMLTypesNode' has no name),
  真实错误码 K_TIML_INVALID_CONTROL_VALUE_SINT 被吞。修法: 逐键 `k.back=0;
  k.period=0` (手柄类型/位置无关)。`.add()` 批量建的键恰好默认 0 所以 shake 通道没踩到。
  SINT 通道安全配方 = CONSTANT 插值 + back/period 归零。
- **⚠️ blend 分叉事故**: `MHW-Valor-0.0.2-screen-shake.blend` 分支自**拖尾部署前**的状态 —
  339/340/343 无 PMV 块、342/345 是旧窗; 直接导出部署会把 10-21 验证过的五招拖尾全洗掉
  (导出对照 diff 抓获)。**修复 = 把现网验证状态移植回 blend**: 339/340/343 用
  timl_trail_editor.ensure_pmv (含 freehk 元数据配方) 建 PMV + 全部五招 mFlag[0] 键值
  逐键刷成现网值 (339: 8@12-28 / 340: 8@24-40 / 342: 8@30-46+2048@26 / 343: 8@24-38+58-72 /
  345: 8@8-40+2048@72-80), 复导出 diff vs 现网 = **仅剩 343 shake 一处** 后才部署。
  **教训: 部署前必须做"导出 vs 现网"逐 entry TIML diff, diff 不干净不许覆盖**;
  另一实例里拖尾工具改完的工程 (fx-edit-with-tool 谱系) 才是含拖尾窗的最新真相,
  从旧谱系开新工作文件前先查 PMV 在不在。
- **导出流程定稿 (MCP 路线)**: 直接调
  `sys.modules['MHW-FreeKinetics.operators.export_ops'].exportNodeToFile(node, addon)`
  (绕过 freehk.export 操作符的 node-editor poll); 导出前 `globalCacheClear()` 已内建;
  node.filepath 设临时路径后还原; 每次导出必 Fresh 路径 (inject=True 时旧文件会合并)。
  导出日志 3MB [FIXED] 警告为历次常态 (valor_sheath_to_evade 等), 无害。
  大文件 diff 别在 Blender 里两文件同跑 (30s MCP 超时), 分两次 snapshot 存 JSON 再比。
  struct 包可独立 python 跑但 LmtBuffers 依赖 mathutils → 只能 Blender 内解析。
- **PMV 通道结构**: 现网五招 PMV 块都只有 `m08FD20A6` 单 transform (ix0); blend 侧的
  全零 mFlag[1] 通道导出时被丢弃 (tmp/现网一致), 镜像零键写法安全。
  TIML keyframe 的 transition 由 Blender interpolation 映射 (CONSTANT/LINEAR 均可导)。

### 2026-10-22 动作音效 ACTIONSND: 在动作的指定帧播自定义 WAV (无双斩 50 帧首例)

- **需求**: 气刃无双斩 (set 3, fsm id **108**, user 2026-10-22 钉死; gauge-rules 注释里
  "107=无双斩" 是陈旧信息) 使出后第 50 TIML 帧 (5/6 s) 播 `swo_fx.wav`, 且**触发那一刻
  玩家可能已被打飞 — 播放前必须确认仍在该动作中**。
- **实现 (mod build, 部署 md5 351e295e)**: `PollActionSounds` 挂 PropIntHook 每帧轮询
  (PollTrailRules 同款模式)。时间线 = **motion 时钟 `*(float*)(*(player+0x468)+0x10C)`**
  (TIML 60fps 帧, 动作/动作重启归零 — 与"第 n 帧"语义直接对应, 比 instance+8 秒线更贴):
  - **武装**: 当前动作 == 规则 && phase==2 && mot <= cue 帧 (迟到的首次观察不补播旧音);
    **mot 倒退 >1 帧 = 同动作再施放 (motion 重启) → 重新武装**。
  - **触发**: mot >= cue 帧 → 检查 set/id/phase 仍是规则目标 (被打飞=动作已换 → 取消,
    日志 `ACTIONSND cancel ... cue not reached`), 仍命中则 `PlaySoundA` + 日志
    `ACTIONSND fire act=.. mot=..`。
  - **播放 = winmm PlaySound (SND_FILENAME|SND_ASYNC|SND_NODEFAULT)**, 不碰 Wwise
    (10-19 残留教训 = 钩 Wwise 函数, 与播自定义 wav 无关)。路径经 StableActionSoundPath
    给进程级副本 (防 reload 换 vector 时 winmm 还在读路径)。CMake 两 target 加 `winmm`。
  - wav 预检: PlaySound 只吃 PCM (swo_fx.wav = 16bit/48kHz/mono/1.664s ✓); loader 对
    每条规则 `filesystem::exists` 检查, 缺文件在启动日志标 MISSING (播放时静默)。
- **配置 `nativePC/plugins/ValorGauge.action-sound.txt`** 每行一条:
  `<set> <id> <frame60> <wavPath>` (帧=TIML 60fps; 路径相对游戏根); `#` 注释。
  **文件缺失 = 内置默认规则 `3 108 50 nativePC/plugins/ValorGauge.sounds/swo_fx.wav`**
  (sheath-rewrite 惯例: 部署 wav 本身即启用)。Shift+F6 随全量配置链热重载。
- **部署物**: `nativePC/plugins/ValorGauge.sounds/swo_fx.wav` (源
  `sound-work/mhgu/wav/LS_common/swo_fx.wav`), 配置文件已写 (显式一条 3:108 规则)。
- **已知限制**: PlaySound 无音量参数 (响度不合适改 wav 本身); 单声道输出走 Windows
  混音器, 无 3D 定位/游戏内混音; 多条规则同时到点只有一个出声 (互斥打断, winmm 语义)。
  若 108 存在中途 LinkMotion 切换导致 motion 时钟在 50 帧前重置 (MOTIONTRACE 可查),
  武装检测会把切换当作重启、以切换后第 50 帧触发 — 局内 fire 日志的 mot 值可诊断。

### 2026-10-22 (2) 气刃槽积累/消耗全链路定案 (decompile+GAUGEEVT 全日志聚合; "106/108 谁扣的气刃"破案)

- **谜底: 106/108 的消耗就是 DLL 自己扣的** — 2026-10-04 的 valor-chain seed
  (`LoadGaugeRulesConfig` 代码内注入 105/106/107/108 cost 0.25 + fx 0x7D2) +
  部署 gauge-rules.txt 的显式 cost 行, DoAction 接受后经 g_originalGaugeAdd 扣 0.25。
  这些自定义 id **没有原生气刃处理器**, DLL 关掉就没人扣 — "启动 DLL 消耗/关掉不消耗"
  是 seed 的设计行为, 不是未解现象。注意 **GAUGEEVT 日志看不到 DLL 扣费行**:
  g_originalGaugeAdd 走 MinHook trampoline 绕过 detour — 这是日志里没有 -0.25 行的原因。
- **消耗总闸 = FUN_142122f90(weapon, cost)** (14 个每招 begin 处理器各传一个 w03p 槽位):
  ① 门 = event bit0 (player+0x8938 &1) 且 cost>0 且 当前气刃>=cost (cost==gauge 也扣), 全过才扣;
  ② 入口先清 weapon+0x2398, **扣费成功才置 1** = 拖尾/红光"已付费"的原生武装点
  (trail 规则注释说的 "kijin begin handlers (cost>0 && gauge pays)" 就是这里);
  ③ `(uint)cost ^ DAT_142f130d0` = **0x80000000 符号位翻转 = 取负混淆** (vadump 实锤
  `00 00 00 80`), 取负后才进累加器 — GAUGEEVT 看到的负数是干净的;
  ④ 扣完若当前动作==0x56(见切86): **直接写 +0x2368 = clamp(gauge-1.0, 0..1)** = 清空,
  绕过累加器 (GAUGEEVT 不可见), 并刷 +0x236C。气刃不足时 cost 分支整段跳过
  (不扣、不武装 0x2398), 招照样出 — 原生"付费失败无拖尾"的来源。
- **累加器 = FUN_142122410(weapon, val)**: val<=0 直加; **val>0 先乘
  func_0x1411c3420(*(player+0x7D20)) 管理器标量再加** (实测增益 0.1/0.11/0.115/0.132
  离散值的来源之一); clamp 0..1 写 +0x2368; 每次调用刷 +0x236C = playerParam+0x804。
- **增益源 = FUN_142124b20 = TIML 事件分发器** (GAUGEEVT caller +0x2124f13 = 它内部
  0x142124f0e 的累加器调用; **全游戏累加器只有 3 个调用方**: wrapper 消耗 /
  FUN_142123510 动作结束回填 / 本函数): 事件 type==7 (命中类) 时:
  - 增益 = 事件参数块(+0xB8)+0x260 基值 × **w03p 状态乘数四档 +0x874/+0x878/+0x87C/+0x880**
    (按 player+0x8940 bit17/18/19 优先选档, bit15|16→+0x880, 全无→常量 1.0)。
  - 动作特判: 91/92 → player+0x2D64 计数++ + FUN_1421225e0(兜割系);
    102(大居合) → 挂点表(0xC0 步长, 纳刀研究同款)最近行定位 + +0x2D24 = 0xC/0xD/0xE
    按气刃等级 + +0x5A4 = playerParam+0x8C0。
  - type 0x3B(59) 事件 = 清 weapon+0x239C 见切成功一次性锁存 ("动作开始处理器清锁存"的实现处)。
- **GAUGEEVT caller→语义对照** (日志 9429 行聚合): `+0x2122fdf` = 消耗 (wrapper 内部,
  所有招共用); `+0x212358d` = 动作结束回填 (见切成功 +1.0, 锁存消费); `+0x2124f13` = TIML 增益。
- **实测原生消耗 id 表** (本地玩家全日志): 0(-0.15/-0.2) 1(-0.2) 11(-0.15) 12(-0.15)
  16(-0.15) 67(-0.15) 68(-0.2) 69(-0.25) 70(-0.2) 73(-0.15) 74(-0.2) 75(-0.2)
  86(-0.1 见切自身槽, 随后被④清空) 94(-0.2)。见切处理器 FUN_1417fdae0 传 +0x82C 槽(实测 0.1)。
  **自定义 id 105/106/108/109/112 原生零消耗** (无处理器), 但**原生有增益** — 复用的
  原版 TIML entry (339/340/342/343 谱系) 自带 type-7 增益事件, 每跳 +0.075~0.11。
- **14 处理器 ↔ 槽位** (dump .c 全量): fdae0→+0x82C(见切), fdd30→+0x830, fe4c0→+0x824,
  fe530/fe5a0→+0x828, fe610→+0x808, fe8a0→+0x80C, fe900→+0x810, fe960→+0x814,
  fe9e0→+0x81C, fea40→+0x818, feae0→+0x820, fec10/fecd0→+0x834。处理器↔动作 id 终表
  可扫 set-3 类表 0x142EF5C30 (begin=vtable+0x30) 钉死, 未做。
- **w03p 磁盘加密定案**: 本地两份 (chunk-extracted G0 与 nativePC 禁用副本) md5 相同
  (f7fdc67e), 全文件高熵无明文 float (0.05/0.1/0.15/0.2/30/195 逐一扫过零命中) —
  **磁盘上是加密载荷, MHW Editor 打开时解密显示**。旧结论"编辑器看的是冰原版数据"存疑:
  加密本身就能解释"全文件搜不到数值"; 编辑器那份与本文件是否同源未另行验证。
  改数值 = 编辑器改完导出, 或 DLL 直接写运行时 playerParam 槽 (skew: file+871=runtime)。
- **控制面速查**: 自定义招改消耗额/免费 = gauge-rules.txt 显式行覆盖 seed
  (`cost 3 105 0.1` 改额 / `cost 3 105 0` 免费; fx 行独立, 显式行按 (type,set,id) 替换 seed);
  原生招免消耗 = `rewrite 3 <id> 0` (该动作期间所有事件归零 — 会连见切成功 +1.0 回填
  一起吞, 见切/居合慎用)。DLL cost 与原生语义差异: 直调累加器 → 无 bit0 门、无足额预检
  (可部分支付到 0)、不武装 +0x2398 (trail 规则另行每帧强写)。
- **3:77 跳跃气刃二连斩 配方 (2026-10-22, 已部署待局内验证)**: 招式家族 = 73/74 一段目
  (LMT155, begin 原生扣 -0.15/-0.2, wrapper→累加器路径) + 76/77 二段目 (LMT156, 共用类
  cJumpSlashLanding)。**用户感知的"77 消耗"实为一段目扣费** — 3:77 名下日志零负值
  (9429 行聚合 + 序列扫描: 3:77 事件间 -0.15 跳变 ×144 = 链式循环里别处扣的)。
  用户要"不消耗、打中才加" → 部署 `rewrite 3 73 0` / `rewrite 3 74 0` / `rewrite 3 75 0`
  (75 不在 action-list, 按同族 id+同款 -0.2 判定为一段目第三变体; 有异议删该行)。
  二段 77 的原生命中增益 (+0.1~0.22, TIML type-7) 不动 = 打中才加。遗留: 若局内发现
  挥空也涨条 (type-7 增益的命中门控 FUN_14210d350 只查实体+0x60 bit1/bit3, 非严格
  **局内实测 (2026-10-22): 无消耗 ✓ 但原生增益两路都不发** (空中落地/弹反 77 均
  零增益; 日志里 3:77 名下历史正值系链式施放误标/旧数据, 不足为凭) → 已切换
  确定性方案: `rewrite 3 76/77 0` + `gainhit 3 76/77 0.2` (76=二段另一变体, 与
  77 同播 156, 一并覆盖)。gainhit = NotifyPlayerHitConnected 内 timerhit 同块,
  DmgCalcHook 连击门控, 挥空永不触发, 走累加器 clamp 0..1, 日志
  `GAUGERULE gainhit act=.. +.. gauge=..->..`, id 0=通配。
  **gang1 干涉已修 (部署 md5 cf83213e)**: 弹反链 112→77 切换触发
  settle(left112, 绝对写 gauge=snapshot-0.15), 其后 1s phase-2 guard 对**双向**
  漂移都回退 — gainhit 增量会被吃。修法 = gainhit 落地时若 phase==2 同步上移
  g_gang1Expected (与 GaugeAddHook 负值记账镜像, 声明合法增量)。配置数值 0.2
  可按手感调; 一段目也要加气刃照抄 gainhit 换 73/74。Shift+F6 热重载生效。

### 2026-10-22 (3) 拔刀刚一 DRAWGANG1 状态无关化 + 携带态翻转机制定案 (user-verified)

- **拔刀刚一最终方案 (DRAWGANG1, 配置 draw-gang1.txt `<frames> [nohitSec]`)**: DoAction
  进来是气刃斩1 族 (3:11/3:12, **不看任何动作历史/收刀态集合**) + 红刃 + 气刃>0.15 →
  放原生拔刀动作 N 帧 (默认 1) → PollDrawGang1 中断强制 3:112。**携带态翻转 = 原生
  3:11 动作类职责, 1 帧足够 (user-verified 正常进入拔刀态)**。
- **030/047 状态对只编排视觉握持, 不驱动携带态 (本次证伪)**: 原版持刀动作 104/105 全带
  **常量对 (030.Y=2, 047.Y=1)**, 居合 109/113 才有中途翻转; 曾把 345 的换手键型改成常量
  (两轮: (1,2) 猜反 → (2,1) 贴近原版), 破坏了 192_remap 编舞里**本来就有的中段换手**,
  user 已自行修正回换手键型并部署。教训: 挂点对 = 动作编舞的一部分, 按"该动作该有的
  视觉"写, 与状态机无关。通道名 blend=MhBone_NNN / LMT=**BoneFunction.NNN** (查错名会
  误判 ABSENT)。导出坑再+1: 重建单键 fcurve 必须 f0+f142 双键 (单键使导出器 bounds()
  切空向量崩) + back/period 清零。
- **实现要点 (tick 诊断定位的两坑)**: ①收刀站立时控制器槽永挂 3:0 (drawn idle phase=2
  循环), R2 进**缓冲槽** (+0xBC/+0xC0) 不上 current → PollDrawGang1 同时盯 current+
  buffer, 缓冲有 3:11 就等它上 current 再按时钟触发; ②phase 0/1 = 接受未 begin 的管道
  状态不得解除 pending (1:0 按踩中该窗口误解除, 表现为气刃斩1 完整播出)。once-guard =
  控制器 current(+0xAC/+0xB0) 已是 toId 时丢掉第二发缓冲 3:11。
- **sheathed 集合退役**: 动作历史启发式 (lastSet==1 族 + {9,6} 收刀动画) 只剩给旧原地
  改写模式 (frames=0) 兜底; bootstrap 路径完全不看历史。**持刀待机 R2 (红+气刃够) 也出
  刚一** (user spec: 只要是气刃1就换), 气刃斩1 在气刃<0.15 或非红时照常出、命中攒槽。
  每次按下的 0.15 扣费 = gang1 记账按下即武装 (成功后即为刚一应有成本)。
- FSM 侧待機２条件直连方案已设计未实施 (bootstrap 覆盖后暂无必要); 试验性 FSM 节点 11
  曾致人物卡死 (ActionNo 11 = 收刀 R2 请求号, 请求按号匹配会进坏节点) — 加刚一路由只
  接线不建节点。

### 2026-10-23 GUARD 防御帧 + 特殊纳刀弹反两子系统整体移除 (user 定案: 转换已全转 FSM 内置)

- **删除理由 (user)**: 防御动作、特殊纳刀的转换已完全转向 FSM 内置, DLL 侧的两套
  转换处理没用了。被删的两个子系统:
  - **GUARD (guard.txt 防御帧借用)**: GuardOpenWindow/CloseWindow/OnActionAccepted/
    OnReactionAction 窗口机、GuardVitSetHook/GuardHpDeltaHook 伤害否决/回滚削伤、
    GuardStamDeltaHook 耐力回退、GuardTurnSetHook 转身钳制 (HkTurnSet/FUN_141F755C0)、
    NotifyGuardDamageHit 方向角兜底、GuardApplyBlockBenefits/ClearHealingQuota、
    DoActionHook 的 GUARDREACT 吞反应块、PollGuardWindow、LoadGuardConfig。
  - **sheath-counter (sheath-counter.txt 特殊纳刀弹反转换)**: SheathCounterRule
    (rule/drule/arule/krel/bluesim/mine 五种规则)、SheathArmorHitCheck (arule,
    DmgCalcHook 路径)、SheathDispatchHook (drule, HkHitDispatch 的 mod 侧安装 —
    mod build 不再装命中分发钩子, probe 的 HitDispatchHook 保留)、SheathArmHitRule/
    SheathWatchedAction、SHEATHREASSERT 吞反应、PollSheatheCounter (含事件位挖矿
    IFRMINE + F5 标记)、PollSheatheBlueSim、LoadSheathCounterConfig。
- **连带清理**: 5 个钩子签名表条目 (turnSet/hpDelta/stamDelta/vitSet/vitAdd) 从
  HookId/kHookSigs/kHookNames 移除 (sig_check.py 有独立 TARGETS 表不受影响; 部署目录
  无 addresses.txt 覆盖); VITSET/VITADD 追踪钩子随之退役 (HP 槽 2026-10-05 已钉死,
  发现使命完成); GuardVitalsBox/GuardWeaponOf 更名 **VitalsBox/WeaponOfPlayer** 保留
  (stamina/hp-lock/HPWATCH 还在用); g_vitalsHpSlotCustom 随免死钳制一起删除;
  vitals.txt 头注释的"+0x7630=锋利度"旧说法已被 10-05 实测推翻, 一并修正。
- **保留 (易混淆项)**: 见切/居合原生叮触发链 (bit15/bit250/NegrEvent/CondEval)、
  收刀拔刀刚一 SHEATHREWRITE/DRAWGANG1 (sheath-rewrite.txt/draw-gang1.txt, 现行拔刀
  刚一方案)、SHEATHWATCH (10-18 收刀序列状态沿日志, 武器位置工作用)、耐力系统
  (stamina.txt drain/cost/emptyExit — **注意**: 现役配置 drain 98 + emptyExit 98→收刀
  →1:324 喘息仍是特殊纳刀架势的 DLL 侧处理, 若 FSM 内置版已覆盖记得清空该文件)、
  probe 侧 HitDispatchHook/RXD/挖矿全数不动。
- 部署 ValorGauge.dll md5 **b3975fa4ebec4bd142faf2386ac77711** (两 target 编译零警告
  一次通过)。guard.txt/sheath-counter.txt 部署文件已成死配置 (无加载器), 留在磁盘上
  未删; SheathCounterRule 时代的 .bak 同理。

### 2026-10-24 无双斩343/刚一156 玩家"哈"喊声落地 (EG02 语音请求号定案 + blend/部署漂移修复)

- **配方 (依据 timl-editor/TIML_PARAM_REGISTRY.md 的 E345 三轮 A/B 终态, 全部 user 听过认证)**:
  **1409 = 拔刀起手喊** (345 f2 同款), **1019 = 挥砍/二刀喊** (345 f130 同款), 1119 = 中段喊
  (user 曾裁定删除的那声)。载体仍是 EventGroup02 (mTrigger 键帧=触发帧, 键值=ReqNo 行位掩码)。
- **落地**: 343 (无双斩) 新建 `EventGroup02.133` 行A=1019 + 触发 f56=1 (第二刀窗 f58-71 前夜);
  156 (刚一) 复用已有 `EventGroup02.095`: 新增行B=1409, 触发 f1(值1,1119) → **f4(值2,B=1409)**,
  行A=1119 保留不触发 (345 终态同构)。行轨 f0 单键 LINEAR/back=0, 触发轨 SINE (基线 f0 back=1,
  脉冲 back=0), 整数帧 — 复制 156 现成 EG02 action 再改键是最稳的新建路径 (freehk ID 属性自动继承)。
- **部署** w03_00.lmt md5 **46ff5e5da1407e6e9443c597069682b7** (3995604B, 整文件 blend 导出),
  备份 `w03_00.lmt.bak-20261024-0046-pre-shout`; blend=shoutout2 已保存。
  局内验证待 user (343 第二刀喊/156 起手喊)。
- **⚠️ blend/部署漂移陷阱 (本次差点翻车)**: shoutout2.blend 的 345 `PlMotionVisual mFlag`
  是 10-22 draw345 会话**二进制调参前**的旧值 (部署侧: f8=8/f16=2056/f40=8 + f70 归零键,
  blend 侧还多出 ix1 冗余 f24/f38 零键) — 直接整文件导出部署会**静默回退用户的震屏调参**。
  已把 blend 345 对齐到部署值。**铁律: 整文件 blend 导出部署前必跑全 entry TIML 语义 diff**
  (`timl-editor/_shout_semantic_diff.py`, 泛化自 patch_timl_defense.iter_tracks, 464 entry
  全对比; 433/435/437 两侧同样解析失败=历史盲区, 无害)。
- **坑**: blend fcurve 删键 `int(kf.co.x) in (24,38)` 首轮没删掉 f38 (co.x 明明=38.0, 原因
  未明), 换 `abs(kf.co.x-38)<0.5` 容差一次成功 — **删键一律用容差匹配**。
- **行序实证**: 新建 EG02 只追加 rowB fcurve (插入序在 A 后), 导出后 patch_timl_defense dump
  显示 ReqNo B=1409 + trigger f4=2 → **导出器按 hash 权威序 (A..F) 落位, 与 blend fcurve
  插入序无关**, 位掩码语义不受追加顺序影响。
- 本次 343/156 的两个自定义音效 = 10-22 ACTIONSND (WAV) 路线, 与本节 EG02 (游戏语音库)
  是两条独立通道, 互不影响。

### 2026-10-24 (2) 喊声 id 语义翻案 (user 听音辨认) + EG02 在自定义槽位可用定案

- **机制定案**: EG02 喊声在 156/343 这类自定义槽位**完全可用** — 二分实验 (把 345 的
  EG02.086 原块连行带触发整体拷到 156/343, f2 触发) 两个动作都出声了。
- **id 语义翻案**: 上一节"1409=拔刀起手喊(想要的那声)"是错的。user 局内听音辨认:
  **1409 = 前摇较劲声 ("哦", 拖重物那类)**, **1309 = 爆发"哈"** — 307 (小居合) 的 EG02
  就是单行 A=1309 双触发 {f7, f31} = user 听到的两声。1119/1019 语义未变 (345 时代结论),
  但本轮未重验。选声先对照 307。
- **第一版静默根因 (推定, 高置信)**: 当时的 156 EG02 是在已有 [行A, mTrigger] 后面追加
  行B → 二进制轨序变 [A, mTrigger, B], 位掩码若按 transform 序取行, 值 2 (bit1) 选中了
  **mTrigger 轨本身** → 无声。**铁律: EG02 的 ReqNo 行 fcurve 必须全部先于 mTrigger 存在
  (规范序 = 行A..F 然后 mTrigger); 给已有事件后补行 = 整个 action 从规范 donor 重建**。
- **终态**: 156/343 的 EG02 = 单行 A=1309 + mTrigger {f0=0, f4 / f56 = 1} (307 同款结构),
  blend 里动作名 EventGroup02.086.e156 / .e343 (拷自 345 donor)。部署 w03_00.lmt
  md5 **74f66412c4b1079faf5bdaf7123df90d**; 备份链 bak-20261024-0046-pre-shout →
  -shout-bisect → -shout-1309-final。
- 新工具: `timl-editor/_shout_semantic_diff.py <a.lmt> <b.lmt>` (全 entry TIML 语义 diff,
  dataType=1 轨), `timl-editor/_shout_eg02_compare.py <lmt> <entry>...` (EG 类原始结构对比,
  含 transition/easing 字节)。

### 2026-10-24 (3) FreeKinetics 声音编辑器升级 (喊声工作流沉淀进插件)

- **新能力** (timl_sound_editor.py, 活会话热重载已验证: 幂等/重排/新建三路径全绿):
  - `freehk.timl_snd_voice_add(entry, frame, voice_id)` = 一键给 LMT entry 的 EG02 加
    一次发声: 无 EG02 → 从 blend 现成单行规范 donor 拷 ID 属性新建事件并自动挂
    TIMLActionNode 进 entry 的 data node; 有 → 用承载该语音号的行加触发 (行缺失自动
    set_row + 规范化, 六行全满覆盖A行), 同帧同值幂等跳过。面板 "喊声快捷" 盒 = entry/
    语音号/帧 + 按钮, 不依赖 action 选择器。
  - `freehk.timl_snd_canon` = 规范化行序 (ReqNo 行A..F → mTrigger ix0/ix1 → 其余保序),
    键值/帧/插值/back/period 全保留; 行序非规范时面板 row_set 旁自动亮 ERROR 图标按钮。
  - `set_row` 新建行时自动调用规范化 (堵死"后补行落在触发轨后"的静默坑); voice_add
    在算掩码前也先规范化。
  - 语音号速查表 VOICE_ID_HINTS (1309=爆发哈/1409=前摇哦/1119=中段喊/1019=挥砍喊),
    行表/触发解码/报告全程带人话标签。
- 部署状态: 装机目录直接改源码并已热重载生效; `MHW-FreeKinetics.zip` 已重打包
  (旧包留 `MHW-FreeKinetics.zip.bak-20261024`)。Blender 重启后自动加载新版。

### 2026-10-24 (4) 无双斩343 第二段拖尾延长至 f100 + "FreeHK 只能加不能改/删"定案否定

- **变更**: blend (当前工作文件 = `MHW-valor-blender/MHW-Valor-0.0.2-efx-edit2.blend`, 比
  damage-modify 新) entry 343 `PlMotionVisual.144` 的 mFlag (FreeHKTiml.m08FD20A6) 双通道
  末键 f72→f100, 第二段拖尾窗 58-72 → **58-100** (段1 24-38 不动; 动作本体 166 帧, f100 在界内)。
- **能力定案 (user 问 "FreeHK 是不是只支持加?")**: 不是。三层都能删/改:
  ① 拖尾编辑器 GUI 本身就有**按序号删段** (timl_trail_seg_del, 见 10-21 (7)), "编辑" = 删了重加;
  ② 无头 op 同样齐: `bpy.ops.freehk.timl_trail_seg_add/seg_del/list`;
  ③ 最底层 TIML 轨就是普通 Blender fcurve (`FreeHKTiml.m<hash>`), 任意增删键/挪帧可直接
  bpy 编辑 (本次 = 直接挪 `keyframe_points[-1].co[0]` 72→100, 双通道同步, CONSTANT 保持),
  导出器按 fcurve 现状导出 — **UI 没暴露 ≠ 数据不能改**。声音编辑器的 set_row/canon 同理可删行重排。
- **验证**: 整文件导出 (`export_ops.exportNodeToFile`, 需先设 `scn.freehk_tether` = m_mesh
  Armature) → `_shout_semantic_diff.py` 对部署二进制: 167 entry **仅 343 一处差异**
  (mFlag 末键 72→100), 其余零漂移 (433/435/437 两侧同样解析失败 = 已知盲区无害)。
  导出器全树 [FIXED] 告警为历次常态, 与本次改动无关。
- **部署**: `nativePC/.../w03_00.lmt` md5 **6433adfe9798c77f7309cf54d5ffecda**, 备份
  `w03_00.lmt.bak-20261024-trail343-100`。DLL 侧零改动 (343 的 `trail 3 <id>` 规则与
  +0x2398 武装条件 10-21 已配好, 这次只是同一条 bit3 窗变长)。
- **注意**: blend 内存里已含本次编辑但**未保存**, user 需自行 Ctrl+S (沿用"导出自工作文件、
  保存由 user 决定"的惯例; 导出器对其他 action 的自动修复不随本次保存而定格)。

### 2026-10-24 (5) 拖尾面板"删除无效"复盘: 面板按钮接线 bug (operator 不传参 = 静默用类默认值)

- **症状**: user Entry=343 / 段=1 点"删拖尾段", 面板列表纹丝不动, 切 entry 再回来还在。
- **根因**: `_draw_trail_panel` 四个按钮 (一键配置/状态/seg_add/seg_del) 全部
  `layout.operator(id)` 裸调, 没把 `scene.freehk_trail_entry / freehk_trail_seg /
  freehk_trail_start / freehk_trail_end` 传进 op → Blender 按类属性**默认值**执行 =
  **永远作用于 entry 345 / 段0 / 帧24-38**。user 那一下实际删掉了 **345 的拖尾窗
  (8-70)**; 343 无恙。面板列表每次 draw 都现解析无缓存 — "看着没刷新"是因为 343
  的数据真没变 (变的在看不见的 345 上)。user 的操作本身全对 (现场字段
  Entry=343/段=1/起58/止100 就是证据)。
- **为什么 10-21 验证没抓到**: `_trail_validate.py` 全部显式传参
  (`entry_num=e, frame_start=s, frame_end=t`) 绕过面板; 当时"user-verified"那轮
  实际只踩过默认 345 路径。
- **修复**: 四键全部接线 (`op.entry_num = entry; op.frame_start/end = scene 值;
  op.seg_index = scene.freehk_trail_seg`), 装机源码已改 + 活会话热重载生效,
  仓库 timl-editor/ 副本已同步。**通用教训: 面板 operator() 必须显式传 op 属性,
  否则静默用类默认值; 其他编辑器 (窗口/声音/damage) 用的是安全写法
  (op 属性 default=-1 哨兵 + execute 内回退读 scene 属性), 无同款问题。**
- **事故恢复**: 345 用 `seg_add(entry_num=345, frame_start=8, frame_end=70)` 原样
  恢复 (bit3 位平面重写自动保留 2048@16-40 / 2048@72-81 两窗, 键 =
  (0,0),(8,8),(16,2056),(40,8),(70,0),(72,2048),(81,0)); 整文件导出语义 diff vs
  部署 = **零差异** (除已知 433/435/437 双侧盲区) → blend 与部署一致, 部署
  md5 6433adfe 未动, 无需重新部署。

### 2026-10-24 (6) 拖尾终态: 343 第二段 58-90 (user 用修好的 GUI 自助完成) + 修复闭环验证

- user 在修复后的面板上自助完成 改90→导出→部署 (部署 md5
  **5252ca41807f31152e1239f8bc259a38**, 02:05 与 blend 保存同分钟), blend
  (efx-edit2.blend) 已存盘。提交前验证: 部署 vs 基准导出
  (_trail343_fix345_export.lmt, 修345后的零差异版) 全量语义 diff = **仅 343 mFlag
  一处** (100→90 + 一枚 ix1 键), 其余 166 entry 零漂移。GUI 修复闭环 user-verified。
- 已知无害残留: 343 ix1 通道留 (100,0) 僵尸零键 — set_bit3_windows 对 ix1 不做
  同值去重且 old1d 帧集合永久并入 frames union, 纯冗余零值键, bit3 消费只看值
  不影响语义; 该 entry 以后再走段增删它仍会被保留 (继续无害)。

### 2026-10-24 (7) 勇气模式充能系统落地: gainhit 唯一充能源 + 无刃→白 + 满白→黄 (valor 入场)

- **设计 (user 定案)**: 复用气刃系统做勇气模式入场。普通态(无刃0/白1)所有命中充
  练气值 +0.1/跳, 勇气纳刀链上招(105/106/107/108, 同 10-04 seed 集) +0.3/跳;
  黄刃及以上 = 勇气模式, 充能降为 0.05/0.1。无刃时首次命中充能 → 直接转白刃
  (计时器回满); 白刃充满(>=0.999) → 自动升黄刃 = 进勇气模式。黄→红留给原版
  升级招 (回转斩族: FSM 練気ゲージ判定(大回転斬り) 门控 + 处理器
  FUN_1417e3c40/3cc0 做 level+=1+特效, **原版升级不刷计时器不清气刃量** —
  我们的白→黄刷计时器是对 vanilla 的有意偏离, 让黄刃入场永远是完整时长)。
- **user 修正 (关键)**: "气刃值满不会自动升级, 特定动作才升级" — 原版没有满槽
  自动升级, 所以白→黄必须 DLL 主动写 (已做)。白刃等级时长 user 自己改资源文件
  (w03p Gauge_Duration_White; 磁盘 w03p 是加密载荷, 编辑器改完导出)。
- **实现 (mod build, 3 处)**:
  1. `gainhit <set> <id> <amount> [valor]` 第 4 参 = 黄刃及以上时的金额
     (GaugeRule.hasValor/b); id 0 通配全 set-3 (FindGaugeRuleLoose 精确行优先)。
  2. `option charge-takeover 1` + **caller RVA 精准清零原生增益**: 累加器
     (FUN_142122410) 全游戏只有 3 个调用方 (10-22 定案), 其中正增量只有
     TIML type-7 命中增益 (FUN_142124b20 内部 call site, GAUGEEVT caller
     **+0x2124f13**) 和见切成功回填 (FUN_142123510, +0x212358d)。takeover 在
     GaugeAddHook 里只清 caller==0x2124f13 的正增量 (addresses.txt 可覆盖
     `gaugeTimlGain=0xRVA`) → 原生命中增益归零、消耗(负)/见切回填/regen 全透传,
     gainhit 成为唯一充能源, 配置数值即最终数值。rewrite 行优先于 takeover
     (rewritten 标志), 显式 rewrite 不被二次清零。
  3. `SetSpiritLevel(player, weapon, newLevel, why)` = 复刻 FUN_14211c5f0 的
     等级变更分支: 写 +0x2370 + **+0x2374=1.0 (计时器回满, 偏离 vanilla 升级
     不刷计时器)** + +0x2390 = playerParam+0x884 (免衰减宽限,
     Gauge_No_Decay_Time_after_Spirit_Gauge_Upgrade) + **刀光过渡 FX**
     g_auraSet = FUN_142110a90 (RVA 0x2110A90, addresses.txt `auraSet=`) 传
     (weapon, fxId, 0,0,0) 再锁 +0xad4 常驻色: 白 fx100/ad4 100, 黄 0x65/0x66,
     红 0x68/0x69 (id 全部抄自 14211c5f0 各分支)。命中路径 (DmgCalcHook/
     HitCharge hook 的 gainhit 块) 加 weapon+0x9F8==modWeaponType 门。
- **配置 (部署 gauge-rules.txt, bak-20261024-charge)**:
  `option charge-takeover 1` + `gainhit 3 0 0.1 0.05` (通配默认) +
  `gainhit 3 105/106/107/108 0.3 0.1` (链上招)。既有 76/77 显式 0.2 行优先于
  通配且无第 4 参 → 勇气态也是 0.2 (注释里给了改法)。`hit-charge.txt` 写 **-1**
  (旧蓝条→满充强制红刃系统退役, 防止和新系统打架双入场); gauge-lock 确认 `2 0`
  (关) 不干扰。旧 DLL 对新配置行安全 (4 参多余 token 被忽略 / option 键被忽略),
  但 takeover 需要新 DLL — **别在旧会话里 Shift+F6 热重载后再打一圈**(会双份充能),
  重启即干净。
- **日志**: `GAUGERULE gainhit act=.. +<amt> (valor) gauge=a->b` / `GAUGERULE
  takeover: suppressed native gain +x (act ..)` / `GAUGERULE spirit-level
  charge|charge-full: p -> n timer=1.0 grace=..s aura=fired|skipped`。
  验证路径: 普通攻击命中看 gainhit 行; 无刃打一下看 spirit-level charge;
  白刃充满看 spirit-level charge-full + 刀光变黄; 空挥看 gauge 不涨 (takeover
  只在 DmgCalc 命中路径加钱)。
- **新地址**: auraSet=FUN_142110a90 RVA 0x2110A90 (无直接 xref, 指针调用, 5 参
  形态抄 decompile); gaugeTimlGain caller RVA 0x2124F13。顺带澄清: caller RVA
  是 _ReturnAddress() - 模块基址 (GaugeAddHook 里已提升到函数头统一计算)。

### 2026-10-24 (8) 充能方向修正: 气刃值=计时器 0x2374 定案, chargehit 取代 gainhit 充能

- **user 局内纠错**: "原本 dll 3:84 打中积累气刃值, 现在 3:11 打中涨的是练气, 弄混了" —
  术语就此钉死: **气刃值 = 等级计时器 weapon+0x2374** (掉级倒计时条, 原 timerhit 3:84
  扩的就是它); **练气值 = 0x2368 蓝条** (气刃斩消耗/普攻回充/见切回填, 原版自理)。
  上一节 (7) 的 gauge-side 充能 (takeover + gainhit 通配/链上行) **整体作废**:
  takeover 代码保留但休眠 (配置不加 option 即关), gainhit 76/77 0.2 (10-22 确定性
  切换) 与充能无关, 保留。
- **新设计 (user 两问拍板)**: `chargehit <set> <id> <bar> [valorBar]` — 命中把**计时器
  按 0..1 条刻度**往上推 (不是秒!); 白刃(1)时条**跨越**满值 (prev<0.999 → next>=0.999)
  → SetSpiritLevel(2) 进勇气; 黄/红时充能只是维持条。**每次进白刃条清到 0.05**:
  无刃→白转换直接 timerValue=0.05 (SetSpiritLevel 加了 timerValue 参数, 黄/红入场仍
  =1.0); 黄/红→白掉级由 PollValorLifecycle 掉级沿 (current==1 && previous>=2, 每帧
  Present 线程) 把原版回满的条压回 0.05, 日志 `GAUGERULE valor-recharge`。→ 条只能
  靠打攒满, "天生满条无法触发跨越"的结构问题被堵死。
- **部署**: gauge-rules.txt (bak-20261024-charge 之上再改, 通配 `chargehit 3 0 0.1
  0.05` + 链上 105/106/107/108 `0.3 0.1`); **timerhit 3 84 3.0 已删** (被通配
  chargehit 覆盖, 留着双份充条); hit-charge=-1 维持。DLL md5 5e2e8c65。
  g_modWeaponType 定义前移到 g_lastObservedSpiritLevel 旁 (PollValorLifecycle 现在也
  门它, 原声明在 900 行区太靠后)。
- **日志语义变化**: `GAUGERULE chargehit act=.. +0.1 bar=0.15->0.25` (条刻度);
  `spirit-level charge: 0 -> 1 timer=0.05`; `spirit-level charge-full: 1 -> 2 timer=1`;
  `valor-recharge: dropped to white, bar 1 -> 0.05`。练气槽不再出现 DLL 加行 —
  它回到纯原版节奏。
- **遗留对齐点**: 黄→红升级招 (回转斩族) 吃的是**练气槽满** (0x2368, 原版门控) —
  勇气态内练气槽照原版节奏走, 所以红刃路径无改动; user 若想红刃也走条充能再说。

### 2026-10-24 (9) w0Xp 参数文件容器破解 (Blowfish) + Guard Angles (Shared) = 引擎侧真格挡的数据面配置

- **容器格式定案**: w0Xp = **Blowfish-ECB 加密**, 全 14 武器共用 key
  `j1P15gEkgVa7NdFxiqwCPitykHctY2nZPjSaElvqb0eSwcLO1cOlTqqv`(55 字节截到 56),
  加解密前后各做一次 u32 BSwap; 明文头 = `W03\0`+u32 版本(0x14) = 模板的
  Weapon[4]+uint, 其后即 .bt 模板紧凑布局 (BinaryReader 顺序读, 无对齐)。
  证据: MHW-Editor.exe 反编译 (ilspycmd 8.2.0 + DOTNET_ROLL_FORWARD=Major,
  完整副本在 `_mhweditor_src/`) — `EncryptionHelper`(BlowfishEngine+BSwap)、
  `Wp03Param.cs`(逐字段 LoadData/WriteData)。此前"编辑器显示值在文件里搜不到/
  195/135/60 搜不到"之谜全部解开 = 密文。**工具: `w03p_tool.py`**
  (dump/guard/hexdump/setguard, auto .bak + attrib 解只读, 依赖 pycryptodome)。
  nativePC 文件与 chunk-extracted 两份密文相同 (历史结论复核成立)。
- **Guard Angles (Shared) 表 = 文件偏移 0x1B5(437), 2×7B 条目**
  `{f32 Guard_Angle_Both_Sides, u8 Knockback_Threshold_S/M/L}` (entry1 @444)。
  LS = 90.0/14/39/40 ×2 (user 编辑器截图逐值吻合)。全武器对照 (w03p_tool.py guard):
  GS 70/90°14/39/40, SNS 60/90°**14/20/21**(小盾低阈值=易大后仰), DB/LS/HAM/HH/SA
  = 90°14/39/40(无盾默认档), **LANCE 110/180°39/49/50, GL 110/360°39/49/50**
  (大盾宽角高阈值), CB 70/70° 但 entry1 阈值不同(24/49/50)。
  → 语义 = **引擎格挡方向锥(度) + 后仰小/中/大伤害阈值**; entry1 = 第二状态
  (待钉: CB 阈值不同→疑似斧形态, GL 360→疑似某种全向态; 也可能是防御强化/Guard
  技能档)。
- **运行时消费链 (静态证据链闭环, 运行时偏移待局内 A/B)**: 参数表 =
  `*(*(weapon+0x9B0)+0x7810)` (getter FUN_14211c060)。angle 以度消费
  (解码 FUN_142178100/142176240(player+0x230, +0x7870/+0x1510) 出弧度, ×
  DAT_142f16ec0=57.285 转度, DAT_142f16ec8=180.0):
  - param+0x940/0x944 与 +0x948/0x94C = 两组 float 对 = 两个条目的角度窗
    (FSM 条件求值器 FUN_140f0f830/FUN_140f0f8e0: deg∈[min,max];
    FUN_1418247d0(发 event bit 0xC5+99 的动作类): deg∈[min,max) 严格;
    FUN_1417e4260: event bit9 门, deg∈[min, 180−max] 分类前/后向选动作 id
    (param_1+0xcd8/cdc/cd4) 并把角度写 player+0xe1b4)。
  - param+0x950/+0x954 = 两个 **u32 阈值三元组** {S,M,L} 字段打包
    (LS = 0x0028270E), 动作类 begin (FUN_1417d8d00 族, 按 FUN_141f62070 返回块
    +0x28 bit4 门) 经 **FUN_141f72330** 装到 **player+0x14B0 / +0xE178**
    (+0xE174=激活字节; FUN_141736ab0/14173de50 等武器防御类按状态重装/清零)。
  - 加载器把 7B 文件条目扩展成 min/max 对 + 打包阈值 → **文件→运行时不是全文
    +871 平移** (+871 只对 Params(5) 簇 file 1177-1317 → runtime +0x800-0x888
    验证成立; guard 表 file 437 平移会落到非对齐地址, 说明有重排)。
- **翻案之翻案**: 10-05 (9) "param+0x940/0x944 实测 [1200,0] 非角度" 的测量对象
  是 **player+0x7810 = 玩家位置缓存** (玩家对象自己的 +0x7808-0x7818 是位置拷贝区,
  FUN_142018a50 写点; X=1200cm 量级), 偏移撞号误测。参数表 +0x940 角窗结论恢复成立。
- **与本项目防御体系的接口**: 306 特殊纳刀强格挡 (TIML b30+mFlag1b28) 让引擎把
  命中按格挡处理, **方向锥与后仰档就吃这张表** — user 10-14 局内实测的
  "真·带角度防御 (背后被打直接击飞)" 的锥角即本表 LS 的 90°。想改 LS 真·格挡:
  角度改宽 (90→180 近全向)、阈值抬高 (Large 40→255 永不大后仰)、或抄长枪档
  (110°+39/49/50) — **纯数据面, 无需 DLL**; MHW-Editor GUI 与 w03p_tool.py
  两条路都能写。改完放 nativePC 部署同名路径即可 (Blowfish 回加密工具已封装)。
- **新造"能转换的防御类"只差 FSM 接线 (2026-10-24 对现部署 wp03_action.fsm 的盘点,
  xfs_parse.py 解析 98 节点)**: 反应节点**全部已存在且全部零出边(停车位)** —
  act34 弾かれ=node43, act35/36/37 防御=node95/96/97, **act38/39/40 强化防御=node91/92/93**。
  请求进入不需要入边 (零入边节点按 ActionNo 匹配请求进入, 09-19 模型), 所以強G 三件套
  打出 {3,39} 时 node92 已在被进入、播完 38-40 的反应动作后无处可去 = "格挡成功但 FSM
  不切换"的完整解释。**新防御类配方 = 任选载体动作贴強G 位 (306 现役 0x52400006 +
  mFlag1 b28 + staminaParam; 禁 b26 同窗) + node91/92/93 加出边指到反击动作 (条件可选,
  无条件=自动转换) + w03p Guard Angles 表调锥角/阈值**。三档可分派不同反击 (91/92/93
  各自出边), 条件可做红刃门 (VariableNode)。- **条件属性表速览 (2026-10-24, vadump 0x1430ba000+0x6000, UTF-8)**: 全武器 FSM 条件
  属性注册表里存在 **`アクション`** / **`MotionNo`** / `CheckRequestFromMotion` /
  `フェイズ0/1/サブ1` / `攻撃ヒット` / `レバー(方向)` / `遷移可` 等未在 wp03 使用的属性
  — 若 VariableNode 可绑 `アクション`/`MotionNo` (FSM-Studio 属性下拉 30s 可验), 反应
  节点 (91-93/95-97) 的出边就能**按来源动作分派不同反击** = 同一组位组合承载任意多
  个行为上独立的防御类; 第三类的解锁点在 FSM 路由层, 不在位组合层。

  (Wp06 cChargeCounter*, LS 抄位不转换, user 已实测), 不要走那条。
- **遗留**: ① 运行时 +0x940 区实际值 dump 一眼定案 (预期 −90/90 或 90/90 形态 +
  0x0028270E ×2; ValorProbe/LuaEngine 均可); ② entry1 触发条件; ③ 后仰档
  S/M/L 的具体消费点 (player+0x14B0 字节 vs 攻击记录 record+0x248/24C 阈值的
  比较处, 10-05 的后仰档链) 未逐指令钉死。

### 2026-10-24 (9) "没检测到特殊纳刀"破案: 纳刀链第一招 = ACT109 纳刀一文字 (FSM 部署件全表实锤)

- **user 报告**: 特殊纳刀后接的招式没吃到 0.3 大额充能。**根因 = (8) 的链上招 id 配错**:
  用的还是 10-04 seed 的 {105,106,107,108}, 但特殊纳刀 (ACT98) 的直接派生是 **ACT109
  纳刀一文字** — 不在行里, 只吃了通配 0.1。
- **部署 FSM (wp03_action.fsm) 勇气链全表** (xfs_parse.py 提取; mpProcessList 是单容器
  对象, ActionNo 在 mpParameter.ActionNo.EnumValue — 之前脚本按 list 扫所以全空):
  [61] ACT98 勇气纳刀 (==特殊纳刀, user 混用同义) 出边: '一文字'→[72] ACT109 纳刀一文字 /
  '气刃踏步斩'→ACT70 気刃踏込斬り / 左右移动斩→ACT82/83 / 斬り下がり→ACT84 /
  '松开R'+'体力耗尽'→[90] ACT6 收刀。
  [72] ACT109 → [87] **MOT118** '一文字接上挑' (MOT 注入, 动作层 id 仍=109, chargehit
  照样命中 109 行!) 或 → [74] ACT112 刚一。
  其余自定义 ACT: 104 気刃１スリF / 107 気刃２スリF / 110 気刃３スリF / 111 気刃３スリL /
  106 一文字短 / 105 一文字长 / 108 气刃无双斩 / 112 刚一 / 38,39,40 强化防御 / 100 收刀回避。
  MOT 节点: 357 大居合后登龙 / 358-360 右上右下左上一文字短 / 361-363 同长 / 364 小居合后
  登龙 / 118 一文字接上挑 — MOT 段动作层停在前一招 id, 充能/规则按宿主 id 命中。
- **修复 (纯配置)**: chargehit 行换成 `3 109 / 105 / 106 / 108 = 0.3 0.1`; **107 删行**
  (滑步气刃, 非纳刀链, 回落通配 0.1)。可选行注释在文件里: ACT70 (架势踏步斩, 原版招别处
  也出, 按id配=全局大额) 和 ACT112 刚一要不要大额, user 定。DLL md5 5e2e8c65 未动,
  Shift+F6 热重载生效。
- **教训**: "勇气纳刀后接的招式"这类 **按语境**的需求, 落到实现上是**按动作 id**的 —
  id 表必须从**当前部署的 FSM**现挖, 三周前 seed 的 id 集合不可信 (moveset 已大改两轮)。
  MOT 注入段不换 id 是双刃剑: 规则自动覆盖后继段 (便利), 但 MOT 招永远拿不到独立身份。

### 2026-10-25 语境充能定稿: chargehit-from (前一动作==纳刀98 → 大额), user 点破 MHGU 语义

- **user 需求**: 勇气纳刀接的移动斩 (MHGU) 增长比普通移动斩高很多 → 必须区分"是不是
  纳刀后接的", 纯 id 行做不到 (82/83/84 移动斩、70 踏步斩与普通连段共享 id)。
- **实现**: `chargehit-from <set> <prevId> <amt> [valorAmt]` — 命中时读 **controller
  +0xC4/+0xC8 (前一动作, 游戏动作层在真实转换时自己写, 09-26 控制器全图字段首次实战)**
  , prev==规则目标 (3:98) 则覆盖金额。优先级 from > 具体招行 > 通配; 日志 chargehit 行
  新增 `prev=3:<id> src=from|move` 供局内验证 +0xC4/+0xC8 语义。chargehit 块只在有
  chargehit 行匹配时才跑 (通配行须保留)。部署配置 `chargehit-from 3 98 0.3 0.1`。
  DLL 部署由后台任务等游戏退出完成 (构建 md5 见任务日志)。
- **语义注意**: prev 在缓冲衔接时会提前翻页 (82 早期帧就 accept 了后继 → prev 变 82),
  多段招尾跳吃不到大额 — 可接受, 原版取消系统同款语义。

### 2026-10-25 (2) chargehit 矩阵化: @语境单元格 + 四档解析 (user: 纳刀后继各招应有专属增量)

- **user 反馈**: 纳刀后接的各招在 MHGU 里各充不同的气刃值, 一个 from 大额不够 → 重设计。
- **新语法**: `chargehit 3 <id>@<prevId> <amt> [valorAmt]` — 语境单元格, 只有"上一招是
  3:<prevId>"时该招才吃这个额。四档解析 ResolveChargeHit: **ctx 单元格 > chargehit-from
  默认 > 普通招行 > 通配行** (from 压过普通招行是有意的: 语境提升就是要抬高普通招)。
  GaugeRule 加 hasCtx/ctxPrev; id token 含 '@' 时 split。日志 chargehit 行带
  `tier=ctx|from|move|wild prev=<set>:<id>` 供逐发核对。
- **prev 语义补充定案**: controller+0xC4/+0xC8 由游戏在**真实转换时**写, 缓冲 accept
  不动它 — 比 DLL tracker (accept 即翻页) 更贴"前一招"语义, 命中时现读即可。
- **部署**: DLL md5 ea7ef559; 配置 chargehit 段重写 (四档说明 + 纳刀出边清单 +
  5 条注释态 @98 示例行), from 3:98 0.3/0.1 与 109/105/106/108 普通行保留。
- **配置即调参面**: 整体改纳刀语境 → from 行; 单招纳刀档 → @98 行; 某招普通档 →
  普通行。全 Shift+F6 热重载。

### 2026-10-25 (3) 100 收刀回避充能 + 纳刀出边真实请求 id 钉死 (user 调参实测)

- **user 补充**: 勇气纳刀(98)格挡转入收刀回避(100, LMT312)也要回气 — "看似格挡成功才
  回气, 但 100 全游戏唯一入口就是 98, 按 id 无条件回 = 等价格挡语义"。**配置行落地**:
  `chargehit 3 100 0.15 0.08` (先照袈裟斩档, user 自调)。312 有 A/B 两段攻击判定窗
  (EventCollision00 ReqNo 1/2) — 是攻击招, 命中充能直接可用, 多段每跳各充。
- **user 调参实测钉死**: 纳刀架势出边的**真实请求 id = 7/8/9** (袈裟斩/移动斩族, ctx 行
  `chargehit 3 7@98 0.15 0.08` 局内生效) — FSM 节点 ActionNo 82/83/84 是**另一套编号**,
  链接进入的招节点号≠请求号 (09-20 结论的又一实证, 这次是 user 从 chargehit 日志读的)。
  之前配置注释里写的 "82/83/84/70 出边" 已纠正。
- **user 的充能经济定案** (配置文件现状, 数值 user 自调): 通配默认行和 from 默认行都被
  user 注释 — **普通攻击不充能**, 只有纳刀链充: 7/8/9@98 = 0.15/0.08, 109 = 0.19/0.053,
  105/106/108 勇气态 0.02-0.03 (普通态 0), 100 = 0.15/0.08。ResolveChargeHit 对未列出
  招返回 nullptr = 不充, 无通配行时系统照常工作 (ctx/move 层不依赖通配)。

### 2026-10-25 (4) chargestart 起手充能规则 (100 收刀回避专用; "312 有攻击窗"结论被 user 翻案)

- **user 纠错**: 312 (收刀回避) 实战从没出过伤害 — 我从 TIML dump 看到 EventCollision00
  轨 (ReqNo A/B + mFlag f35/f61 窗段) 就判"有攻击窗"是**误读**: 没有
  CollisionTimelineObject ノードID 轨把窗挂到 hitbox, 那些轨是无路由残留。教训:
  **判"某招能不能命中充能"必须有实战伤害佐证, TIML 轨存在≠有判定** (COL 章节"TIML 只开窗"
  的另一面: 开窗还要看路由)。
- **新规则** `chargestart <set> <id> <amt> [valorAmt]` — **起手无条件充条** (bar 刻度),
  用于"永远吃不到命中但必须回气"的招。应用点 = ApplyBeginGaugeRules (cost/fx 同点),
  语义与 chargehit 完全一致: 0→白转换 (bar 0.05) + 白刃跨满→黄升级 + valor 4 参。
  100 用它正合适: 唯一入口是 98 格挡转入, **起手 = 格挡成功事件本身**, 无条件充等价格挡语义。
- **配置**: `chargestart 3 100 0.15 0.08` 替换了错误的 chargehit 3 100 行 (user 实测
  chargehit 版从未触发, 与"无攻击窗"互证)。DLL md5 6e88a118。
- **chargestart vs timer**: timer 是秒数+按等级时长换算, chargestart 是条刻度直加 —
  调参语义和 chargehit 一致, 优先用 chargestart。

### 2026-10-25 (5) "0.15 没加上"破案: 架势出边的袈裟斩是 82/83/84, 7/8/9 是输入路径请求 id (同招两套号实锤)

- **user 报告**: 7/8/9@98 配了 0.15 但目测只涨 ~1/20。**日志破案**: (a) 那三行一次都没
  命中过 — **prev=3:98 的袈裟斩命中全部是 act=3:82/3:83**; (b) 目测的小额 = 通配行
  `chargehit 3 0 0.02 0.015` (user 自己开的)。
- **机制定案 (09-20"链接条件测按键不测请求"的另一面)**: **同一招两套 id 按入口分** —
  架势→袈裟斩走 **FSM 链接执行, DoAction 发的是目标节点的 ActionNo (82/83/84)**;
  连段中按键输入产生的袈裟斩走请求层 = **7/8/9**。NFH 10-12 案例 ("node24 ACT=16 →
  DoAction{3,16}") 同机制。**chargehit/ctx 行按动作层 id 匹配, 配哪套要看入口路径**;
  from 语境行免疫这个二义性 (只看 prev)。
- **修复**: 配置三行改 `82@98/83@98/84@98` (数值 0.15/0.08 保留, sed 精确替换, user
  正在同时手调文件)。7/8/9 若要充可配链式语境 (如 7@9) 或普通行。

### 2026-10-25 (6) オーラレベル条件取值链全破解: 黄刃判定新钩点 (AURAW/AURAR 日志钩子已部署, 待局内验证)

- **勇气模式转换失效根因 (二分实证)**: 部署版 wp03_action.fsm 所有勇气分支用的条件名
  `オーラレベル黄以上` **在游戏属性注册表里不存在** — 全 dump exe .rdata 搜"オーラ"仅两条:
  `オーラレベル白以上`@0x1430BA6D8 (cFSMPl_W03 成员 0x269) 和 `オーラレベル赤以上`@0x1430BA6F8
  (0x26a)。名字解析 (FUN_140b2e4e0 全局 / FUN_140b306a0 分武器组) 查不到 → 转换实例建不出来 →
  条件恒假。与 09-22 `R+↑`/裸`↑` 同类: **FSM 属性名必须是注册表里存在的字符串**。
  LS 组 (case 3) 成员块 0x269-0x277 共 15 个封顶: 白/赤以上 + 9 个練気ゲージ判定(気刃斬りI..見切り
  + 居合斬り(0x276) + 見切り成功(0x277), **没有黄、没有裸等级属性 → 纯 FSM 表达不了 ==2 / >=2**。
- **原版语义 (由原版链接用法反推, 可靠)**: 白以上 bit0 门控 J気刃二連斬り/必殺駆け上がり
  (= 等级≥1); 赤以上 bit0 门控 J気刃三連斬り (= 等级≥3)。原版勇气作者 (MHW-valor) 62 条全部
  `白以上 AND 練気ゲージ判定(気刃大回転斬り AND R` — **"黄刃检测"其实是气刃量判定伪造的**
  (旧观察"白不触发黄红触发"正是这个 AND 造成的, 不是颜色语义)。
- **取值链 (本轮主产出, 全环验证)**: FSM VariableNode → 名字注册表记录 (.data
  白0x144F7EF88 / 赤0x144F7EAB0) → 类型描述符 (0x1431C2C10 / 0x1431C2CC0, 6-7 个工厂指针+名字串
  `nHmTransition::cWp03AuraLevelWhiteOver/RedOver`) → 类 vtable (**0x1431C5668 / 0x1431C56B0**)
  → **vtable+0x30 check 函数 (白 0x140F0DC60 / 赤 0x140F0DCD0)**。vtable+0x20 = lea rax,[rip];ret
  的 RTTI getter。签名唯一命中已入 sig_check.py/signatures.json (auraWhite/auraRed)。
- **check 函数现状**: WhiteOver = player(obj+0x10)→weapon(+0x76B0) 门 → FUN_14211C400(weapon) →
  可选 IsActionAvailable(ctx, 0xCC) → 0=放行 / -1=拒绝。RedOver = 读 **weapon+0x2358**
  (`<2` 尾调 WhiteOver 体, `>=2` 直接 -1)。**weapon+0x2358 = 气刃量档位**: 升级驱动区
  (6760059 一带) 把气刃量 float **+0x2350** 对照 4 档阈值表 (func_0x14211caa0 取表) 算 0-3,
  同写 +0x2354(速率)/0x2374/0x2388/0x2390/0x2398/0x239c — **原版"蓝条攒满升色"的真判定字段,
  与 DLL 直写的 +0x2370 平行**。⚠静态极性疑点: RedOver `>=2 拒绝` 与"J三連需红"矛盾、
  RenkiRateAvailable(0x140F0DD00, 同族) `==2 才放行` 也怪 — **极性/调用图以局内 AURAW/AURAR
  日志为准, 别按静态读数写死语义**。
- **AURAW/AURAR 日志钩子已部署** (mod build, md5 2325c29cf5fd77366446121747f6f19e): 两 check
  纯转发 detour, 记 `ret / obj+8 / tier(+0x2358) / lv(+0x2370) / gauge(+0x2350) / obj /
  caller RVA / act`, 本地玩家过滤 + 首发自带 own= 标记 (NegrEvent 教训) + 40 行/s 上限。
  MEMORYMAP 报告多两行 auraWhite/auraRed=ok。**下一版语义改写就挂同一对钩子** (detour 内按
  level>=2 分流; vanilla 招共用这两类, 需按当前动作 id 分流或接受全局改语义)。
- **配套必做**: 部署 FSM 的 20 来条 `オーラレベル黄以上` 必须改回 `白以上` (或其他已注册名),
  否则无论 DLL 怎么改条件恒假。纯数据备选: 原版作者的 `白以上 AND 練気ゲージ判定(気刃大回転斬り`
  配方 (量阈值即"黄"门槛, DLL 已控量条)。
- **(同日补充) 转换类在玩法中零调用 (反证)**: AURAW/AURAR 钩子整局 (user 正常游玩, 2.5 万行
  其他钩子日志, 3:80 斬り上げ×12 / 3:87 必殺攻撃×39 都打了) **一次未触发** →
  **FSM 链接条件的玩法求值不走 nHmTransition 类** (那些类/注册表/描述符是另一套, 可能是编辑器
  或动作层候选仲裁用)。新主假设: 引擎走**注册表 getter FUN_141922190/1F0/240**
  ((entity, id, index) → row+0x24+index*2 计数器; 09-14 属性 222 的取证调用方 0x11B/0x11C
  = FSM 求值区)。mod build 原本只装了 int getter (force 路径宿主), **bool/float getter 没装**
  — 已补装并加 `AURAPROP` 日志 (id 0x260-0x280 全 LS 块过滤; 練気ゲージ判定族 0x26b-0x273
  被原版 R2 连携必然求值, 它们出现即证实路径)。部署 md5 6f53ab4a8506492db239e7494ca1f36c。
  若 AURAPROP 也空: 下一手 = 找 cFSMPlBase 实例地址后 PAGE_GUARD 守卫成员区 (probe 的
  ATKTRACE 机制可复用)。
- **(同日补充 2) getter 路径也证伪 → round 3 = 注册表绑定钩**: AURAPROP 整局零调用 (user 打了
  3:87 必殺攻撃×137 等足量动作) → 引擎**不经过** FUN_141922190/1F0/240 读 LS 条件块。round 3
  (md5 f65e4a6017956f514895f1abd40eb38e): 钩 **FUN_140462700 注册表行查找** (propLookup, 签名
  已入 json) — key 0x260-0x280 的 (key,row) 首次绑定记 `AURABIND`,617/618 的行地址被记住后
  每帧轮询 row+0x24+idx*2 计数器边沿记 `AURAROW`;另一次性 dump 组件绑定键
  (component=*(player+0x126D8), key=*(int32*)(component+0x84+id*24), id∈{222,617,618,631}) —
  617/618 若 key=0 即未绑定 (631 前例), 引擎就是直读 cFSMPlBase 实例成员, 下一步 PAGE_GUARD
  实例 (先要从 propLookup 绑定日志或 FUN_14118a0c0 的 per-frame ctx 找实例地址)。
- **(同日补充 3) +0x2358 假设成形 → round 4 = tier 钉死判决实验**: 3:75(J気刃三連斬り, 赤以上
  门控)在 AURAPROP 局打了 95 次照常出招 → 转换类+getter 双双出局实锤。读 +0x2358 的 6 函数簇
  (4922225+)是升级演出 FX 选择器(按档位取 0x7D1 族 id),不是求值器。**新假设: 白以上/赤以上
  条件读的是量档位 +0x2358(由量条 +0x2350 对阈值表算出),不是 DLL 写的 +0x2370** — 一并解释:
  原版勇气作者的量条配方有效(量条→tier→条件)、7月"白不触发 2/3 触发"(当时量条状态不同)、
  三轮引擎钩子全静默(泛型求值器直接读字段)。vanilla 里 0x2370 是 tier 的缓存镜像(升级/掉级
  处理器两边同写), 用户的 SetSpiritLevel 制造了分裂。**round 4 (md5 da216c904a37831721e2bee9
  84b5b9c)**: 新配置 `ValorGauge.aura-tier.txt` 单整数 (0=off/1=镜像 level→+0x2358/2=钉
  +0x2358=0, 随 Shift+F6 热重载), 每帧写 +0x2358, 变化记 `AURATIER`。**判决: mode 2 (已预写 2)
  下白刃时 J二連/必殺駆け上がり 变不可用 = 条件读 tier 实锤**; 若 mode2 无影响而 617 绑定键=0,
  转直读 cFSMPlBase 实例成员 + PAGE_GUARD/F5 挖矿。round-3 的 AURABIND(component 绑定键 dump)
  也在本版 DLL 里, 一次局内同时收两份数据。
- **(同日补充 4) round 4 判决 + 注册表行实锤**: mode 2 (tier 钉 0) 整局 AURATIER 零写入 = tier
  从未离开 0 (量条清 0.05 的系统下 updater 没抬过它), 而兜割/J二連(74)/J三連(75) 照常出招 →
  **条件也不读 +0x2358**。但 round-3 的 AURABIND 大丰收: **整块 0x260-0x280 都在注册表里绑了行**
  (heap, stride 0x90; 0x269→row 白以上 / 0x26a→row 赤以上), component 绑定键 id617=id618=key1
  (id222=key500, id631=key1)。AURAROW 抓到 +0x24 计数器跳变 (0→2397/2399→-1→0 振荡) **但全部
  发生在加载期, 游戏中静止** — 活体 FSM 可能绑了另一行 (旧 dedupe 吞了后续绑定日志) 或条件值
  不在 +0x24。**round 5 (md5 2c9b0b570c1a86ef27f6b319d42ef46a)**: 617/618 的 propLookup 不去重
  (`AURABIND2`, 上限 60 条) + 跟踪最新行 + **400ms 行内容快照差分** (`AURADUMP`, row+08..37
  48字节 hex + lv + act) — 跨色变差分出条件值真身偏移。tier 实验配置已回 0。
- **(同日补充 5) round 5 结果 + 收敛**: AURABIND2 揭示 **617/618 被多实例反复绑行** (≥5 对:
  0x6d1250b0/0x6cd850b0/0x75338240/0x3e138240/0x75401cf0..., 617/618 成对 +0xa0 相邻), "跟踪最新行"
  会追着菜单/预览实例跑 — AURADUMP 全落在 lv=-1 (加载期), 游戏中没采到活体。行内容结构:
  +0x08=成员id(0x269/0x26A 本尊!), +0x24 dword = 帧戳样值 (2397/2399/-1/0, 617/618 差 2)。
  **证据链收敛**: J二連/兜割/J三連 在 tier=0 钉死下照常出招 + 条件随 0x2370 驱动的颜色正常门控
  → **白以上=lv(0x2370)>=1, 赤以上=>=3, 读的就是气刃等级**(行计数器是引擎侧镜像); 7月"白不触发"
  = 原作者配方的 AND 量条判定, 从来不是颜色语义; 用户系统唯一真 bug = 未注册的'黄以上'名。
  **round 6 (md5 35258da2999ec72617aef99f9ae02ffc)**: 跟踪全部行 (每 id ≤12) + DLL 驱动的等级
  边沿立刻整表快照 (`AURALVEDGE` vs 400ms 常规 `AURADUMP`) — 黄↔红边界翻转的行/字段 = 活体
  条件值真身 → 下一版 per-frame 写行计数器实现"白以上:=yellow+"(纯 DLL, FSM 回改白以上, 无节点
  失步)。备注: DLL 强制至少白刃, 白以上恒真, 判决观测点 = 黄↔红边沿 (赤以上翻转)。
- **(同日补充 6) round 6 结果 → round 7 = PAGE_GUARD 抓现行**: AURALVEDGE 零命中 — 等级边沿时
  12 个被跟踪行没有一个字节变化 → 活体条件值不在 propLookup 绑的那些行里 (那些行是别的消费者;
  玩家 FSM 的绑定/求值走别的表或直接缓存)。**round 7 (md5 36bd2634cad46b8bd2478893813b0e2d)**:
  克隆 probe 的 ATKTRACE VEH 机制 → `PollAuraGuard` (mod 每帧轮询): 拔刀锁+3s 自动布防, 守卫
  全部被跟踪 617/618 行 (≤24 行, 每行所在页 PAGE_GUARD, 单步重挂), 窗口 90s 后自动撤防,
  `AURAGUARD i<N> <R|W> off=0x.. rip=+0x.. stk=..` 记录行内访问 + 邻近分配噪声计数。守卫窗内
  用户切色 → 读写 RIP 即引擎求值器/写入器真身。若 90s 内行内零命中 → 实锤"值不在注册表行",
  转挖 cFSMPlBase 实例 (F5 挖矿: 无刃↔白 差分, 但用户系统强制至少白刃, 需临时注释掉强制)。
- **(同日补充 7) 守卫崩溃复盘 + 静态扫尾 + round 8 = F5 挖矿**: (a) AURAGUARD 崩溃 = 守卫 ≤24 个
  堆行 300s, 行被释放/复用后守卫页打进游戏 SEH —— **已改为配置 opt-in**
  (`ValorGauge.aura-guard.txt`=1, 默认关; 崩溃版 md5 a819db64 已撤为 .hold-crashy36bd,
  安全 mod 版 .hold-safe-mod=73594c21659916cbc96cce7d34a3cded 待换回)。(b) 用户提议"从 +0x2370
  写入地址找读取点" — 静态扫完全部 95 处读点 (新增排查: FUN_140b98060=请求重映射 240/247→244、
  FUN_1417fe1c0=FX 0xC134-7 选择、FUN_1417ffdb0=lv==2 选 motion 参数 +0x980/+0x950、
  FUN_14214d840=5 档计量、updater 拔刀位 bit0x20/0x40), **无行写入器、无独立求值器** →
  引擎把等级**拷贝**进 FSM 上下文后求值器读副本 (0x2370 直搜永远搜不到)。**round 8
  (probe 798ab4d505ee166cb736b5072787c858)**: F5 挖矿器新增两窗口: **fsmCtx** (mgr+0xDA0 →
  0x2BC0, FUN_14118a0c0 的 cFSMPl_W03 每帧上下文) + **fsmComp** (player+0x126D8 → 0x6000, 键表
  组件); probe 无 SetSpiritLevel 干扰, 等级自然演进, 白以上在 0↔1 边界真实翻转可挖。
  流程: 换 probe DLL → 任务里白/黄/红/衰减各 F5 标记 → ValorProbe.log 报告找边界翻转字节。
- **(同日补充 8) round 8 挖矿失败复盘 → round 9 = 挖矿器改造**: 用户挖矿局数据无效, 两个 bug:
  (1) **MinerCaptureFrame 只在 见切86/居合101/102/翻滚18 动作中记录帧** — 用户标记的白/黄/红
  状态时刻根本不在采集窗; (2) **regionNames[8] 越界** — fsmCtx(9)/fsmComp(10) 永远不显示。
  **round 9 (probe 3961091718c672976e1a3aeac86b7448)**: 挖矿器改气刃挖矿专用 — 连续录制(去
  动作门, 600 帧自动滚段), F5 = 结束当前段+记录当时气刃等级进日志 ("MINER: phase N starts now
  (aura lv at mark=X)"), MinerRun 加 levelBegin/levelEnd, kMinerMaxChanges 30k→120k, 段保留
  16→24, 分析重写: 每键"出现相位列"升序报告 (`MINER KEY <region>+0x.. phases=[..] changes=N`,
  regionNames×10 含 fsmCtx/fsmComp) — 只在颜色跨越相位翻转的字节 = 条件值候选。上一局数据作废,
  需重跑: 白/黄/红各停几秒 + F5, 无刃也标。
- **(同日补充 9) round 8 挖矿数据分析 + round 10 = 硬件断点**: 相位-等级表完美 (11 相位覆盖全部
  0↔1↔2↔3 边界), 但 **全 log 无 phases=[7,8] (赤以上翻转) / [1,2,3,10] (白以上翻转) 的键** —
  ~100KB/帧 ×10 窗口都没有边界翻转字节 → **条件值不落地, 求值时现场计算**。另发现 +0x2CE4
  真身: **开刃阶段机的等级捕获** (FUN_1421225e0: params+0xA68=段数, +0x2740=段数组, +0x23C4/8
  计数, 段0 写镜像; 6764464 读镜像映射 {1,4,7}/{3,6,9} 进 +0x2790 计分表; 重置=1)。挖矿器已改
  气刃专用 (连续录制/600帧滚段/F5 记等级/相位掩码报告, round 9 md5 39610917)。**round 10
  (mod fc26cc5f73926750ce1d8fb34d8e60b9)**: **硬件调试寄存器** — `ValorGauge.hwbp.txt`=1 启用,
  DR0=weapon+0x2370 DR1=+0x2CE4 (RW, 4字节), 全线程挂载 (suspend/set/resume), 拔刀锁+2s 自动
  布防, 150s 窗口后清 DR7, VEH 记 `HWBP d<N> rip=+0x.. stk=..` (上限600)。用户触发条件求值
  (跳劈R2/上挑接R2等) → 未知 rip = 求值器真身 (静态已知读取方 RVA 离线过滤)。
- **(同日补充 10) round 10 HWBP 又崩 → 用户裁定: 换 x64dbg 外部调试器**: 进程内 VEH+全线程
  SetThreadContext 挂 DR 与游戏线程/异常管理冲突 (第二次崩溃)。**mod 的 HWBP 已配 0 禁用**
  (代码保留 opt-in)。x64dbg 流程: 进任务拔刀 → 从 ValorGauge.log 拿最新 `controller=0x..`
  → 附加 x64dbg → 表达式 `[[<controller>+14E8]]+2370` 得等级地址 (controller=player+0x61C8,
  weapon=*(*(ctrl+14E8))+2370... 准确式: 等级地址=[[ctrl+14E8]+2370 的解引用链:
  weapon=[ctrl+14E8] 解引用, level=weapon+2370) → 硬件断点 Access/4字节 + **log-only**
  (break condition=0, log text={cip}) → 触发白/赤以上门控招 → 日志 RIP 减基址 0x140000000
  对静态已知读取方过滤, 未知 rip = 求值器。静态扫尾结论一并记录: +0x2CE4=开刃阶段机等级捕获
  (FUN_1421225e0 段0 写镜像), 读它的 6764464 映射 {1,4,7}/{3,6,9} 进 +0x2790 计分表, 非条件值。
- **(同日补充 11) x64dbg MCP 实战 + 条件值"不落地"实锤 + round 12**: x64dbg MCP (HTTP :9094,
  Bearer 在 .mcp.json) 全程可用: AttachProcess/EvalExpression/ReadMemory/WriteMemToAddress
  (参数名 byteString, 空格分隔)/SetHardwareBreakpoint/ListBreakpoints。**真相链**: (a) DR 读断点
  立刻命中 = **我们自己的 mod 每帧读等级是最大噪声源**(PollValorLifecycle 等); (b) 断点条件
  (cip 排除 valorgauge 模块)与 fast-resume 对 HW 断点均未生效, 暂停风暴两次杀死游戏
  (另有一次手算地址错 968850≠96E850); (c) 用户 FSM 探针上线: 待機２ R+赤以上→斬り上げ(80),
  R+白以上→気刃斬3(68), 兜底→突き(79), **裸写 +0x2370=1 后 Ctrl → 79(白以上=假!)** —
  **条件值不读 +0x2370/镜像/+0x2CE4/+0x2D24(0xC)/+0xAD4 任何已知字段, 且 F5 挖矿
  (10 窗口 100KB/帧)在全部相位里找不到 [1,2,3,10]/[7,8] 翻转掩码 → 条件值由引擎升级/掉级
  流程计算或存于未扫描区域**; (d) 排查盲区补上: 挖矿 weapon 窗口只有 0x2400, 而 +0x2740
  数组/+0x2790 计分/+0x2CE4/+0x2D24 全在 0x2400 以外! **round 12 (probe e179760b)**: weapon
  窗口 0x2400→0x3000, 重跑 F5 相位挖矿 (无刃/白/黄/红各标记) → 找 0x2400-0x3000 内边界翻转
  字节。x64dbg MCP 工具经验: SetBreakpointFastResume/条件对 HW 断点无效; ExecuteDebuggerCommand
  响应回显当前 cip 非命令目标; MCP 有 ~256 字符命令长度限制; python socket 直连必须带完整
  HTTP 头 (用 curl 最稳); 暂停态下 WriteMemToAddress 需先 PauseDebug。
- **(同日补充 12) 白以上"裸写不生效"实锤 + 求值器追捕进行中**: x64dbg HW bp + 条件断点(排除
  valorgauge/更新器/HUD/衰减) 全套就位后, **裸写 +0x2370=1(含 timer 1.0) 后 Ctrl → 79(双判定
  失败)** — 白以上在等级 1 时为假! 同场验证: 镜像 +0x2CE4=1、+0xAD4=100(写它也无效)、
  +0x2D24=0xC(无效) → **条件值不读任何已知字段, 引擎自己升级流程(69 命中/掉级)维护的某个
  派生态才是真身**; 用户 FSM 探针(待機２ R+判定→68/79/80)工作正常(等级 0 → 79 ✓)。
  另排掉: +0x2D24=0xC 无效、+0x2CE4/开刃阶段机、+0xAD4 auraSet。**噪声源定位: mod 内
  PollAuraRow(round 5/6 诊断轮询, 骑 PropIntHook 每帧 ResolveLongSwordLevel 读等级)未随
  weapon-type 门控 — 已门掉**; PollValorLifecycle 也补了硬门(weapon-type≠3 即跳过,
  md5 f5d97f3c/514c15a8 之后的构建)。流程经验: scancode SendInput 才能触发游戏按键
  (虚拟键不行); x64dbg HW 断点的 break condition/fast-resume 在该版本不生效(每次访问都暂停);
  TLS 回调断点每次附加从数据库回灌, bpc 清; MCP 工具 condition 长度上限 ~256。
  **下一步**: 用户关游戏 → 部署 f5d97f3c → 重开 → 附加 → HW bp(无 mod 噪声) → 87/Ctrl 触发
  → 暂停 RIP = 求值器真身。
- **(同日补充 13) 行写入器抓获 → 条件值真身仍未定位, 但可行路径已清晰**: x64dbg 内存断点
  (write-only, 0x269 两行)命中 0x14203D91A = FUN_14203d000 的 **瞄准/位置写入**
  (movss [rax+0x30], xmm7 写浮点坐标, FUN_1412dc780/FUN_1419c0ee0 链取对象) — 行 =
  转换类组件实例, 混有瞄准数据, 非条件值存储。**关键事实汇总**: (1) FSM 探针
  (待機２ R+赤以上→80/R+白以上→68/兜底→79)工作正常, 等级 0 → 79 ✓;
  (2) **裸写 +0x2370=1(含 timer)后 Ctrl → 79 = 白以上为假** → 条件不读等级本体;
  (3) 同样排除: +0x2CE4 镜像/+0x2D24=0xC/+0xAD4=100/+0x2358 tier;
  (4) **但用户 DLL 的 SetSpiritLevel 配方驱动的白刃, 白以上门控的 J二連 正常可用
  (round-4 实测)→ 14211c5f0 配方路径确实驱动条件真值**; (5) mod 读取静默方案:
  PollValorLifecycle 硬门(weapon-type≠3 跳过, md5 514c15a8)+ PollAuraRow 门掉
  (f5d97f3c)。**下一局决定性测试**(weapon-type 恢复 3, mod 全功能):
  SetSpiritLevel(2) 到黄 → Ctrl 探针 → **出 68 = 条件跟随 SetSpiritLevel 真值** →
  最终方案 = 勇气分支改白以上(等级≥1 恒真于黄红)+ 白刃阶段 DLL 压量条(量判定假)/
  或 DLL 请求门 — 全用已验证机制, 无需继续逆向求值器。
- **(同日补充 14) 行写入器实验与内存断点结论**: 只写内存断点(0x269 两行)命中 0x14203D91A =
  FUN_14203d000 的**瞄准/位置浮点写入**(movss [rax+0x30], 经 FUN_1412dc780/FUN_1419c0ee0 链) —
  行 = 转换类组件实例池, 与瞄准数据混用, 非条件值存储。bpmc 清理后恢复流畅。**x64dbg 该版本
  HW 断点不生效 break condition**(排除了 valorgauge 的条件照样暂停)——条件过滤对 HW 断点不可用。
  用户正确指出: LLM 实时玩 60fps 游戏不现实, 分工 = 用户物理操作 + 程序化时机控制。
  **当前状态**: weapon-type=3 恢复, x64dbg 脱离, mod DLL 514c15a8 在位 (含 PollValorLifecycle
  硬门/PropLookupHook/AURABIND2 日志), f5d97f3c (PollAuraRow 门) 未部署待下次。
  **10+ 轮结论**: 白以上/赤以上 = 引擎自有升级/掉级流程维护的派生态; 裸写 +0x2370 不驱动;
  SetSpiritLevel 配方(14211c5f0 复刻)驱动 (round-4 J二連 证据); 赤以上在用户 DLL 红 ✓。
  **可行最终方案**: 勇气分支 黄以上→白以上 (AND 練気ゲージ判定(見切り 已在) + DLL 白刃期
  压蓝条 (量判定恒假, 升黄即放开) — 全用已验证机制; 或 DLL 请求门 (等级1 拒勇气招, 节点失步
  caveat)。
- **(同日补充 15) 输入架构确认**: 运行时模块表: dinput8.dll ×2 (0x7FF8A80C0000 26MB = Stracker's
  wrapper + 0x7FF8E9090000 132KB = 系统) + xinput1_4 + xinput9_1_0 + winmm + hid。游戏 exe 的
  静态导入表被保护壳清空(离线 PE 解析为空) → 运行时模块表为准。MHW 输入 = 轮询模型
  (DirectInput GetDeviceState 键盘 + XInputGetState 手柄, 每帧拉状态); 键鼠→手柄按钮 = 游戏
  内部映射。**拦截点**: 手柄 = XInputGetState (干净导出, MinHook 直挂, 改返回的 wButtons 位);
  键盘 = DirectInput 设备的 GetDeviceState (COM 方法无导出, 需 vtable patch 或
  DirectInput8Create 捕获设备对象)。**纯应用层方案**: R+△ 组合在游戏内部本就生成 3:87 请求
  (気刃突刺), DoAction 钩子可见每个请求 → 外挂条件不满足时改写/吞掉 3:87 = "不满足不让进",
  检查时机 = FSM 转换点 (SHEATHREWRITE 同款模式, 机器现成)。用户裁定 LLM 实时玩游戏不现实,
  分工 = 用户物理操作 + 程序化时机/内存/断点控制。
- **(同日补充 16) 输入系统静态地图 (Ghidra)**: 游戏**静态导入** DirectInput8Create/XInputGetState/
  XInputSetState/XInputGetCapabilities (delay-load 包装: XInputGetState@0x14273DCC9,
  DirectInput8Create@0x14273DC3E, 固定 VA 可 MinHook); 设备轮询族: XInput 调用方
  FUN_1422a5710/59d0/5e70/29fbc0/fdc0/a0650 (4 手柄槽 × 0x2D8 设备状态, 枚举+选择),
  DirectInput 调用方 FUN_1422e6a20/e8060/29f8e0/a5040/29f4c0。输入 = 轮询模型。
  **R+△ 组合在游戏内部 = 3:87 请求 (気刃突刺)** → DoAction 钩子可见 → 外挂条件门可以在
  DoAction 层做 (SHEATHREWRITE 同款): 条件不满足改写/吞掉 87, 检查时机 = FSM 转换点。
  键盘路径注意: KB 玩家的 R+△ 走 DirectInput → 游戏 KB→pad 内部映射 → XInput 钩子
  挡不住键盘; 挡键盘需 dinput GetDeviceState 缓冲钩子 (COM vtable, 工作量中等)。
- **(同日最终) 行页守卫路线死刑**: 写-only 内存断点(0x269 行)走查发现行所在页与 **GPU 驱动
  (igc64)/输入宿主(inputhost)的高频写入共享** — 页粒度断点每帧命中无关写入, 过滤不可行,
  且驱动写守卫页直接把游戏打挂。**堆行路线全线退役** (HW bp/条件断点/mem bp 三种全试)。
  10+ 轮最终事实: 白以上/赤以上值 = 引擎升级/掉级流程维护, 存储未定位 (10 窗口挖矿 + 全字段
  排除 + 行页守卫三路全败), SetSpiritLevel 配方是唯一已知驱动路径。
  **落地路线 (全部已验证机制, 零逆向)**: (1) FSM-Studio 把勇气分支的黄以上 → 白以上
  (AND 練気ゲージ判定(見切り 已在); (2) DLL 白刃期压蓝条 (level==1 时 +0x2368 钳 ≤ 見切り
  判定阈值, 升黄放开) → 白刃期量判定恒假 → 勇气分支死; 升黄放开 → 活。零失步零截断。
  或 (b) DoAction 请求门 (等级1 拒勇气招, 节点失步 caveat)。
- **(同日收尾) 实验代码全部回退**: dllmain.cpp git checkout 回 HEAD — 今天的 DLL 侧实验
  (AURAW/AURAR/AURAPROP/AURABIND/AURAROW/AURADUMP 钩子、PollAuraGuard/PollHwbp/PollAuraTier/
  PollAuraRow、HWBP/守卫机器、valor-clamp、挖矿器气刃改造)全部丢弃, 知识保留在本文件。
  保留并已提交: sig_check.py 的 auraWhite/auraRed/propLookup 签名 (逆向成果)。
  plugins 已重部署干净 HEAD 构建 (8a26cc36), weapon-type=3, aura-tier/hwbp 实验配置已删。
  **下一会话入口**: FSM-Studio 把勇气分支 黄以上→白以上 (AND 練気ゲージ判定(見切り 已在)
  → DLL 加白刃期蓝条钳制 (~20 行, 见补充 14 落地路线) → 探针验证 (黄刃 Ctrl→68,
  白刃 R→刚一 不出)。战技系统 (R+△/R+○) 是之后的事。

### 2026-10-25 (7) 勇气入场方案改版 plan B: 白刃充条, 红刃=勇气模式 (user 定案, FSM 归 user / DLL 归本侧)

- **方案切换 (user)**: 上一节落地路线 (白以上+量条钳制伪造"黄判定") 弃用 —
  user 改走更简单的二元模型: **白刃(1) = 充能相位, 红刃(3) = 勇气模式**,
  FSM 勇气分支由 user 改挂 **`オーラレベル赤以上`** (已注册属性 0x26a, 12+ 轮追捕
  的核心遗留成果)。条件真值驱动沿用已验证事实: 裸写 +0x2370 不驱动条件
  (补充 12), **SetSpiritLevel(14211c5f0 配方) 驱动** (round-4 J二連 + 补充 13
  "赤以上在用户 DLL 红 ✓"), 引擎原生红也走同配方形态。
- **DLL 改动 (部署 md5 2477ac6e313e00be5e09282b0bcf0d71, 上任 8a26cc36)**:
  1. **跨域升级直达红**: chargehit + chargestart 的白刃满值跨越
     (prev<0.999 → next>=0.999) 改 `SetSpiritLevel(3, "charge-full")`
     (原写 2), 日志 `spirit-level charge-full: 1 -> 3 timer=1 aura=fired`。
  2. **valor 档位收紧为等级>=3** (原 >=2, 三处: gainhit/chargehit/chargestart):
     第 4 参维持额只在红刃生效; 105/106/108 (a=0 勇气态专属) 在红刃照常微充,
     白/黄吃满额。黄刃无角色, 只是红→白折返的过路态。
  3. **红→黄折返沿 (PollValorLifecycle 新增)**: `current==2 && previous==3`
     → `SetSpiritLevel(1, "valor-exit", 0.05f)` + tracker 钉回 1 (防下一帧
     白刃沿重复 valor-recharge)。红刃自然到期和居合/兜割扣级
     (Gauge_Consumption_for_Helm_Plus_Iai_Spirit_Slash) 落到黄的一帧内即折回
     白刃+条 0.05, 勇气结束立回充能相位。日志 `spirit-level valor-exit: 2 -> 1 timer=0.05`。
  4. **gainhit 块拆掉 round-1 残留**: 0→白转换 (带满条 timer=1.0!) 和
     蓝条满→升黄 (0x2368>=0.999 ∧ 白刃 → SetSpiritLevel(2)) 删除 — plan B 下
     蓝条满 ≠ 勇气入场, 且旧转换给"天生满条"会堵死跨域检测。gainhit 现在纯练气
     喂食 (76/77/109 0.2/0.3 行不受影响, 刚1 蓝条门照常)。
  5. SetSpiritLevel 前置声明加在 g_lastObservedSpiritLevel 旁 (默认参留在定义处,
     前置声明带默认参 = C2572 重定义)。
- **边界语义**: 自然白→黄升级招 (若 user FSM 保留) 不折返 — 只抓红→黄;
  黄→白自然掉级仍走既有 valor-recharge 清条 0.05 (每次进白清条惯例);
  勇气入场唯一路径 = 充能条跨越满值, 蓝条/升级招不再产生平行入场。
- **待局内验证 (user 首测点)**: (a) 白刃充满 → 刀光变红 + FSM 勇气分支活
  (赤以上被 SetSpiritLevel(3) 驱动 — 配方推定成立但白→红直跳是首测);
  (b) 红刃到期/居合扣级 → 一帧内折回白刃+条 0.05+白刀光; (c) 红刃期间
  105/106/108 命中按 0.02-0.03 维持条; (d) charge-full 日志 1 -> 3。
- 部署配置 gauge-rules.txt 头注释同步 plan B 语义 (规则行零改动)。


### 2026-10-25 勇气 plan B 局内验证通过 (user 定案收口)

- plan B 全链路 user 局内验证通过: 白刃满条 → SetSpiritLevel(3) 直写红刃入场
  (オーラレベル赤以上条件随配方驱动, FSM 勇气分支正常点火); 红刃到期/被扣级 →
  黄刃折返白刃 + 条清 0.05; 充能/维持额四档解析照常。部署 DLL md5 2477ac6e。
- 至此 10-25 (6) 12+ 轮条件值追捕的落地路线闭合: 白以上/赤以上 = 唯二注册色条件,
  SetSpiritLevel 配方 = 唯一已知驱动路径, 黄以上不存在 (恒假) — 系统定案,
  后续按此模型扩展 (战技槽 = 外挂新字段蓝条, 不复用练气)。


### 2026-10-25 HUD 气刃颜色不刷新: keep-white + overlay 模式行 (user 双 workaround)

- 问题: SetSpiritLevel 配方驱动 FSM/刀光正常 (2477ac6e 局内验证过), 但 HUD 刻度条
  颜色不随任务中的等级写入重刷 — 配方是内存写复刻, 绕过了游戏自己的 HUD 刷新路径。
  user 定调不深挖 HUD notify, 直接上两个 workaround。
- **workaround 2 keep-white** ("只要是太刀就保持白刃", PollValorLifecycle):
  任何 0 级观测沿 (集会所出生/任务开始/白刃自然衰减归零) → SetSpiritLevel(1,
  "keep-white", 0.05) 立即回白。观测路线补 manager 链回退 (原先无动作锁直接
  return, 集会所/任务初帧是盲区; manager 链路径强制 weapon+0x9F8==mod 武器门,
  外武器 0x2370 别义不跟) — 白刃在进任务前就绪 = HUD 入场初始化读到正确颜色
  (FLiNG 式"进任务前设好")。controller 路径 player/weapon 提到共享变量, 三分支
  (keep-white/valor-recharge/valor-exit) 共用。副作用定案接受: 长时间不打条
  0.05→0 → 重挂白, 几秒周期, 白刀光 FX 随之重复。
- **workaround 1 overlay 模式行**: `SPIRIT lv<N> CHARGE/PASS/VALOR/NONE <bar>%`
  (bar = 0x2374), 每帧实读 0x2370/0x2374, 等级配色, 出界显示 BAD (结构漂移报警
  语义); DisplayWeapon() = 动作锁 → manager 链回退, 集会所即显示。任务中白→红
  时 HUD 恒白属已知 — 真相看此行。
- HunterPie 源码考古 (E:\MHW-arts\HunterPie, v2): MHW 武器类型 = WEAPON_ADDRESS
  (421471 map 0x05073ED0 — 与本项目 manager 全局同址, 旁证) + 装备链
  0x50,0xC0,0x8,0x78,0x2E8 读 byte (LS=3, Weapon enum 0..13); 任务态 =
  *(0x500ED30)→State@0x54 (InQuest=2), 区域 = *(0x500ECA0)→+0xAED0。录
  memory-map 作武器类型滑动时的备用路线。
- 构建: mod/probe 两 target 全过 (std::clamp 不吃 volatile float 左值, 先读普通
  float 再夹); 部署 deploy.sh mod, md5 c5372fa1 (上任 2477ac6e)。待 user 局内
  验证: 集会所/任务入场 HUD 显示白刃; 任务中 valor 入场 HUD 仍白 (已知, 看
  overlay); valor-exit 折返后 HUD 恒白正确; 长时间静置的重挂白周期可接受。


### 2026-10-25 keep-white 首测失败 → manager 链 RVA 滑掉修复 (user 局内验证通过收口)

- 首测 (部署版 c5372fa1, user): 进任务 HUD 仍不显示白, 拔刀后才刷新 — user 点破
  "set3 检测条件还在"。日志实证: 全场 `confirmed by manager chain` = 0 行; 每条
  `-1 -> 0` 首次观测都紧跟 `Player action controller locked` (首 set-3 动作) 之后,
  集会所 0 条 — manager 链恒失败, 观测静默滞留首 set-3 锁启发式, keep-white 实际
  仍被第一次拔刀卡住 (keep-white 开火时 HUD 已用 0 初始化完, 拔刀重同步才显白)。
- 根因: 本地玩家 mgr 全局地址随 build 滑掉。旧 VA 0x145073ED0 (RVA 0x5073ED0) =
  HunterPie 421471 map 的地址; 本机 build = **421810** (HunterPie 日志实证
  `Loaded MonsterHunterWorld.421810.map`), 该全局 421810 = **0x50139A0**。旧地址
  读垃圾过不了武器类型 1..14 校验 → 链恒死。以前无人发现: 链原本只服务锁确认,
  失败有首 set-3 启发式兜底; keep-white 是第一个真依赖此链的功能。
- 修复 (部署 md5 588de89b, 上任 2477ac6e, 中间 c5372fa1 首测失败退役):
  1. ResolveLocalPlayerViaManager 候选解析: addresses.txt `localPlayerMgr=0xRVA`
     覆盖 > 缓存胜者 > 0x50139A0 (421810) > 0x5073ED0 (421471 兜底); 每调用仍过
     武器类型校验; 首次成功打 `MEMORYMAP localPlayerMgr RVA=0x... weaponType=N`
     一行 (下次日志一眼可判链死活)。前置声明 Log/ModuleBaseAddr/OverrideOrZero。
  2. PollValorLifecycle / DisplayWeapon: 链玩家先行, 与动作锁玩家不一致 = 跨场景
     陈旧锁 (集会所↔任务换 controller 实体), 丢锁走链 — 同进程连打两任务也覆盖。
  3. 链修活后, 锁确认的 FOREIGN set-3 拒绝逻辑首次按设计生效。
- user 局内验证通过: localPlayerMgr 链读到、气刃等级正确、HUD 到位。memory-map
  本地玩家/武器类型旁证两行已按 build 改写 (候选+覆盖纪律入档)。

### 2026-10-26 战技系统 DLL 侧落地: 战技槽外置字段 + DoAction 请求闸门 (user 定案启动)

- user 定案设计: 战技条=外置变量 (先文字 n%, 后画条); 玩家某动作**打中才充能**;
  释放走 FSM (user 侧), DLL 输入侧条不满禁组合键。R+△ = 战技释放键 (键鼠 Ctrl+左键),
  R+○ 暂不占用。
- **方案选择**: 沿用 10-25 输入地图"纯应用层方案"——R+△ 在游戏内部生成 3:87 请求
  (気刃突刺), DoAction 层吞 (SHEATHREWRITE 同款), 零新增游戏地址依赖。OS 层键鼠/手柄
  拦截 (XInputGetState/DirectInput8Create 可 MinHook) 本轮不做——闸门只看请求层;
  FSM 若接 R+△ 按键属性边会绕过闸门 (警告已写进 gauge-valor/fsm-input 两手册)。
- **落地结构** (dllmain.cpp, mod build only):
  1. `g_artGauge` atomic float 0..100 = 战技条本体, overlay 文字行 `ART n%`
     (y=52, SPIRIT 行下方, 蓝=充能/绿=READY), 零游戏内存占用。
  2. 充能挂 `NotifyPlayerHitConnected` (连接命中门, 白挥天然不充), `ArtChargePercentFor`
     精确行>通配行, 100 封顶, ARTGAIN 日志 (满条 crossing 记 FULL)。
  3. 闸门在 DoActionHook **tracker 之前** return false: 请求不落地/不进缓冲/不进 tracker;
     门 = art enabled + ArtGateMatches(set,id) + IsLocalPlayerActionController +
     ArtWeaponGate(直查 weapon+0x9F8, 不等 session lock, 首按即闸)。条满 → artPass,
     g_originalDoAction accept 后 drain 清零 (consume 可关)。ARTGATE swallow 限频日志。
  4. 配置 `ValorGauge.art.txt`: gate/charge/consume 三语法, 缺文件=系统全关;
     LoadArtConfig 挂进 LoadForceActionConfig 尾链 = init + Shift+F6 双覆盖热重载。
- **事故一次**: DoActionHook 插 drain 块时 old_string 取了前缀两行 (含原 #ifndef
  VALOR_PROBE 开头), 替换体自带 #endif → 原 mod 块守卫被吞, C1020 意外 #endif +
  probe C3861 ArmGang1GaugeScope; 补回裸 if 后双 target 编译过。
- 部署 md5 `322822ea` (上任 588de89b); art.txt 首轮 `gate 3 87 / charge 3 0 2 / consume 1`。
- **待局内验证**: (1) 充能: 打怪连接命中 ART 涨, 白挥不涨; (2) 条<100 时 Ctrl+左键无反应
  (ARTGATE swallow 日志), 且 vanilla 気刃突刺不出; (3) 条满放行 + 清零 + FSM 侧战技节点
  (user 接线: ActionNo=87 零入边; 属性边会绕闸); (4) lock/chain 冷启动首按是否漏过;
  (5) art.txt 删掉 = 系统关 (Ctrl+左键回 vanilla)。
- user 局内首测 (2026-10-26): **闸门行为验证通过** —— DoAction 吞请求只拦请求本身,
  不截断/不扰动当前动作 (动作流照常走), 节点失步 caveat 未出现; 充能链路/满条放行清零/
  FSM 战技节点接线仍待打怪验证。

### 2026-10-27 156 弹反总时长 -5 帧（user blend 删前 5 帧 + TIML 整链左移）

- user 在 MHW-Valor-0.0.2-shorter-deflect.blend 手动删掉 156 动画前 5 帧（键域 0..72→0..67）；
  TIML 13 个 action 31 条 fcurve 整体 -5，落负键钳 f0 取裁切点现值：EG02 喊声 f4→f0 /
  EG05 f5→f0 / EG06 金属摩擦 f2→f0（README 标注慎动，整链平移属必要）/ PlMotionVisual
  f1+f4→f0=0x808 / 攻击窗 slot4 [3,13)→[0,8)（前 2 帧裁掉）/ EffectParameter1 f2→f0。
- 元数据双同步：动画 freehk.frameCount 71→66（calculateFrameCount ≠-1 时直接用元数据，
  不改=导出带 5 帧定格尾）+ TIML Data animLength 70→65（motion 时钟 0x10C）。
- 验证：timl-diff.js（node 用 E:\MHW-arts\node-v24.21.0-win-x64）164 TIML entry 仅 156
  差异、逐条=设计移键；dump_timl_frames 读回 frameCount=66 / animationLength=65。
- 部署 md5 `3ef1ee9f`（上任 `5252ca41`，备份 w03_00.lmt.bak-20261027-pre156shorten）；
  产物 _blend_export_156_shorten5.lmt/.txt；blend 已保存同步。
- 待 user 冷启动局内验证：打击点/派生时机体感；攻击窗现在 f0 起跳（开局即判定），
  太早的话把 EventCollision00 mFlag f0 键值回 0、f7 前另起窗。

### 2026-10-27(2) 156 卡肉实测 0.40s → 衔接窗补偿 41→动作末尾

- user 报告：命中后卡肉吃掉衔接窗。读 col：156 CTO ノードID=89 → names[50] move=49
  `居合気刃1赤` → **ATK49 HitLag_a/b=0.40、Spd 1.0、fUnk16/fUnk19=0 全冻**（60fps=24帧）；
  行49 即"刚1 定稿行"，弹反共用——改行卡肉会连带动刚1，故选窗补偿不动 col。
- 补偿：PI.mFlag 连段(b14/b16/b30) 与 mFlag1 R+方向簇（含基线 b12/b26、次级 b27/b13）
  结束键 f41 值改为窗内值（f41=0xC003C300 / 0xCFFC30CC，f55=0x40014301）→ 窗开到动作
  末尾（animLength 65），24 帧卡肉后剩余窗时长 = 无卡肉情形，精确抵消。
- 特殊派生(b17/b31) 与 mFlag2/mFlag4 未动（语义未解/非本次诉求）。
- 验证：timl-diff 仅 156、~行恰为两条轨；部署 md5 `165a9e7c`（上任 `3ef1ee9f`，备份
  w03_00.lmt.bak-20261027-pre156window）；产物 _blend_export_156_window65.lmt/.txt。
- 待 user 冷启动验证：命中后衔接手感；若嫌窗口过宽（末尾 10+ 帧仍可预输入下一招），
  把 f41 键值回调即可（如 0x40014300 挡在 55 前某帧）。

### 2026-10-27(3) 106/110 R+方向提前硬切源动作后摇：簇开窗错档 → 推迟到見切り窗 f44/f58

- **现象 (user)**: 106/110 按 R+方向进一文字长 (MOT 361/362) 会硬切掉源动作几帧后摇;
  R 短版 (MOT 358-360) 无此问题; 107 无簇故只有短版 (FSM 上 38 节点其实有 cond308→363 边,
  缺使能簇不点火)。一文字动画本身无丢帧 (359@0 与 362@0 起始姿态逐骨相同, Blender 采样实锯)。
- **排查**: FSM 边 (node12/6/38) 条件 307=[赤]+[R]+[練気見切り] / 308=[赤]+[練気見切り]+(R+←↑→);
  106/110 全部输入使能窗同帧开 (106 f24 / 110 f38: mFlag 连段位与 mFlag1 簇同键)。
  Ghidra: 属性名→id 解析链 FUN_140b2e4e0 (R=0x26, R+↑=0x29/↓=0x2a/←=0x2b/→=0x2c,
  字符串表 0x1430b9bbc-0x1430b9c34); 条件树编译 FUN_14118bda0/评估 FUN_14118a0c0。
- **定案**: [R] 走连段预输入缓冲, 在动作转换点才消耗 (无跳); [R+方向] 为活值属性, 簇窗一开
  FSM 边即点火 → 簇打在 f24/f38 基线键 (b12) 上 = 比 vanilla 取消档早一档 → 硬切源动作。
  **锚点**: 两招 vanilla 特殊派生窗 (mFlag b17+b31, 見切り族路线) 分别 f44 (106) / f58 (110)
  才开, 且 106 vanilla mFlag1 真 R2 组合窗 (b8-b11) 也是 f44 — f24/f38 只是基线键。
- **修法**: mFlag1 首键剥回 vanilla 基线 0xC7FC10CC→0x00001000 (106 f24 / 110 f38 双通道),
  簇改由 f44/f58 (0xCFFC10CC / 0xCFFC30CC 原键) 开 — 与 vanilla 見切り/特殊派生档对齐。
  按住早按 → f44/f58 才点火 (不吃后摇); 窗内后按 → 即时点火不变。
- 验证: timl-diff 仅 106/110、恰为首键剥簇; 部署 md5 `2a40a855` (上任 `165a9e7c`, 备份
  w03_00.lmt.bak-20261027-pre106110delay); 产物 _blend_export_106110_clusterdelay.lmt/.txt。
- 待 user 冷启动验证: 106/110 R+方向衔接手感。若 f44/f58 偏晚, 把首键的簇值直接改到目标帧
  (新增键) 即可; 107 若也要 R+方向长版, 照 106 档打簇 (FSM 边已在)。

### 2026-10-27(4) 107(3:72) 补 R+方向簇 → 左上一文字长(363) 贯通

- FSM 侧 38 节点 (act72) 的 cond308→MOT363 边一直都在, 只缺使能簇。按 10-27(3) 纪律
  开窗打在 107 自己的 vanilla 見切り档: 特殊派生窗 (mFlag b17+b31) f42 开 → 簇 f42=0xCFFC30CC,
  f100 同值续到动作末尾 (137f), f22 vanilla 键 (b10+b12/b26) 原样不动; 原键内 b12/b13/b26/b27
  均含于簇值无损。user 已验收 106/110 延迟档 (残差 1-2 帧偏早, user 定案接受)。
- 部署 md5 `43fdc01e` (上任 `2a40a855`, 备份 w03_00.lmt.bak-20261027-pre107cluster);
  产物 _blend_export_107_cluster.lmt/.txt; blend 已保存。
- **✅ user 局内验证通过 (2026-10-27)**: 107 R+方向 → 左上一文字长(363) 贯通, md5 `43fdc01e` 定稿。

### 2026-10-27(5) ART 战技条任务门：任务外隐藏+清零

- user 报 bug: 玩家不在任务中 overlay 仍显示 `ART n%` 且保留上局残留。定案: 条=任务域状态,
  任务外一律隐藏+清零。
- 落地: `InQuestState()` 读任务态单例 `*(base+0x500ED30)`→State@+0x54==2 (HunterPie map
  实证公式, memory-map 既有; 非钩子纯数据指针, 只走 addresses.txt `questStateMgr` 覆盖,
  读失败/单例 null 按"不在任务"处理=只影响显示不清 gameplay); mod 构建渲染轮询链
  (PollValorLifecycle 后) 加 `PollArtQuestGate()`: 不在任务中 `g_artGauge.exchange(0)` 非
  零打 `ART gauge wiped (not in quest)`; overlay ART 行显示条件加 `InQuestState()`。
- 设计取舍: 充能路径不另加门 (任务外无怪可充, 轮询只兜离开边); DoActionHook 闸门不看
  任务态 (任务外条恒 0<100, R+△ 照吞, 行为不变)。
- 部署 md5 `f4b0d797` (上任 `322822ea`); ValorProbe 同步编译未部署。待 user 局内验证:
  任务内显示/充能不受扰, 回城后行消失+清零, 下一任务从 0 起。

### 2026-10-27(6) ART 任务门特例修正：训练场/探索 = 战斗区白名单

- user 局内反馈: 训练场、野外探索不是"任务"（State@0x54 非 InQuest），但可拔刀打架，
  ART 必须显示。首轮 `InQuestState()==2` 单判据过窄。
- 考古 HunterPie 源码（本仓 HunterPie/ 完整源树 + 421810 map）: `QUEST_DATA_ADDRESS
  0x500ED30`+State@0x54 枚举 int (0 None/1 Ready/2 InQuest/3..7 结算态)；`ZONE_OFFSET
  0x500ECA0`+`ZoneOffsets 0xAED0` 读 int32 stage id；`Stage.cs` 枚举: 101..109 猎场+
  导之地（任务图=探索图同 id）、201..203 竞技场、301..306 据点、401..417 剧情特殊区、
  501..506 房间、**训练场=504**、主菜单=0。
- 修法: `InCombatZone()` = `InQuestState()` ∪ `PlayableStageId()` 白名单（101-109/
  201-203/401-417/504）；PollArtQuestGate 与 overlay 行都换用。日志措辞改 "not in
  combat zone"。`zoneMgr` addresses.txt 覆盖同 questStateMgr。
- 设计取舍: 白名单而非黑名单（未知 stage 默认区外，防误显；mod 新区报 case 再补）；
  State==2 仍保留短路（HunterPie `isQuestInvalid=Id<=0` 判定暗示部分非任务区 State 也
  读 2，两路并上无论探索读几都工作）。
- 部署 md5 `0373d7d5` (上任 `f4b0d797`)。待 user 局内验证: 训练场/探索显示+充能+满放行，
  据点/房间/主菜单隐藏+清零。

### 2026-10-27(7) ART 门补边沿：探索→同图任务清零（quest id 边）

- user bug: 探索攒的条带进任务不清。根因: 探索与普通任务**共用猎场 stage id**(101..108,
  HunterPie Stage 枚举只有一套猎场 id), 任务态 State 也可能两边都读 2(quest 框架空 id
  跑探索)——区外门全程不触发。user 问"stage 应该会变啊": 只有剧情特殊版图(401..417, 如
  AncientForest2=406)才换 id, 普通调查/自由任务不换。
- 修法: 读 quest 结构补 id 字段(+0x4C, 探索/训练场 id=0, HunterPie `isQuestInvalid=
  Id<=0` 同源); `ReadQuestInfo`/`ReadStageId` 拆出复用。`PollArtQuestGate` 三触发器:
  ①进入真任务边沿(State==2 && id>0 的 false→true, 唯一能抓同图探索→任务的信号) ②换区
  边沿(stage 有效值变化; -1 不参与, 加载画面不误触发) ③战斗区外(原有)。跟踪变量
  g_artLastInRealQuest/g_artLastStageId(Present 线程专用, -1 哨兵首读不触发)。
- 地址读全挂时边沿静默退化只剩区外门; DoActionHook 闸门不受影响。
- 构建 md5 `efa2dc7f` (上任 `0373d7d5`); ✅已部署 (游戏退出后覆盖, md5 核对一致)。✅ user 局内验证通过 (2026-10-28): 探索→同图任务清零、换区/区外清零、训练场/探索显示充能不受扰, md5 `efa2dc7f` 定稿。

### 2026-10-27(5) 早按 R+方向被判成 R（3:72=lmt107 / 3:109=lmt342）：[R] 门 b30 推迟到簇档 + 342 补簇

- **现象 (user)**: 两招上 R+方向按早了 → 系统只判到 R（107 走短版、342 走刚一 cond311），
  晚按才判到方向。机制 = 方向属性只在簇窗内写入，而 [R] 门 b30 在早期键（107 f22 /
  342 f36）已开 → 早按 R+方向时输入层只写 [R]，R 路线吃掉输入。
- **342 (纳刀一文字, node72 ActionNo=109, 动画=LMT342)**: 原本根本没簇（mFlag1 只有
  vanilla 形状键），方向永远判不到。按纪律补簇 f56=0xCFFC30CC（342 的 vanilla 見切り档 =
  特殊派生窗 b17+b31 开启帧）+ f110 续到末尾（132f）；vanilla 中段键 f46(0x4001) 的
  b16+b30 一并剥掉（否则 b30 提前 10 帧回归）→ 连段窗 [36,110)→[56,110)。
- **107**: mFlag f22 高位 0x4001(b16+b30) 剥零 → 连段窗 [22,100)→[42,100)，与 f42 簇档对齐。
- **语义后果（user 知悉）**: 两招上 [R] 系路线（107=短版/気刃斬り3、342=刚一）现在从
  档帧才开始可输入，早期按 R 不再缓冲——与长版对称，"档前输入一律不收"。
- 注意: node72 无 R+方向 FSM 边（刚一/接上挑之外），342 的簇目前只让方向"可判"；
  要在 3:109 上接方向路线 = FSM 侧加边（边已在 38 节点模式的 cond308 模板可抄）。
  106/110 若也发现早按误判，同一旋钮（剥早期键高位 b16/b30）即可。
- 部署 md5 `39f0925d` (上任 `43fdc01e`, 备份 w03_00.lmt.bak-20261027-pre107342rdir);
  产物 _blend_export_107342_rdir_fix.lmt/.txt; blend 已保存。待 user 冷启动验证。

### 2026-10-27(6) 方向簇锚点定案修正：攻击结束+1，不是 vanilla 特殊派生窗（107/342 二次修）

- **user 反馈**: (5) 的修法在 107/342 上"还是不行"（早按仍判成 R），但 106 确实能分辨。
- **锚点错误定位**: (5) 把簇档选在 vanilla 特殊派生窗帧（107 f42 / 342 f56），但两招的
  攻击窗分别在 f25-29 / f34-42 就结束——user 命中后自然按键落在档前 13/14 帧，方向仍未
  使能 → 全被 [R] 吃掉。106 "能用"不是特殊派生窗锚对了，而是它攻击结束(f43)与档(f44)
  正好贴合，命中后按键 ≥ 档。
- **定案锚点 = 攻击窗结束+1**（与 106 现状一致）：107 簇+b16/b30 提前到 f30（新增键，
  f42/f100 原键保留无害），342 到 f43（新增键；vanilla 中段键 f46=0x4001 恢复原值闭合
  b30 缺口 → 连段窗 [43,110) 连续）。b30=[R] 门与簇同帧解锁，R 系/方向系对称。
- 部署 md5 `41f5ec3f` (上任 `39f0925d`→中间 `0be3e3f9` 未出用户手, 备份 pre107342anchor);
  产物 _blend_export_107342_rdir_fix2.lmt/.txt; blend 已保存。
- 待 user 冷启动验证: 107/342 命中后按 R+方向应即判方向; 若仍偏, 下一个旋钮 = 档帧微移
  (107 f30 / 342 f43 直接挪)。342 的方向 FSM 边仍缺（node72 只有刚一/接上挑），要路线
  需 FSM 侧照 cond308 模板加边。

### 2026-10-27(7) 早按误判真因=[R]门含b14(低16位漏剥) + 切换窗统一定案 + 342 定性无方向

- **user 二次反馈**: (5)(6) 修完 107/342 "还是不行"，但 106 确实能分辨。复盘: (6) 的
  "攻击结束+1" 锚在 107(f30) 上方向判定已通(user: "72现在没这个问题了")但**切换窗太早**
  (f30 即可切，吞掉 107 后摇); 且 (5) 只剥了高 16 位 b16+b30，**低 16 位 b14 漏剥** —
  b14 同属连段位族 = [R] 门的另一路，早按仍写 [R] → R 路线照吃输入。106 "能用"纯属
  user 按键时机天然落在其档(f44)后。
- **user 定性**: 3:109=lmt342 是特殊一文字，**本就不该有 R+方向** → 342 全部退回 user
  基线 (簇拆净、mFlag f36/f46 高位恢复 0x4001/0x4001、mFlag1 恢复 0x1000-0x3000 形状);
  真正出问题的是 fsm 70(lmt110 気刃踏步斩) 和 fsm 72(lmt107 気刃斬り上げ)。
- **切换窗统一定案**: 连段位族 b14/b16/b30 (含[R]门) 与簇**同帧**在 vanilla 特殊派生档
  开启: 107 → f42 (f30 键降级为基线), 110 → f58 (f38/f46 剥 b14/b16/b30; f38 原值
  0x4000 纯 b14, 首版误写 0x0100 引入 b8 已修正)。档前按 R/R+方向一律不收 (死区)。
- 部署 md5 `e5c5e93a` (上任 `41f5ec3f`, 备份 pre107342anchor 沿用); 产物
  _blend_export_107110342_switchwin.lmt/.txt; blend 已保存。
- 待 user 冷启动验证: 107 切换时机 (f42 若偏早/偏晚直接挪簇三键), 110 早按不再出短版。
  342 的 109 请求如需接其它路线另议 (当前设计无方向组合)。

### 2026-10-27(8) [R] 门真因定案 = mFlag1 基线位 b12/b26（非连段位族），107/110 三修部署

- **user 三次反馈**: (6) 的攻击结束+1 锚修完 107/110 "还是不行"——早按 R+方向仍只判到 R，
  必须"顿一会"。复盘: (5)(6) 剥的 b14/b16/b30 是**连段预输入缓冲**（动作层）的使能位，
  与 FSM 的 [R] 属性无关；[R] 的写入门 = **mFlag1 基线位 b12/b26**——铁证 = 306 事故链
  (10-15): 恒定全开(b12/b26 从f0) → 按特殊纳刀立即打出踏步斩([R] 即活), 入场保护=全关。
  107/110 早期键的 b12/b26 一开, 早按 R+方向 → [R] 照写 → 短版/刚一 照吃。106 "能分辨"
  = user 按键时机天然落在其档(f44)后, 与位无关。
- **修法 (107/110 对称)**: mFlag1 基线位 b12/b26 从早期键剥零、推迟到簇档(簇值
  0xCFFC30CC 自含 b12/b26) → [R] 与 [R+方向] 同帧(f42/f58)解锁, 档前一律不收(死区);
  **vanilla 连段位 b14/b16/b30 原样恢复**(动作层预输入缓冲用, 剥它无益且伤 △/○ 预输入;
  110 f38 原值=0x4000 纯b14, (7) 误写 0x0100 引入 b8 已修正; 342 (7) 已按 user 定性
  退回其基线=特殊一文字无方向, 本轮不动)。
- 若本版后早按仍误判 → [R] 无门(裸按键态), 唯一解 = FSM 侧给 cond307 加
  [レバーニュートラル] AND 子节点(wp03 无先例, 属性写入门未验, 短路线有全灭风险)。
- 部署 md5 `aa17a876` (上任 `e5c5e93a`); 产物 _blend_export_107110_baseline_defer.lmt/.txt;
  blend 已保存。待 user 冷启动验证: 107(f42)/110(f58) 早按 R+方向 应无输出, 按住到档出长版。

### 2026-10-27(9) "方向比R晚"=预输入缓冲头start, 107/110 四修: 缓冲位与簇同档全对称

- **user 验证 (8) 后**: 误判消失 ✓, 但 R+方向手感比单独 R **晚**。机制: (8) 恢复的
  vanilla 连段位 (b14/b16/b30, 动作层预输入缓冲) 从早期键就开 → R 按下即被缓冲捕获、
  在引擎自己的取消点(≈f33 早于档)消耗 → 短版先行; 方向只有 FSM 活值一条路, 只能等簇档
  → R 占先手。方向路线无缓冲通道可给, 只能收 R 的头start。
- **修法**: 107/110 预输入缓冲位 b14/b16/b30 再剥到簇档 (107 f22/f30 剥、留 b8 翻滚;
  110 f38/f46 剥; f46 高位一并剥) → 档前除翻滚外全死区, 档上 R=短版 / R+方向=长版
  同帧解锁, 无任何占先。(8) 的基线位推迟保持不变 — "基线推迟 + 缓冲剥除" 两件套首次
  同时在位, 此前任一版只做了一半。
- 部署 md5 `23503aa8` (上任 `aa17a876`); 产物 _blend_export_107110_symmetric.lmt/.txt;
  blend 已保存。待 user 冷启动验证: R 与 R+方向 时机应一致; 残余体感差 = 一文字长起手
  动画本身(带冲刺), 非输入侧。

### 2026-10-28 col 伤害第一轮调参：无双斩/弹反 MV+破坏值

- **路由实锤**（全 entry CTO 扫描，部署 LMT md5 `23503aa8`，blend 无漂移）：345 刚一→
  node63→行25（独占）、156 弹反→node89→行49（与 311 大居合赤共行）、339/340/342 一文字→
  node54→行17（与原版 106 気刃斬り2 共行）、343 无双斩→f0 node91→行51 + f58 node92→行52
  （独占）。10-19 档案"node90/94 重指"未落部署：行42/45=与51/52同值的未引用副本；
  names 空闲节点实为 85/90/94/98/99/100。全部新招=借原版节点，col 结构从未扩。
- **user 定案**：行49（大居合族）与行25（见切行）可随意用（原版主不在用）；一文字行17 不动。
- **改动**（col_tool setval，备份 `wp03.col.bak-20261028-preMVtune`）：
  行51（无双斩一刀目）MV 31→40 + Cond_MV 26→0（vanilla 本 0，残留会在条件命中时把 40
  顶成 26）；行52（二刀目）MV 150→80、破坏 2.0→1.5；行49（弹反156）MV 39→56、破坏 1.0→1.5。
  行17/25 未动。col md5 `dc5b4bcf` → `76c4798e`，结构校验过（20408B/61names/13banks/59ATK）。
  col 不在 blend 内，无回填问题。待 user 冷启动局内验证。
- 同日追加 A/B：弹反破坏仍嫌高 → 行49 Part_Modifier 1.5→1.2（MV56 不动），md5 `76c4798e` → 待查新值。

### 2026-10-29 战技条打地形也充能：大型怪物目标过滤落地

- **现象（user 报告）**：攻击非生命目标照样充战技条——怪物攻击改变地形生成的可破坏地形
  （打中会碎的那类）命中也 +5%。根因：dmgCalc 连接命中门只过滤了攻击者（本地玩家锁），
  没过滤受击方；充能（战技/勇气 chargehit/练气 gainhit/timerhit）全部只看"连接了没有"。
- **HunterPie 对照**（E:\MHW-arts\HunterPie 源码）：Games/World/Damage/Hooks.cpp 的
  DealDamage 钩子第一参=受击目标（Common::Monster，**id int32 @ +0x12280**），
  `if (!Utils::IsLargeMonster(target->id)) return;` 只统计大型怪——WorldUtils.cpp 两个
  u64 bitset（低组 bit59=训练场大桩，特意计入）。
- **逆向链**：本地 dump 与 HunterPie map 421810 同构建（FUN_DEAL_DAMAGE=FUN_141cc5f80
  在 dump 中存在，其第一参直接读 +0x12280）。我们的 dmgCalc=FUN_141f66dd0（≠HunterPie
  钩的那个函数，签名不同不能照搬 a1=目标 的结论），调用者=dispatcher FUN_141f71960
  damage case，**a2=interaction param_2[3]=受击体**直接透传。dispatcher 内可见受击体
  +0x20 处模块带 +0x12278 指针（em 家族字段）；FUN_141bae4e0 显示游戏用 resolver
  （func_0x000140a16d00）从实体解析出 em 再读 id——em 根/包裹体关系未完全钉死。
- **方案**：不引入新钩子，dmgCalc a2 上做三候选探测（本体→+0x20→+0x8）逐个 IsReadable
  后查 HunterPie bitset；任一候选命中=充，全读得出但都不命中=跳（地形正淘汰），全读不出
  =**fail-open 照旧充**（模型错了不静默回归，HITTGT 日志一局就能钉死真相）。门在
  NotifyPlayerHitConnected 本地玩家检查之后、所有充能块之前。日志 `HITTGT`（前 16 条）
  + `g_nonMonsterSkips` 计数。
- 部署 md5 `efa2dc7f` → `a6da7295`。待 user 冷启动验证：打大型怪 HITTGT verdict=1
  （id=真怪物 id）+ 战技条照涨；打地形 verdict=0、条不涨。若怪物命中显示 verdict=-1
  fail-open，下一步把 em resolver 0x140a16d00 拉进 sig 补真正的解析调用。

### 2026-10-29(2) art 充能读取点搬到 DealDamage 钩子（按伤害充能地基）

- **user 定案**：FUN_DEAL_DAMAGE 比 dmgCalc 更好——没生命的东西也能打中并**出伤害数字**，
  说明它们走同一根函数；且充能要改按伤害算，读取点一步到位。
- **新钩子 HkDealDamage**（FUN_141cc5f80 = HunterPie FUN_DEAL_DAMAGE，dump 与其 map
  421810 同构建已实证）：9 参 void 签名照抄 HunterPie typedef（em*, int damage, float* pos,
  BOOL isTenderized, BOOL isCrit, unk0, unk1, char, attackId）。序言 48 字节零通配
  （`cmp dword [rcx+0x12280], 0xF` 内嵌怪物 id 偏移，无 rel32/rip-rel），sig_check 单点
  唯一命中，--emit/--verify 全绿（TARGETS 补 dealDamage 条目）。
- **充能逻辑搬家**：ARTGAIN 块从 NotifyPlayerHitConnected（dmgCalc 门）整体搬到
  `NotifyArtChargeApplied`：arg1 就是 em → +0x12280 直读 + IsLargeMonsterId，无三候选
  无 fail-open；arg2=damage 进日志（ARTGAIN 带 dmg=）为下一轮 charge-dmg 语法做地基；
  攻击者=新增 `g_lastHitAttacker` 锁存（DmgCalcHook 入口记录——DealDamage 无攻击者参数，
  dispatcher 先 calc 后 apply，DDART 日志前 8 条盯锁存质量）。timerhit/gainhit/chargehit/
  旧 HITCHARGE 留在 dmgCalc 门（HITTGT 三候选门）不动，少一层重新验证。
- **行为变化**：0 伤害/弹刀命中不再充（DealDamage 只在伤害落地时发）；多段每段各发一次
  与原先一致。残留风险：环境伤害（落石砸怪）若不经过 dmgCalc，锁存是上一击的——DDART
  日志能暴露； HunterPie 链式共存无冲突。
- 部署 md5 `a6da7295` → `f95d5a24`。待 user 冷启动验证：打怪条照涨（DDART latch=MATCH、
  ARTGAIN 带 dmg 数值）；打地形/木桶不涨；log 出现 "Deal damage hook installed"。

### 2026-10-29(3) 练气地形漏充两路全堵：充能总汇聚到 DealDamage 门 + 原生回充锁存归零

- **user 局内反馈**：art 条地形不涨了 ✓，但"我们补充的涨练气的还是涨"。
- **读 log 定案两路漏**：①dmgCalc 三候选门在漏——HITTGT 同指针 0xD52F240 判定翻转
  （verdict 0/1 都出现过，id=6357090 垃圾值），且同一次真怪物命中 log 出 a2=0xE51F240
  vs DDART em=0x41C2EA1D30 → **a2 是小 victim 上下文对象不是 em**，候选探测在探子对象
  上读随机小整数，误放行 gainhit/chargehit。②**游戏原生练气回充对地形命中照样发**——
  GAUGEEVT +0x2124f13 val=0.12~0.26 跟在地形命中后面（art 门管不到游戏自己的代码）。
- **修法**：①timerhit/gainhit/chargehit/旧 HITCHARGE 从 NotifyPlayerHitConnected 整体
  搬进 DealDamage 侧 `NotifyAppliedHitCharges`（原 NotifyArtChargeApplied 改名扩容），
  em 直读门一个管全部；三候选探测/ResolveLargeMonsterTarget/HITTGT 全删。
  ②GaugeAddHook 复用 takeover 模式：caller=+0x2124f13 且 val>0 且
  `g_lastAppliedTargetMonster`（DealDamage 每击设置，DDART→GAUGEEVT 顺序 log 实证）
  为 false → 归零，日志 GAUGERULE terrain: suppressed（限 40）；负值消耗/见切回填
  （不同 caller）/rewrite 全不碰；锁存默认 true=未判定前放行。
- NotifyPlayerHitConnected slim 化（只剩 HITATT 诊断 + 他方命中计数）；DmgCalcHook
  a2 语义注释修正（a2≠em）。
- 部署 md5 `f95d5a24` → `aa794daf`。待 user 冷启动验证：打地形/木桶 art 条、勇气条、
  练气红条全不涨；打怪三者照常；log 见 GAUGERULE terrain: suppressed。

### 2026-10-29(4) 训练场木桶放行：{57,58} 进可充名单

- **user 局内验证(3)**：地形命中战技/练气/气刃全不涨 ✓✓✓；但训练场**木桶**得照涨——
  原版普通攻击打中木桶回充（user 干净测试：冷启动 4 击=2 木桶+2 敌）。
- **log 挖出练习目标三件套 em id**：57/58=木桶（被拦，DDART id=57/58 + terrain
  suppressed）、59=大桩（本在 HunterPie bitset，ARTGAIN dmg=139 正常充）。HunterPie
  只放了 59，57/58 是它家没统计的木桶。
- **修法**：`IsChargeableTargetId(id)` = `IsLargeMonsterId(id) || id==57 || id==58`，
  NotifyAppliedHitCharges 的判定换用之——art/勇气/练气/原生回充一处放行全生效。
- 部署 md5 `aa794daf` → `ac853959`。待 user 冷启动验证：训练场木桶三条全涨，
  真地形照旧不涨。

### 2026-10-29(5) 基于伤害的战技条+气刃值积累落地（伤害缩放档）

- **user 验收(4)**：木桶放行后整体 OK，进入下一问题=按伤害充能（战技条+气刃值 0x2374）。
- **设计（user 早期共识：先落伤害缩放档，攻击力归一留待挖到字段后一行切换）**：保留逐招
  权重、整体按伤害缩放——`charge-dmg <标定伤害> [封顶%]`（art.txt）/
  `chargehit-dmg <标定伤害> [封顶条]`（gauge-rules.txt）。生效时额度=配置行额度 ×
  (伤害÷标定)，封顶截断；伤害 0 不充；**删行=回固定额度**（解析器开头复位保证热重载
  可退出）；chargestart（100 收刀回避起手充）不缩放。
- **实现**：全局 `g_artDmgScale/Cap`、`g_chargeHitDmgScale/Cap`；缩放点在
  NotifyAppliedHitCharges（em 门后、伤害现成）：ARTGAIN 的 pct 与 chargehit 的
  resolved amount（ctx/from/move/wildcard 全档通用）；GAUGERULE chargehit 日志加 dmg=。
- **部署标定**：按 log 典型伤害 ~100 定标——art `charge-dmg 100 15`、valor
  `chargehit-dmg 100 0.30`（100 伤害=原额度，与上一版手感衔接）。
- 部署 md5 `ac853959` → `bb590658`。待 user 冷启动验证：大伤害招充得快/小伤害慢/
  弹刀不充；手感不对改标定数或删行 Shift+F6 即回退。下一步（挖到攻击力字段后）：
  标定数换 攻击力×系数 = 配装免疫。

### 2026-10-29(6) 攻击力字段追捕开轮：伤害公式反完，DDART 带候选值待局内收口

- **user 定案**：充能不应按裸伤害，应按 伤害/攻击力；HunterPie 查证=IPlayerStatus.RawDamage
  接口存在但 World 数据源未实现（只有 Rise/Wilds 实现）→ 自挖。
- **静态链**（dump 反汇编+反编译）：FUN_1402c3a60(命中处理)→FUN_1402c1050(攻击上下文
  填充)→FUN_1402bc4a0(公式)：damage = MV(ctx+0x34←atkparam+0x20) × (zone×ctx+0x7c) × …
  其中 **ctx+0x7c ← interaction+0x54**（物理分支）= 有效攻击力候选；ctx+0x38(固定伤) ←
  atkparam+0x24（与 memory-map col ATK 行布局吻合）。特殊伤害类型(0x2a/0x2b)走玩家统计
  getter **0x141216910/20 = movss [rcx+0x64]/[rcx+0x60]**（X=*(damageCtx+0x10)=攻击者）
  → player+0x60/+0x64 浮点候选。
- **诊断落地**：mod 版新增 HitDispatchHookMod（锁存 interaction，HkHitDispatch 签名已有），
  DDART 日志（前 12 条）扩为 `p60= p64= cand54= mv20=`。判定法：user 报状态画面攻击力 →
  跟随 buff 变化且数值吻合者胜；cand54 应随攻击力 UP 药/猫饭同步变化。
- 部署 md5 `bb590658` → `ef487b60`。待 user 局内：打怪几下、吃个攻击力 UP 再打几下，
  把 DDART 行发回；钉死后把 charge-dmg/chargehit-dmg 的标定数升级为 伤害/攻击力×系数。

### 2026-10-29(7) 攻击力候选判负+结构扫描上轮

- **DDART 候选全灭**（user 局内数据）：cand54 随伤害等比（dmg/cand54≈1.18 恒定，
  user 判断=红刃伤害补正类固定倍率，即伤害中间量非攻击力）、p60/p64 垃圾值、
  mv20=0（组件读法错）。user 参考=UI 攻击力 1366→1399（攻击 UP 食前后）。
- **修法=结构扫描**：ValorGauge.atkscan.txt 写期望值（≤8 个 float），armed 后前 3 次
  命中各扫一遍 player+0x0..0x20000 / weapon+0x0..0x8000 / w03p+0x0..0x2000，
  float ±3 匹配全打偏移（ATKSCAN 日志，单次限 40）；Shift+F6 重载重新武装。
  攻击力变化前后各扫一轮，偏移交集=字段本体。
- 已写入参考值 1366/1399。部署 md5 `ef487b60` → `6f12be17`。待 user：冷启动（或
  Shift+F6）后打 3+ 下怪 → log 发回；若攻击力已变回去，改 atkscan.txt 的值再
  Shift+F6 重扫一次，两轮交集钉死。

### 2026-10-29(8) 扫描 v1 判 0 命中 → v2：int32 双模式 + 页粒度查内存

- **v1 结果**（user 局内）：3 扫全 0 命中（±3 float），卡顿=逐 4 字节 IsReadable
  ~36k 次 VirtualQuery 在游戏线程。user atkscan.txt 加到 3 个期望值仍 0。
- **推断**：攻击力不是 float 存在被扫区域——可能 int32 存储、或区域/偏移不够大。
- **v2**：float ±3 ∪ int32 精确双匹配；区域扩 player 0x30000/weapon 0x10000/w03p
  0x4000；IsReadable 改页粒度（(off&0xFFF)==0 时才查，36k→40 次，卡顿消除）；
  DDART 加第二公式源候选 atk158=*(float*)(atkParam+0x158)（ctx+0x7c 的另一来源，
  若随 UI 攻击力跨 buff 变化则取代 cand54）。
- 部署 md5 `6f12be17` → `0f533798`。待 user：atkscan.txt 填当前 UI 攻击力 →
  Shift+F6 → 打 3+ 下 → ATKSCAN 行回传。

### 2026-10-29(9) 扫描 v2 判 0 → 公式现场 dump 轮

- **v2 结果**：3 扫仍全 0（float±3 ∪ int32，player/weapon/w03p 三区域）→ 判定：
  组合攻击力 1366/1399 不以单一数值落地（UI 现算），DDART atk158/mv20 组件链读法
  也全无效（=0）。静态猜偏移路线判负。
- **v3=钩公式本体**：新 HkDmgFormula = FUN_1402bc4a0（玩家伤害公式，写出 FUN_1402c3030
  应用的伤害浮点对；sig 0x2BC4A0 48B 零通配唯一命中，TARGETS+json 已同步）。
  arg2(rdx)=FUN_1402c1050 构建的攻击上下文——公式必乘攻击力，**值必在结构里**。
  钩内 dump ctx +0x30..0x110 float/int 双视图（FORMULACTXf/FORMULACTXi，每次重载
  前 10 击）。钉到字段后下一轮把值锁存接进 charge-dmg/chargehit-dmg 的归一化。
- 部署 md5 `0f533798` → `9cce5b95`。待 user：冷启动打 10 下以内怪，FORMULACTX 两行
  ×N 组发回（对照 UI 攻击力 1366/1399）。

### 2026-10-29(10) 公式 ctx dump 判读：ctx 无攻击力 → 表示值/3.3 扫描轮

- **FORMULACTX 判读**（10 击 dump）：+34=131~212 随招式（MV 档）、+5c=18.9~28.8（固定伤/
  属性类）、+c0=688.784 恒定但随 +d0/d8/e0/e4/e8 坐标族（位置速度非攻击）、+7c=1.0、
  无 1366/1399/414 任何形式 → **ctx 不携带攻击力**，公式内部经 vtable getter 取。
- **新假设（关键常识补课）**：MHW UI 攻击力=**表示值 = 真实攻击力 × 武器倍率（太刀
  ×3.3）**——内存存的是真攻 1366/3.3≈414 / 1399/3.3≈424，v1/v2 扫的表示值当然 0 命中。
- **v3 扫描**：LoadAtkScanConfig 自动把每个显示值追加 /3.3 变体（≤16 值），int32 精确 +
  float ±3 双模式照旧。atkscan.txt 内容不变（写显示值即可）。
- 部署 md5 `9cce5b95` → `10a1576e`。待 user：Shift+F6（或冷启动）打 3+ 下，ATKSCAN 行
  回传；命中后与攻击力变化两轮交集钉死字段，接 伤害/攻击力×系数 归一化。

### 2026-10-29(11) 真攻 /3.3 扫描也判 0 → 公式 ctx 锁存 + 食 buff 对照实验轮

- **v3 结果**：/3.3 真攻变体仍 0 命中 → 表示值假设也不落地于三区域。放弃内存搜索路线。
- **转实测定性**：DmgFormulaHook 锁存公式 ctx（g_lastFormulaCtx，公式先于 DealDamage
  执行，锁存新鲜），DDART 加 `atkMV=*(float*)(ctx+0x34)`。已知 +34 随招式 131~212。
- **判别实验（user）**：同一招打桩 3 下 → 吃攻击 UP（1366→1399）→ 再同招 3 下。
  atkMV 按比例 1399/1366 变 = 攻击力承载字段（attack = atkMV×100/MV[col]，归一化分母
  就位）；atkMV 不变 = 纯 MV（攻击力经其他 getter 进公式，路线再定）。
- 部署 md5 `10a1576e` → `8df3bee4`。待 user 对照实验 DDART 行。

### 2026-10-29(12) atkMV 定性=攻击力×MV×部位伤害中间量；终公式成形 + MVPTR 探针

- **数据判读**（user 同招 3:85 踏込斬り×12）：atkMV 95.3~171.7 连续变、dmg/atkMV 恒定
  ≈1.18（=斩味×怒い固定项；25% 会心 12 刀无 1.25 跳=会心不在此链）。col 59 行 MV 全表
  4~126、踏込斬り族 18/24/21/22——**没有任何 MV=95** → atkMV=运行时预合成
  "attack×MV/100×zone"（95.3≈414×0.24×0.96 吻合）。
- **终公式解锁**：charge = rowAmount × K × damage × MV[招式] ÷ (atkMV×100)
  ——attack 分子分母精确抵消=伤害/攻击力，MV 保留（大招多充，MHGU 手感），部位/会心
  成噪声透过。需要 per-招 MV：①ctx 带 atkParam 指针则零静态表（MVPTR 探针：ctx+0x0..0x40
  每 qword 当指针试读 +0x20，值域 4~130 即候选，限前 10 击日志）②找不到则走 col 静态
  actId→行路由表（col_tool 扩展）。
- 部署 md5 `8df3bee4` → `3a8e1ec9`。待 user：冷启动打几下（FORMULACTX/MVPTR 行回传）；
  攻击力承载性最终确认=吃攻击 UP 前后同招同部位 atkMV 比例变化（+2.4% 预期）。

### 2026-10-29(13) user 终公式定形=伤害/1366×K（1366=含武器+药+猫饭的组合攻击力）→ 钩属性对象 X

- **user 定案**：公式=这招实际伤害/1366×K，不需要各招 MV；1366=武器自身+药+猫饭的组合
  攻击力（=显示值语义）。语义与现行 charge-dmg 的标定数同构，差别只在组合攻击力要不要
  活读（活读=吃 buff 免手动重校）。
- **组合值三轮扫描判 0 后的新目标**：FUN_1402c1050 的 *(mgr+0x10)=X=攻击者**属性对象**
  （既非玩家实体也非武器，此前从未扫过；固定伤 getter 0x141216910/20 读 X+0x64/X+0x60
  证明它是攻击力家系）。新 HkAtkCtxBuilder 钩之：STATSOBJ 0x+0x40..0x100 float/int 双
  dump（前 10 次）+ 锁存 g_lastStatsObj 备最终除数。
- 部署 md5 `3a8e1ec9` → `15f84281`。待 user 冷启动打 10 下内回传 STATSOBJ 行，对照
  1366/1399（或 /3.3 的 414/424）找字段。

### 2026-10-29(14) 暴击标志接入=user 要的"有没有触发暴击"一直在签名里

- **user 定案**：会心率影响积累，希望能检测暴击。事实：DealDamage 签名（HunterPie
  typedef）第 5 参就是 BOOL isCrit（第 4 参 isTenderized 软化也在），零逆向成本。
- **实现**：NotifyAppliedHitCharges 加 isCrit 参；crit 抽出 dmgNorm = damage÷1.25
  （MHW 世界物理暴击 ×1.25；负会心 ×0.75 无标志，罕见放行），charge-dmg/chargehit-dmg
  两条伤害缩放路径全部改用 dmgNorm——暴击不再使充能摆动。DDART 加 crit=/dmgNorm=
  字段可对照验证（暴击刀 dmg≈1.25× 邻刀而 dmgNorm 持平）。
- 顺带：user 猜想 1.18/1.25 的 dmg/atkMV 恒定项=锋利度补正——两 session 间 1.18→1.25
  漂移吻合斩味变化，采纳；常数项最终折进 K。
- 部署 md5 `15f84281` → `dacf428f`。待 user 冷启动验证：暴击刀充电与平刀一致
  （DDART crit=1 的 dmgNorm 应≈邻刀 dmg）。

### 2026-10-29(15) 攻击力追捕收官：五轮判负退场 + 暴击归一化落定 + 诊断代码清理

- **追捕总结论**：组合攻击力（1366/1399=武器+药+猫饭）UI 现算不落地。猎捕轨迹：
  ①玩家/武器/w03p float±3+int32 扫描（含 /3.3 真攻变体）0 命中；②公式 ctx=攻击力×MV×
  部位伤害中间量（非纯攻击力）；③属性对象 X +0x60=300/+0x64=1.0 恒定；④MVPTR 候选
  120.588 常量假阳性；⑤cand54=伤害/1.18 无独立信息。sig 全部留档 sig_check.py
  （atkCtxBuilder 0x2C1050 / dmgFormula 0x2BC4A0 / dealDamage 0x1CC5F80）。
- **user 定案采纳**：公式=伤害/1366×K，1366 语义=显示攻击力÷系数，进 charge-dmg/
  chargehit-dmg 标定数（手动更新：换装备/常驻攻击力变化时改一个数）。1.18/1.25 恒定项
  =锋利度族补正（user 猜想，两 session 漂移吻合），折进 K。
- **暴击归一化（user 需求）**：isCrit=DealDamage 签名第 5 参（零逆向成本，isTenderized
  第 4 参亦在），dmgNorm=damage/1.25 进两条伤害缩放路径；DDART 带 crit=/dmgNorm=。
- **退场纪律执行**：ATKSCAN 扫描器（含 plugins/ValorGauge.atkscan.txt）、FORMULACTX/
  STATSOBJ/MVPTR dump、DDART 候选字段、HitDispatch 交互锁存钩子全部移除；保留
  DmgFormulaHook 纯锁存一线（atkMV 路线休眠可复活）。旧 ATK trace 设施（AtkScanThread
  等，前 session 产物）未动。
- 部署 md5 `dacf428f` → `26eb95ca`。充电终态：目标过滤（大型怪+木桶 57/58）→ 伤害
  标定数缩放（暴击归一）→ 封顶；攻击力漂移=手动改标定数。

### 2026-10-30 chargehit 暴击乘数（user 定案回归手写行）

- **user 定案**：气刃值充能回归手写 chargehit 行（伤害缩放撤收），加一个参数=暴击触发时
  额度的乘数。格式对齐说明：user 例"0.3(勇气) 0.01(非勇气)"顺序与现行行相反，保持现行
  **非勇气在前、勇气在后**，暴击乘数加为第 5 可选参：
  `chargehit <set> <id>[@prev] <非勇气额度> [勇气额度] [暴击乘数]`。
- **实现**：GaugeRule.critMult（缺省 1.0），parser 第 5 参（>0 才收），应用在 valor 档位
  选择与 chargehit-dmg 缩放之后（crit 刀 amount×乘数），日志 GAUGERULE chargehit 加
  `critx1.1` 标记。chargehit-dmg 行从 live 配置撤收（伤害缩放只保留在战技条 charge-dmg，
  暴击归一化 dmgNorm 仍在）。
- 部署 md5 `26eb95ca` → `bff54bea`。待 user 冷启动：给要强化暴击的行手补第 5 参，
  暴击刀 log 出 critx 标记、充能=额度×乘数。

### 2026-10-31 勇気納刀触发变晚诊断 + 107/110 "106 化"修复（簇窗结构终案）

- **现象 (user)**: 新 blend（R+方向组合版）下 fsm70(lmt110 気刃踏込斬り)/fsm72(lmt107 気刃斬り上げ)
  勇気納刀"没失效但触发变晚"; 其余动作（127/101/105/104…）全部符合预期。user A/B（FSM 不动换 LMT）
  排除接线; 旧 LMT 下两招纳刀时机准确。
- **诊断链**: ①新导出 `w03_00-r+dir-but-shealth-failed.lmt` md5=`23503aa8`=当时部署版=10-27(9)
  symmetric 态（无谱系漂移）。②新旧 LMT 全轨 diff：107/110 差异**仅** PlMotionInput mFlag/mFlag1
  （无 mTrigger/防御位/时长变化）；其余 entry 逐字节同——与"只有这两招变晚"完全吻合。
  ③机制 = 10-27(8)(9) 把 107/110 早期键的 **mFlag1 基线 b12/b26** 与 **mFlag 连段缓冲 b14/b16/b30**
  全剥到 0、推迟到簇档（107 f42/110 f58）→ [R] 写入门关闭 → 勇気納刀（[R] 按住→特殊纳刀代码路由，
  FSM 无边、纯输入层——部署 FSM node6/38 无→61 边实锤）档前死区 20 帧（107 0.37s→0.70s、
  110 0.63s→0.97s）。106 逃过是因为 (3) 只剥簇**留了 b12**（f24=0x00001000）——user 指认
  "106 既能 R+方向又能正常纳刀"即此。
- **106 结构解码（标准答案）**: 基线 b12 早开 + 缓冲位 vanilla 早开 + **簇=引擎取消点**
  （106 攻击结束 f43→簇 f44）+ FSM 边序 cond308(长版, R+方向) 在 cond307(短版, 裸 R) 之上
  → 检查点上方向必赢：无误判（(5)-(8) 问题）、无硬切（(3) 问题）、无死区（今天的问题）。
  10-27(7) "106 能分辨纯属按键时机落档后" 的判读修正为**结构性成立**。
- **修法（blend=MHW-Valor-0.0.2-damage-modification.blend 活会话 MCP 改键，.150=107/.153=110，
  16 位通道整轨重建 EXPO）**: 107 mFlag1 f22/f30→0x00001000、簇 f42→**f34**(0xCFFC30CC,=取消点
  f33+1) 尾 f100 保持; mFlag f22/f30→0x40014100(vanilla, 含 b8/b14/b16/b30)、f42/f100/f124 原
  vanilla 值不动。110 mFlag1 f38→0x00001000、簇 f58→**f46**; mFlag f38→0x40014000、f46→0x40014100
  (vanilla)、f58/f64/f109 不动。106(.147 实为 104 块, 未动) 与其余 165 entry diff 零变化。
- **验证**: `_shout_semantic_diff.py` 部署版↔新导出全 167 entry **仅 107/110 的 PlMotionInput
  两条轨**差异（433/435/437 双侧解析失败=已知盲区）。三处一致：blend 已存盘 / 产物
  `_blend_export_106style_107110.lmt` / 部署 w03_00.lmt，md5 全=`12caef7e`
  （上任 `23503aa8`，备份 `w03_00.lmt.bak-20261031-pre106style`；blend 备份
  `MHW-Valor-0.0.2-damage-modification.blend.bak-20261031-pre106style`）。
- **待 user 完全冷启动验证**: ①70/72 按 R 纳刀应回到旧时机（107 f22=0.37s/110 f38=0.63s 起）;
  ②R+方向长版切换帧 107=f34/110=f46——偏早（吞后摇）挪 f36/f38（110 挪 f50+），偏晚前移;
  ③红刃态攻击中按住 R+方向应出长版不出短版（簇=取消点构造上无吃窗）。
- **工具遗留**: `_dump_rdir_windows.py`（两 LMT PI 键窗对比）、`FSM-Studio/_dump_fsm_links.ts`
  （部署 .fsm 无头全边 dump; ⚠ mConditionId→conditions() 是 **0 基直引**（card.ts:51），
  减 1 会整体错位一格）。`_test_rdir_switch_window.py` TARGETS=弃用态，勿再跑回。

### 2026-10-31(2) 勇気納刀逆向第一轮：LS 属性 id 全图 + 输入属性表 + 0x2368 写点扫描

- **背景**: (106 化) 后 user 实测"不行"。两版数据修复未根治 → 启动代码逆向（user 开 Ghidra）。
  第二版修复先行部署：107/110 基线键回填 vanilla 精确值 b12+b26=0x04001000
  （上一版 b12-only 是我的发挥，vanilla/OLD 全是双位；部署 md5 `25601581`，产物
  _blend_export_106style_b26.lmt；**user 未测，待冷启动**）。
- **属性 id 全图（FUN_140b306a0=分武器组解析器, LS=case3）**: LS 块 0x269-0x277 =
  0x269 オーラレベル白以上 / 0x26a 赤以上 / 0x26b-0x275 練気ゲージ判定(気刃斬りI,II,III,
  大回転,踏み込み,突き,斬り上げ,J斬り,J二連,見切り,必殺) / **0x276 = 居合斬り** /
  0x277 見切り回避成功後の攻撃ヒット。注册器 FUN_140b2a030（LS 属性描述符链表,
  共享 stub accessor=caseD_0@0x140240720）。
- **全局输入属性 id（FUN_140b2e4e0 strcmp 链）**: 0x26=R, Rキープ, Rリリース,
  0x7d=R押してない, **0x28=R+レバー / 0x27=R+ノーレバー**（摇杆中立对——10-27(8)
  设想的"レバー中立 AND 子节点"有现成属性）, 0x2b/2c/2a/29=R+←/→/↓/↑, Aim/NoAim,
  レバー前/後/左/右キープ, レバーニュートラル(@0x1430b9ca8)。R+○ 六个变体在 0x1430b98bc+。
- **读数据必用 vadump.read_va**: .rdata 段 raw≠RVA 分界在 0x1430ba1xx 附近，裸 VA-0x140000000
  在其后全错（本轮两次误判的根因）。
- **0x2368 (居合架势位) 写点扫描**: 全部 14 处 FUN_14211cc60 调用点在 0x1417d6xxx-0x1417d9xxx =
  **Wp01(片手剑)类代码区**（vtable 注册函数 FUN_1417fa7b0 实锤 Wp03=cKijin 族在
  0x1417fexxx 区、名字表 0x143383xxx, cKijinStepSlash vtable=0x1433846b0）——LS 类不调它。
  LS/Wp03 区写点 0x14180abe0 = 清零函数（word [rcx+0x2368]=0, vtable 方法）; 置位走共享区
  0x14212xxxx（0x142120a1a/120cd0/1223c2/12259b/1245b5/126fbc 六处 c7/89 写）——与 09-19
  "架势=+0x2368"互证：LS 纳刀决策在**共享武器更新代码**，不在武器类。0xC149/C14A/C14B/
  C0A2/C0A4 = 该区动作切换用的 motion begin 码。
- **未解（下一步）**: ①属性值写入闸门（PlMotionInput 位→属性的映射函数本体——
  属性 id 是运行时解析的, 常量 grep 死路; 候选=FSM 条件求值器 FUN_14118a0c0 反推值表偏移,
  或 x64dbg 断 0x14212xxxx 写点回溯调用链）②勇気納刀物理输入到底是什么组合
  （user 说"含 R"；R+○/R+方向/R 决定走哪条闸门位）。

### 2026-10-31(3) 勇気納刀从 act70/72 被R吃：五轮 LMT 修复全败 + 逆向图谱 + 交接（重开会话）

- **问题定型**: 勇気納刀(3:98→306链)在気刃踏込斬り(act70/LMT110)/気刃斬り上げ(act72/LMT107)上
  按键(Ctrl+Space, Ctrl=R2 映射, Space 盘键身份未钉≠○)被 [R] 路线吃掉("给R判定走了"),
  从不落 3:98; 在気刃斬2(**act67/LMT106**, node 12——注意 user 全程用 ActionNo 报招,
  "lmt 106 是 fsm 67 名字気刃斬2")上三键(R/R+方向/纳刀)**开始帧各不相同且全部正常**。
- **已证伪清单(每条附杀死它的测试, 新会话勿重走)**:
  1. 107/110 PI.mFlag1 基线/簇位: b12-only(`12caef7e`)、b12+b26 vanilla(`25601581`)、
     **OLD-exact 整块 swap**(`2dcacb89`, user 实测败)、+106 f24 回填 0xC7FC10CC(`4d6d3d28`,
     败)——107/110 的 **TIML 全部洗清**(OLD↔NEW 全文件 diff 只有 106/107/110/156/342 五条,
     107/110 差异仅在 PI 两轨; swap 后仍败=这两招 TIML 与纳刀无关)。
  2. PI.mFlag4 b28@f22(`ec0633e4`, 现部署, 败)——三招相关性假说(106 f24✓/107无✗/110 f38✗)
     被局内否决。**现部署=ec0633e4**(无害增量, 回退点 `_blend_export_106style_b26.lmt`=`25601581`)。
  3. FSM 加边: node 6/38 →61(条件复用居合斬り cond232/275, 同翻滚后摇 node98 现役同款)——
     user 加了, 败。→ **居合斬り(0x276)属性在 act70/72 期间根本不会被代码置真**。
  4. 类表读数: 0x142EF5C30 + 0x20 stride 对当前 dump **错位**(act67 给 cJumpSlashLanding,
     act86 给 SNS 类名)——9 月旧版地址, 修复前别信; SPIRIT_SHOT §1 早已警告"类名不可靠"。
- **逆向图谱(新会话直接用)**:
  - LS 属性 id(FUN_140b306a0 case3): 0x269 白/0x26a 赤/0x26b-0x275 練気ゲージ判定(気刃斬りI,
    II,III,大回転,踏み込み,突き,斬り上げ,J斬り,J二連,見切り,必殺)/**0x276=居合斬り**/0x277
    見切り回避成功後の攻撃ヒット。注册器 FUN_140b2a030。
  - 全局输入属性(FUN_140b2e4e0 strcmp 链): 0x26=R, 0x7d=R押してない, **0x28=R+レバー /
    0x27=R+ノーレバー**, 0x29-0x2c=R+↑↓←→, レバー前後左右キープ, レバーニュートラル
    (@0x1430b9ca8), R+○ 六变体(@0x1430b98bc+, 无 code xref=指针表间接引用)。
  - +0x2368(架势位): setter FUN_14211cc60 的 14 个调用点全在 **Wp01(片手剑)区**
    (0x1417d6xxx-d9xxx); LS 区(0x1417fexxx, cKijinStepSlash vtable=0x1433846b0, 注册器
    FUN_1417fa7b0)只有清零(0x14180abe0 word=0); LS 置位走共享区 0x14212xxxx 六个写点:
    **0x142120a1a/120cd0/1223c2/12259b/1245b5/126fbc**。
  - 原生链考古(LMT_TIML_Research): 特殊納刀/構え=**原生驱动状态**, vanilla FSM 无 98/99/306;
    更新器 FUN_1421237B0: "动作∈{6,9,98,99} 且缓冲请求==100→起 101; 缓冲请求==1→起 100"
    ——纳刀输入走**请求缓冲系统**(与 △/○ 预输入同族), 非 FSM 边非活值属性。
  - 日志实证(ValorGauge.log): 3:19(翻滚后摇)→3:98 直达✓(该 node 有→61 边); 3:72→恒
    3:106/3:105(一文字短/长 ACT, R 路线)从无 3:98; 1000 系不过 DoAction 日志(1013 判定
    时机查不到 DoAction 层); SHEATHWATCH 里 3:98 期间 2368=151 跳动、3:72 期间 2368=0。
- **模型(当前最优)**: "类代码 × TIML 信号, 按类分别门控"(SPIRIT_SHOT 定案同构)——act67 的类
  有原生纳刀/请求路由, act70/72 的类没有; 输入层只写 [R], 另一组件的分类由类代码完成。
  106 三键三帧=类内三套独立门的实测表现。全轨 diff(106 vs 107/110)剩两个未测候选:
  **PMC.mFlag2 b31 窗(107/110 有 f15/f23=0x8000, 106 整轨缺失!)** 与 PMC.mFlag 107 早期
  0x00400006 形状(106/110=0x02400006)。
- **下一步(按性价比)**:
  1. **x64dbg 动态**: 断 0x142120a1a 等六写点(或 DoAction 0x140269CE0 过滤 id=98/1013),
     act67(成功) vs act70/72(失败) 各按一次纳刀, diff 调用栈=直接拿到门函数;
  2. 静态: Ghidra 反编译 cKijin 系类 update(部分区域未被分析, 需本地 capstone 补;
     vtable 0x1433846b0 槽位已列表)找类内输入处理差;
  3. 工程兜底: DLL krel 同款"按下边沿→force {3,98}"(动作∈{70,72,...} 可配置, 绕开全链);
  4. 钉输入: Space 的盘键身份(键位设定页看图标/納刀态试按)——决定走哪条属性。
- **现场状态**: 部署 `ec0633e4`; master blend=MHW-Valor-0.0.2-damage-modification.blend
  (同步, 备份 .bak-20261031-pre106style); 备胎 `_swap_107110_OLD_exact.lmt`(2dcacb89);
  工具 `_dump_rdir_windows.py`/`FSM-Studio/_dump_fsm_links.ts`(⚠ mConditionId 0 基直引
  card.ts:51)/`_fsm_deployed_links.txt`; 本会话遗留未清: `_tmp_107110_old_exact.blend`、
  各 `_blend_export_*.lmt`。motion-data 簇窗定案节已打证伪标记。

### 2026-10-31(4) PlMotionInput 静态读取链第一轮（以 LMT106/act67 为正常对照）

- 解密 dump 的 `.data` `0x14500102c` 唯一出现类型 hash `0x6FCCD10E`；
  Ghidra xref 指向 `FUN_141921e80`，该函数以名称 `nTimelineParam::PlMotionInput`
  注册大小 `0x60` 的 TIML 参数对象，构造器 `FUN_14231eb50` 清零。
- 参数类 vtable `0x1433c6b50` 的字段反射入口经 `0x140435a90` 跳转
  `FUN_14231f190`：`mFlags[0..7]` 按 4 字节排在对象 `+0x08..+0x24`，
  `mTriggers[0..7]` 排在 `+0x28..+0x44`；字符串表明确含 `mFlag7` / `mTrigger7`，
  旧工具列出的 mFlag..mFlag6 不是该类型的全部反射字段。
- `FUN_141f710a0` 以 `0x60` 步长复制该对象数组，调用方 `FUN_141f4d0f0`
  为玩家对象初始化两个 PlMotionInput 实例（数组头位于玩家 `+0xe480`）。
  这一步只确认实例布局和初始化，不代表已找到每帧的消费函数。
- OLD/NEW 的 106/107/110 PI 轨复核：旧 107 早期 `mFlag1=0x04001000`、
  110 早期 `0xC7FC10CC`，两招旧版纳刀正常；新 106 早期 `mFlag1=0x00001000`
  仍三键正常。结合 OLD-exact swap 败，单靠 mFlag1 基线位解释不了故障。
- 未闭合：每帧求值后各 mFlag 位如何进入输入属性/请求缓冲；以及 Ctrl+Space
  的 Space 对应盘键。当前未运行游戏，无法做 act67 对照动态断点；静态结果不能推出
  “某位就是纳刀门”。后续从 PI 实例/请求缓冲消费点同时采样 67 与 70/72。

### 2026-10-31(5) 107/110 共引 106 的 PlMotionInput 实验（待局内验证）

- user 指定实验：不复制曲线，在 Blender 节点图中保留 LMT107/110 各自的
  TIMLActionNode，只把其 `input_action` 指针改指向 LMT106 的同一个
  `TIML::nTimelineParam::PlMotionInput.149`。对应节点：106=.1644、107=.1656、
  110=.1696；原 PI Action `.150`/`.153` 未删除，便于回退。其余节点连线未动。
- Blender 打开的是 `MHW-Valor-0.0.2-damage-modification.blend`，修改前 `is_dirty=True`；
  先备份磁盘版 `*.blend.bak-20261031-prePI106share`，再用 `save_as_mainfile(copy=True)`
  保存内存态 `*.blend.bak-20261031-prePI106share-live.blend`。实验态另存
  `MHW-Valor-PI106share-test.blend`；主 blend 磁盘文件未覆盖，当前活会话保留共享引用。
- 树导出 `_blend_export_PI106share.lmt`；全 167 entry TIML 语义 diff 相对部署版
  `ec0633e4` 仅 107/110 的 PlMotionInput 四轨不同，导出轨与 106 完全相同。
  433/435/437 两侧均解析失败，为既有盲区；导出日志无 UNSOLVED/CRITICAL。
  110 动作长 116 帧，所引 106 PI 有 f120 末键，导出保留此键；早期窗口仍在动作范围内。
- 游戏未运行时部署；`w03_00.lmt` md5 `ec0633e4` → `2936c09b`，备份
  `w03_00.lmt.bak-20261031-prePI106share`。待 user 完全冷启动对比 act67/70/72
  的 Ctrl+Space→3:98、R→短版、R+方向→长版；若纳刀仍败，106 与 107/110
  已使用同一 PI Action，可排除 PI 曲线内容差异，转查动作类/请求路由。

### 2026-10-31(6) 共引 106 PI 局内成功；旧“PI 洗清”结论撤回

- **user 局内反馈**：act70/72 纳刀已正常，R/R+方向切招也基本正常；残余问题是
  切招稍早。当前部署 md5 `2936c09b` 保持，未再改 LMT。
- 导出 LMT 的 TIML 数据地址 106=`0xB0B10`、107=`0xB7CA0`、110=`0xD5E20`
  （各自 PI type 地址 `0xB0B70/0xB7D00/0xD5E80`）：Blender 三节点共指一个 Action，
  **游戏文件中是三份独立 PI 记录**。因此有效的是所写 PI 内容，非运行时指针别名。
- 成功版相对前版 `ec0633e4` 全 167 entry TIML 语义 diff 仅 107/110 PI 四轨变化。
  成功版两者早期均 f24: mFlag=`0x40014000`、mFlag1=`0xC7FC10CC`、
  mFlag2=`0x00400150`、mFlag4=`0x10000000`。前版 107 f22:
  `0x40014100/0x04001000/0x00400150/0x10000000`；110 f38:
  `0x40014000/0x04001000/0x00400150/0x10000000`。差异集中于 mFlag1
  完整位簇提前打开及 110 的整体开窗提前 14 帧；107 mFlag 早期还少了 b8。
- `2dcacb89` OLD-exact 失败版 107 PI 只有三轨（无 mFlag4），110 早期 f38
  mFlag1=`0xC7FC10CC`，仍败。它不能证明 PI 无关；10-31(3) 的“107/110 TIML
  全部洗清 / 类代码唯一门控”结论撤回。当前只能说 106 PI 全套内容足以修复，
  **具体起效位、开窗帧、是否需要 mFlag4 联合作用均未单独验证**。
- 切招偏早的直接候选：成功版 mFlag1 方向位簇 f24 开，而取消点为
  107=f34 / 110=f46；需把纳刀有效位与方向使能位拆开做 A/B，不能直接把整簇
  推迟（早先推迟整簇曾杀纳刀）。下一轮先在实验独立 Action 上按位族 bisect，
  保留 106 原版 PI 及当前成功部署作为对照。

### 2026-10-31(7) PI 方向位分组延后实验：107=f34、110=f46（待冷启动验证）

- user 同意复制后调时机。Blender 活会话先以 `save_as_mainfile(copy=True)` 保存
  `MHW-Valor-PI106share-live-preTiming.blend`；107/110 各从 106 PI `.149`
  `Action.copy()` 得到独立 Action `PI106-window-split-107/110` 并重指各自
  TIMLActionNode，106 `.149` 不动。实验工程另存 `MHW-Valor-PI106-window-split-test.blend`；
  主 blend 磁盘文件未覆盖。
- 首轮只动 `PlMotionInput.mFlag1` 的 b18-b25 组（mask `0x03FC0000`）：
  107 f24 `C7FC10CC`→`C40010CC`，f34 恢复 `C7FC10CC`，f44 原值不动；
  110 f24 同改 `C40010CC`，f44 `CFFC10CC`→`CC0010CC`，f46 恢复
  `CFFC10CC`，f54 原值不动。其余 PI 轨和 106 全部不动。
  目的：验证早期保留的位能否继续支持纳刀，同时让方向切招回到取消点；
  b18-b25 是否完整承担方向门尚无实证，本版是 A/B 探针。
- 树导出 `_blend_export_PI106_window_split.lmt`；相对已验证成功版 `2936c09b`
  的全 167 entry TIML 语义 diff 仅 107/110 各一条 mFlag1 轨变化；433/435/437
  两侧解析盲区照旧，导出日志无 UNSOLVED/CRITICAL。
- user 纠正部署流程：**LMT 可在游戏运行中覆盖部署**，但读取新 LMT 仍需完全冷启动。
  已运行时覆盖部署 `2936c09b`→`85708b26`（md5 已核对），前版备份
  `w03_00.lmt.bak-20261031-prePI106windowSplit`。待 user 冷启动测两点：
  ① act70/72 Ctrl+Space 是否仍进 3:98；② R+方向切招是否延后至预期取消点。

### 2026-10-31(8) 110→107 接入偏早：单独延后 mFlag2 实验（待局内验证）

- **user 反馈上版 `85708b26`**：107/110 两个问题动作均已修正，仅 LMT110
  接入 LMT107 的时机稍早。部署 FSM node6(act70/LMT110)→node38(act72/LMT107)
  的边条件为 `△ | ○`；当前 110 PI.mFlag2 f24=`0x00400150`、f44=`0x00C003F0`，
  原 110 自身 PI 则从 f38/f58 开同值窗。mFlag2 逐位语义未钉死，本轮仅检验该轨
  是否影响此边的切换时机。
- Blender 活会话先保存 `MHW-Valor-PI106-window-split-live-pre110to107.blend`；
  从已验证的 `PI106-window-split-110` 复制独立 Action
  `PI110-to107-later-mFlag2`，只把 mFlag2 双通道的 f24→f38、f44→f58
  （末键 f120 不动），106/107 Action 和 110 其他轨全部不动。
  实验态另存 `MHW-Valor-PI110-to107-later-test.blend`，主 blend 磁盘版未覆盖。
- 树导出 `_blend_export_PI110_to107_later.lmt`；相对 `85708b26` 的全 167 entry
  TIML 语义 diff 仅 entry110 的 PI.mFlag2 一轨变化；433/435/437 双侧解析盲区照旧，
  导出日志无 UNSOLVED/CRITICAL。已运行时覆盖部署 md5 `85708b26`→`58d0d8d9`，
  前版备份 `w03_00.lmt.bak-20261031-pre110to107later`，部署 md5 已核对。
- 待 user 冷启动：act70/LMT110 使用 △ 或 ○ 接入 act72/LMT107 是否延后至合适时机；
  同时确认 Ctrl+Space 纳刀、R/R+方向仍正常。若纳刀受影响，说明 mFlag2
  也参与纳刀输入，需缩小到 mFlag2 的具体位而非整体移窗。

### 2026-10-31(9) 原版 110 输入窗核对 + PI 前中段整体后移 14 帧（待局内验证）

- **user 反馈 `58d0d8d9`**：只把 110 PI.mFlag2 移回 f38/f58 无效果；整体感觉
  110 的衔接都偏早。故不再把问题归给 node6→38 的 △|○ 单边或 mFlag2 单轨。
- **只读基线** `chunk-extracted/chunk/hm/wp/wp03/mot/w03_00/w03_00.lmt`
  entry110：frameCount=117、TIML 动画长 116；PI 四轨 f0 全零，首个非零键
  **f38**（mFlag=`40014000` / mFlag1=`04001000` / mFlag2=`00400150` /
  mFlag4=`10000000`）；mFlag **f46** 加 b8=`40014100`；主要衔接档 **f58**
  （mFlag=`C003C100` / mFlag1=`0C003000` / mFlag2=`00C003F0` /
  mFlag4=`30000000`），mFlag f64 再升 `C003C300`。值不同于当前 106 PI
  模板，帧锚点可用于本轮 A/B。
- 以此前已验证的独立 Action `PI106-window-split-110` 为来源复制
  `PI110-all-input-plus14`，仅重指 110 TIMLActionNode；106/107 未动。
  四条 PI 轨的所有前中段键 `0<f<100` 整体 +14，f0 与 f100/f110/f120
  尾键保留：mFlag f24/44/45/62→f38/58/59/76；mFlag1
  f24/44/46/54→f38/58/60/68；mFlag2 f24/44→f38/58；
  mFlag4 f24/40→f38/54。110 PI 首窗回归原版 f38；较大簇落 f58-60，
  但各轨值和后续细分帧仍采用 106 模板，并非原版精确回填。
- Blender 活会话先保存 `MHW-Valor-PI110-to107-later-live-preShift14.blend`，
  实验态另存 `MHW-Valor-PI110-all-input-plus14-test.blend`；主 blend 磁盘文件
  未覆盖。树导出 `_blend_export_PI110_all_input_plus14.lmt`；相对部署版全 167 entry
  TIML 语义 diff 仅 entry110 PI.mFlag/mFlag1/mFlag4 三轨变化（mFlag2 已是 f38/f58）；
  433/435/437 两侧解析盲区照旧，导出日志无 UNSOLVED/CRITICAL。
- 已运行时覆盖部署 md5 `58d0d8d9`→`ad6c379b`，前版备份
  `w03_00.lmt.bak-20261031-pre110PIshift14`，部署 md5 已核对。
  待 user 完全冷启动验证：110 的全部派生时机及 Ctrl+Space 纳刀是否仍正常。

### 2026-10-31(10) PI 时机最终局内确认与工程归档

- user 局内确认部署版 `ad6c379b` 的 LMT110 全部衔接时机、勇気納刀时机均符合预期。
  LMT107 仍感觉约早两帧，user 决定保留，不再调整。本轮结果不等同于 PI 最小起效位
  已被逆向钉死；起效位与请求缓冲的逐位映射仍待研究。
- 最终实验工程 `MHW-valor-blender/MHW-Valor-PI110-all-input-plus14-test.blend`；
  活会话最终状态已保存回主工程 `MHW-Valor-0.0.2-damage-modification.blend`，
  覆盖前备份 `.blend.bak-20261031-prePI110final`。最终 LMT 仍为 `ad6c379b`，
  未再改动游戏文件。树导出全 167 entry TIML 语义 diff 见上一条。

### 2026-10-31(11) PlMotionInput 对象绑定链追踪（只读）

- 从 `FUN_141f710a0` 的唯一代码调用点 `0x141f4fd7a` 反查：调用前
  `lea rcx,[rdi+0xe480]`、`edx=2`，元素步长 `0x60`；紧邻的
  `+0xe490` 是另一类 `0x80` 元素数组，不能把这两组混为 PI。玩家初始化后
  `FUN_141fbc8d0(player,0,PI[0])` / `(player,1,PI[1])` 绑定两个实例。
- `FUN_141fbc8d0`：按玩家 `+0x460/+0x468` 选择输入对象布局，将传入的对象
  指针登记在玩家 `+0x6b8 + index*0x40` 的列表；随后经
  `FUN_142236170`/`FUN_1422361b0` 找最多四个下游对象，把列表地址写入其
  `+0x190` 和 `+0x168`，首对象标志 `+0x198=true`。该函数还绑定其他类型
  参数对象，属于通用绑定机制，不能据此判定 PI 位如何生成请求。
- 列表插入函数 `FUN_14231ed80` 逐指针去重，必要时扩容；插入后更新元素计数
  `+0x20` 和变更标志 `+0x24=1`。因此下一轮可从下游对象 `+0x168/+0x190`
  的读取点追列表消费，再沿 PI 实例 `+0x08..+0x44` 的字段访问找掩码和缓冲。
  本轮未动游戏文件，也未取得逐位映射；110 四轨整体后移的局内结果只能约束
  时机，不能单独证明某一轨是纳刀门。
- user 定案当前实践：同类问题先从基本正常的 LMT106 复制 PI 内容到目标动作，
  再按目标原版窗口调时机、局内逐项验证。106 在这轮恰好可作为模板；若再出现
  同类故障，继续逆向消费路径，不把复制法当作通用位映射。

### 2026-10-31(12) 翻滚后摇勇気納刀不通：居合斬り=0x276 黑盒定案 + R+○ 边 A/B 实验（待 user 局内验证）

- **症状 (user)**: 翻滚动作（act 18/19→LMT11、20→12、21→13、22→10；1005=前翻滚聚合，
  1001=气刃斩1聚合）后摇阶段按 R 出气刃斩1 正常，同窗口按勇気納刀只出气刃斩1。
- **FSM 侧排查（部署版 aaa856b9，FSM-Studio 无头 dump）**: 勇気納刀=节点61(act98)，
  入边已铺满 待機2(0)/回避13-16/踏込(6)/斬り上げ(38)/スリまとめ(75)/ほぼ全攻撃(62)/
  翻滚后摇(98,act19 零入边请求进入)，条件全部=「居合斬り」(cond232/275 同一属性)，
  且在回避节点上排在 R 边之前——FSM 不缺边。回避族专用的翻滚后摇节点98是 mod 加的，
  只有出边无入边。
- **属性身份**: 居合斬り = 分武器组解析器 FUN_140b306a0 case3(LS) strcmp 0x1430b51b0
  → **属性 id 0x276**（0x277=見切り回避成功後の攻撃ヒット；全局输入属性表
  0x1430b9680-0x1430b9cc8 逐条 VA→id 已核：R=0x26、R+○=0x3b、R+↑=0x29 等）。
  求值器未定位（0x277 界外 cmp 命中 0x141658aef 是另一状态机非 FSM 条件求值）。
- **LMT PI 静态对比（部署 ad6c379b）**: 翻滚 10-13 的 mFlag1 b12 全程已开（f5/f11/f70 起），
  排除"缺基线位"。与 106/107/110（勇気納刀可用）的差分位 mFlag b14/b15、mFlag2 b22/b23、
  mFlag4 b28/b29 被**无勇気納刀边的原版攻击**（104/105/108/127/101/102/112）同样持有
  → 它们是攻击链通用 profile，非居合斬り闸门。唯一与"能否居合"全吻合的信号 = **PI.mFlag6**:
  待機 LMT1={b11} 常开（可居合）、107/110 无 mFlag6 轨（可居合）、翻滚 10-13 后摇=b13
  （不可居合，恰在 b17+b31 特殊档同帧出现）。312={b16,b17}。按 10-18 先例（PI 位窗
  "局内无效果"=动作类不消费），回避动作类是否消费 mFlag1 簇本身存疑，位语义冻结不猜。
- **实验（一变量 A/B）**: 复用 FSM 里 28 条 vanilla 孤儿 R+○ 条件（cond53，零引用），
  给节点 13/14/15/16/98 各插一条 `R+○ → 勇気納刀#61` 边置顶（原居合斬り边保留在后，
  其余边原序后移）。R+○=全局输入属性与翻滚已验证可用的 R/○ 同族，PI 侧无需新位
  （mFlag1 基线 b12/b26 已开）；若 R+○ 属性在回避动作上可求值即贯通，若不可求值则
  局内零变化（原边仍在）。
- **工具坑**: FSM-Studio FsmModel.addLink 对"单链接直存实例"节点（如 98，mpLinkList
  非 array 而是单 link 实例）会经 list()=null → setList([新边]) **静默丢原边**——
  必须 addLink 后 setList 回 [...linksOf(旧), 新]；conditionSummary 带 `#N: ` 前缀，
  精确匹配属性名用 endsWith(': '+名)。无头编辑脚本 FSM-Studio/.tmp_add_rpluso_edge.ts。
- **部署**: FSM md5 `aaa856b9`→`f25256e2`（备份 wp03_action.fsm.bak-20261031-prerollrpluso），
  全图 dump diff=仅 5 条新边，字节校验 链接名×5、居合斬り×6 不变、LMT 未动（仍 ad6c379b）。
  **待 user 冷启动**: 翻滚后摇按住 R+○ 应出勇気納刀；原 R/○/R+△/△ 派生应不变。
  若无效 → 下一台阶在 blend（MHW-Valor-0.0.2-dodge-sheath.blend）给 10-13 补
  mFlag6 b11 或攻击链 profile 位，再或 DLL 层直接改写请求。

### 2026-10-31(13) 翻滚勇気納刀第二轮：R+○ 边局内判负 → TIML PI 位三件套部署（待 user 冷启动）

- **第一轮判负 (user)**: FSM R+○ 边（f25256e2）局内无效——R+○ 组合属性在回避动作上
  不被求值/消费。边保留（居合斬り 边之上的备援路径，命中任一即出勇気納刀）。
- **旁证新线索**: FUN_14118a0c0 消费的 LS 行 0x143FD5920+0x20 表（{key,ptr,count}×0x18）
  只键控 1012/1013/1014 三个系统状态（=有居合斬り边的三个系统状态节点），条目为
  (set=3,id) 请求对列表+float 参数块——每系统状态的动作集，非输入使能位图；没再深挖。
- **TIML 编辑（blend=MHW-Valor-0.0.2-dodge-sheath, MCP 会话内）**: PI 字段哈希映射实证
  mFlag=m08FD20A6/mFlag1=m6E63FBC7/mFlag2=mF76AAA7D/mFlag6=mF0076E64（键帧值与部署
  LMT 逐键核对）。LMT 10/11/12/13 的 PlMotionInput 各改 3 处现有键值（不加键不动插值）：
  ① mFlag6 尾键 0x2000(b13)→0x2800(+b11, 待機 profile 叠加, b13 保留零回归)；
  ② mFlag 尾键 OR 0xC000(+b14/b15, 对齐攻击恢复档 0xC003C3xx)；
  ③ mFlag2 尾键高16位 OR 0xC0(+b22/b23)。尾键=各 entry 后摇起点（10:f80/11:f65(f42,f50)/12,13:f46(f42,f42)）。
- **导出部署**: MCP exportNodeToFile(inject=True, 母本=部署 ad6c379b 拷贝, tether=m_mesh
  Armature, globalCacheClear)，导出超时 30s 但落盘成功（大文件合并慢, MCP 调用后 sleep 轮询）。
  语义 diff=仅 10-13 的 PI 三轨三值（433/435/437 双侧解析失败=已知盲区）。
  LMT md5 `ad6c379b`→`bf532c83`（备份 w03_00.lmt.bak-20261031-prerolliaido）；
  blend 已保存（dodge-sheath）。FSM 未动（仍 f25256e2）。
- **本轮归因设计**: 三处改动同轮部署（测试轮次贵）；若贯通可选 bisect（先撤 ②③ 只留
  mFlag6，或反之）。若仍不通 → mFlag6/攻击档位全灭，剩余解释=回避动作类根本不消费
  居合斬り/R+○ 求值（312 无类先例同构），路线转 DLL（DoAction 层把翻滚后摇 R+○
  改写为 {3,98}，SHEATHREWRITE 同款）。
- **待 user 完全冷启动**: 翻滚后摇按 R+○；并复查 R/○/R+△/△ 派生无回归。

### 2026-10-31(14) 翻滚勇気納刀第三轮：TIML 判负 → 真闸门=set-3 请求分类器表（user 的 LMT104 线索破案）

- **第二轮判负 (user)**: PI 位三件套（bf532c83）局内无效。user 否决 DLL DoAction 改写
  （要数帧/猜键位），给出关键线索：vanilla 很多动作都能切納刀，如 LMT::w03_00::104。
- **破案**: 気刃斬1=动作 id 11=分类器 key 11。LS set-3 请求表顶层 VA 0x143F4D6A0
  （113 键=动作 id 0-112，条目 {u64 key,u64 子表ptr,u64 count,pad}×0x28；文件里
  count=0 运行时回填，key/ptr/子表行全静态）。子表行 {type,输入组合id,匹配a,匹配b,
  set,目标id}×0x18，解析器 FUN_14026b830（-20 行=方向变体跟随、(-1,-1) 目标行连禁
  下一行=vanilla 禁招机制）、匹配器 FUN_14026ba00（组合检查器对象+过滤链）。
  **带 (1,23,1,0,3,98) 行的 51 键 = 全部可切纳刀动作**（7/11/12/15/29/34-40/67-89/
  92/95/97/101-112，含 key11）；输入组合 23 全表只出现在这种行上=特殊納刀专用组合。
  翻滚 18-22（18/19 共享子表 0x143F4EA50、20/21/22 各自 EC30/ED70/EEB0）无此行
  → TIML/FSM 怎么改都不可能通（两轮判负的根因）。
- **补丁设计 patch_roll_sheathe_row.py**（纯数据, count 不动, 只占死行/休眠行）:
  key18/19 row13 (1,408,0,0,-1,-1)→纳刀行, row14 (1,288,1,0,3,66)→(1,288,0,0,-1,-1)
  自禁用（row13 不再连禁, vanilla 刻意禁的前滚スリF 保持禁用）;
  key20/21/22 row11 (1,278,0,1,1,532)（目标 set1:532 不存在的休眠行）→纳刀行。
  真 exe 未加壳实锤（字符串在文件），6 处行字节逐一验证后出补丁文件
  MonsterHunterWorld.rolliaido.exe（md5 8c39e801，41B diff 全在 5 目标行内），
  原版 exe md5 63cdcf3d 未动。**部署待 user 定夺载体**: 换 exe（游戏须关,备份
  现成）或既有 ValorGauge.dll 启动时内存写同 5 行（无钩子无帧数无键位猜测）。
- **遗留**: 输入组合 23 的物理键位=用户现行切纳刀绑定（与気刃斬1 上完全同一条处理
  链, 零猜测）。窗口问题未解: 分类器何时放行 combo23（过滤链是否读 PI 第二把锁）
  未逆向——若局内表现为翻滚一进就纳刀（过早）, 窗口整形回 TIML/过滤链分析。
- **部署 (user 定案换 exe)**: 游戏未运行确认后, MonsterHunterWorld.exe
  md5 `63cdcf3d`→`8c39e801`（备份 MonsterHunterWorld.exe.bak-20261031-prerolliaido,
  84225952B 大小不变）。FSM(f25256e2)/LMT(bf532c83) 未回退=分类器过滤链若读 PI
  第二把锁, 上轮补的位正好供给。待 user 冷启动: 翻滚后摇切纳刀; 观察是否过早
  （一进翻滚就能纳刀=窗口门在别处）; 回归查 翻滚→R/○/R+△ 与前滚スリF 仍禁。
  仍不通 → 逆向 FUN_14026b670/ba00 过滤链(combo23 放行条件)。

### 2026-10-31(15) exe 校验实锤 → 同 5 行改为 ValorGauge.dll 启动内存写

- **exe 补丁判负 (user)**: 换补丁 exe 游戏启动失败=本体有完整性校验, 原版已还原
  (63cdcf3d)。user 放行 DLL 载体（非 DoAction 改写: 无钩子/无数帧/无键位猜测,
  就是把同 5 行表数据写进内存）。
- **实现**: ValorGauge.dll InitializePlugin 最前 LoadAddressOverrides() +
  PatchRollSheatheRows(): ModuleBaseAddr+RVA(锚点 key11 原版纳刀行 0x3F4F1F0
  六 i32 字节校验, 不符=游戏更新整体放弃) → 5 行 old-bytes 校验后 memcpy 覆盖
  (3F4EB88/3F4EBA0/3F4ED38/3F4EE78/3F4EFB8, RVA=VA-0x140000000, .data 可写无
  VirtualProtect)。日志前缀 ROLLSHEATHE; addresses.txt `rollSheathePatch=0`
  可整体停用。必须在 FSM 管理器拷静态表进 mgrCtx 之前=InitializePlugin 最前
  (LoadAddressOverrides 提前调用一次, InitMemoryMap 里重复读同一文件无害)。
- **部署**: dll md5 `8c2c124d`→`2319237d`（CMake Release, MSVC2022 cmake）。
  exe 已还原原版; FSM(f25256e2)/LMT(bf532c83) 维持不回退（过滤链若读 PI 第二把锁
  正好供给）。待 user 冷启动: 看 ROLLSHEATHE 日志 5/5 → 翻滚后摇切纳刀; 过早/
  不通的下一层 = FUN_14026b670/ba00 过滤链放行条件。

### 2026-10-31(16) 四轮全灭 → SHEATHCLSFY 探针上线（诊断分类器对翻滚的咨询链）

- **第三轮判负 (user)**: ROLLSHEATHE 表行内存写在局内依然无效（user 未确认日志是否
  5/5）。四轮(FSM 边/TIML 位/exe 补丁/内存写表)全灭 → 停止猜测, 上被动探针。
- **探针**: 新 hook id `sheathClsfy`=FUN_14026b830(候选行匹配器, 48B 全精确签名,
  fallbackRva 0x26B830, addresses.txt 名 "sheathClsfy")。detour 只过滤 5 张子表
  (翻滚 18/19 共享 EA50、20 EC30、21 ED70、22 EEB0 + 基线 key11 F100), 结果/
  firstIdx 变化才记行, 上限 300 行: `SHEATHCLSFY key=.. count=.. flag=.. ->
  row=(..)|NOMATCH firstIdx=..`。addresses.txt `sheathClsfyProbe=1` 启用
  (已写入游戏目录)。ROLLSHEATHE 补丁加读回校验。安装点=InstallPoolQueryHook
  mod 分支尾(InitMemoryMap 之后, overrides 已加载)。
- **部署**: dll md5 `2319237d`→`024b0cec`。判读表: 翻滚行不出现=分类器不咨询翻滚
  (类级门) / NOMATCH=组合23未命中(输入态差异) / row=(1,23,..,98) 命中却无纳刀
  =请求被下游丢弃(重映射器/DoAction)。

### 2026-10-31(17) 翻滚切纳刀贯通(user确认能切) → 窗口前移到R窗口帧 (LMT bd34d085)

- **贯通实锤 (log 215064+)**: 红刃期 翻滚(3:18/3:19/3:20/3:21/3:22)→{3,98}请求→
  勇気納刀(entry306)播放→链上3:6踏込斬り。**气刃槽 0x2368=0 时不发**（前段
  11/19 交替无 98、SHEATHWATCH 全 2368=0）=组合23的放行含槽非零门(vanilla 自带,
  未逆向定位)。SHEATHCLSFY 探针整局 0 行 = 运行时消费不经过 b830(挂点白装,
  机制不影响)。
- **user 反馈时机偏晚** → 窗口对齐设计规格"R 能出气刃斩1 的时刻纳刀也能出":
  三组位(mFlag+b14/b15、mFlag2+b22/b23、mFlag6+b11)开窗前移到各 entry 的
  b16(R窗)键帧: 10: f80→f67(长110) / 11: f42-65→f10(长80) / 12,13: f42-46→f26(长76)。
  mFlag6 插新键(双通道同帧, EXPO 与邻键一致), 已有键改值携带新位。
- **坑**: bigFlags 双通道 idx0=低16/idx1=高16——entry11 f50 我把 0xC000 敲成
  0x1000(0x331C0D, b11≠b14), 语义 diff 揪出后修正 idx0=0xC10D(idx1=0x33 不动)。
  ordered=False 假警报=浮点比较, snap 后全序。
- 导出 diff=仅 10-13 PI 三轨(433/435/437 已知盲区), LMT md5 `bf532c83`→
  `bd34d085`(备份 w03_00.lmt.bak-20261031-prerolliaido2), blend 已存
  (dodge-sheath)。DLL(024b0cec)/FSM(f25256e2) 不动。
- 待 user 冷启动: ①时机体感=R 可用即纳刀可用, 过早/过晚单键微调(mFlag6/mFlag2/
  mFlag 的插入键帧); ②前滚(3:19)日志里从未出过 98——前滚位面缺 b31/mFlag2 b4,b6,
  若前滚仍切不出单独处理; ③气刃槽空切不出=v内建门, 观感待定。

### 2026-10-31(18) 窗口对齐R帧判"太早"(没落地就切, 组合23活值即开即切) → 落地帧近似 v1 (bd34d085→e2f050b2)

- **user 实测**: 能切了, 但翻滚没落地就能切(按住方向键翻滚玩家不落地的阶段就被
  切走); 気刃斬1(R)的时机看着是对的。→ 组合23=活值属性(10-27(3) 同款), 窗一开
  即点火, 开窗帧=切出帧, 不能借用 R 的预输入缓冲窗口。
- **新锚点**: R 窗(10:f67/11:f10/12,13:f26)=太早, 二轮尾窗(10:f80/11:f42-65/
  12,13:f42-46)=偏晚 → 落地帧近似取中: **10:f72 / 11:f28 / 12,13:f34**。
  三组位(mFlag+b14,15/mFlag2+b22,23/mFlag6+b11)统一挪到该帧, 早段键还原。
- **坑(通道语义再踩)**: bigFlags idx0=低16/idx1=高16——idx1 的"携带值"必须是
  高半段原值(如 0x4001=b30+b16), 不能写全 u32(0x40010000); 又漏一处 revert
  (129 mFlag[0] f70)。导出 diff 全对后部署。
- LMT `bd34d085`→`e2f050b2`(备份 prerolliaido3); blend 已存。**调时机=单键**:
  每种翻滚一把钥匙(10:mFlag6+ins@72 / 11:@28 / 12,13:@34, 加 mFlag/mFlag2 同帧
  ins 与早段还原), user 报"早/晚"即挪。
- 待 user 冷启动: 落地前后手感; 每种翻滚分别报。

### 2026-10-31(19) "直接不切"破案: 双通道 idx1 超写 16 位被 TIMLJoin 截断 + 崩溃重建丢键 (LMT 898b2314)

- **user 报告不切** → 位置单调性反证(f26 切/f42 切/f34 不可能不切) → dump 部署文件
  实锤 entry12/13 mFlag2 f34=0x150(idx1=0), 设计应 0xC00150。两个根因:
  ① **idx1(高16位通道)键值只能写 16 位半段**(0xC0), 我写成全 u32 风格 0xC00000 →
  parseBigFlag 的 TIMLJoin(pl,pr)=(pr<<16)|pl 高位溢出截断, f34 只剩 0x150,
  b22/b23 缺席 → 与 mFlag b14/b15 不同帧不成立 → 不切。entries 10/11 同类键写的
  0xC0 没踩。修复+全轨 16bit 扫描(已 clean)。
  ② 重建脚本在 Blender4.x 撞 `Keyframe.select` 不存在(改 select_control_point/
  直接删行), 崩溃时 129 mFlag2[0] 已 remove+new 只插到第 1 键 → 后 3 键丢失
  (f70/f72/f80 idx0), 补回。中间态还出过 ordered=False 假警报(浮点噪声)。
- 教训入册: **PI 双通道位键: idx0=低16 位值, idx1=高16 位值, 都是 16 位半段**;
  插键后必查 fcurves 数组序 + remove+new 重建脚本要放异常安全位(先加完键再整理)。
- 终版 diff = 仅 12/13 mFlag2 f34 修复。LMT `e2f050b2`→`898b2314`(备份
  prerolliaido4); blend 已存。窗口= 10:f72 / 11:f28 / 12,13:f34 不变。
- 待 user 冷启动: 落地时机体感 + 每种翻滚分别报。

### 2026-10-31(20) 数据侧放弃 → DLL 方案定稿: 组合23检查器轮询 + DoAction 发 {3,98} (dll 218b9262)

- **TIML 修复版(898b2314) 仍不切 (user)** → 数据侧彻底放弃, 转用户点名的方向:
  "知道游戏按什么键进特殊納刀就无需用户配置"。**解 = 分类器自己的组合23检查器**:
  mgrCtx+0xD0 槽表[23*8] → 检查器对象, +8 flag byte 置 bit0(行 field2=1 的按压
  语义), vtable[6](+0x30) 无参求值 → -1=未按/其它=组合成立。键盘/手柄同链零配置。
- **mgrCtx 捕获**: 新 hook `sheathCap`=FUN_14026c0f0(RVA 0x26C0F0, 分类器入口,
  调用链 14118e4a0←FSM tick), detour 仅存 param_1。调用链静态爬到 141f78663 断
  (未定义区), 改运行时捕获。
- **轮询 PollRollSheathe** (PropIntHook=游戏更新线程, 仿 TryQueuedForceWrite):
  当前动作∈{18,19,20,21,22} 且 motion 帧(anim+0x10C)≥阈值 且 检查器成立 且
  (可关)气刃槽 2368>0 → DoAction({3,98}) 经 g_originalDoAction 直呼 + tracker
  同步。一次翻滚一发(帧回卷重武装)。配置 ValorGauge.roll-sheathe.txt:
  enable/gaugeGate/18..22 阈值, 默认 28/28/34/34/72=TIML 调参近似值。
- 阈值本质 = 组合活值无缓冲, 必须给个"何时接受按压"的帧门——就是当初"数帧"
  反对的部分, 但现在只剩这一个可调参数且用户可改, 键位部分已零配置。
- **部署**: dll `024b0cec`→`218b9262`; 配置 roll-sheathe.txt 已建(9b4382f5)。
  ROLLSHEATHE 表行补丁仍留(其 f80/f42 尾窗=vanilla 路径备用)。FSM(f25256e2)/
  LMT(898b2314) 维持。日志行 ROLLSHEATHE/SHEATHCUT。
- 待 user 冷启动: 翻滚(各向)后段按特殊納刀键 → SHEATHCUT accepted + 勇気納刀。
  调时机 = 改 txt 里对应动作阈值(无需重编)。若 SHEATHCUT rejected 或 chk 恒 -1
  → 检查器求值语义/参数再逆向。

### 2026-10-31(21) 轮询首次进翻滚即崩 → 加固: 武装bit1/vtable界校验/SEH兜底 (dll 2a2f76f6)

- **崩溃定位**: 日志止于首个 act=3:19, 无 SHEATHCUT = 轮询第一次走检查器求值即崩。
  主嫌: ba00 流程里检查器 flag byte bit1(武装)由游戏逐帧对"已武装组合"置位, 翻滚
  从不武装组合23 → 求值解引用未初始化内部态; 或 flag/页写/野 vtable。
- **加固**: ①arm |= 3(bit1 武装+bit0 按压) 且 VirtualQuery 查页可写才写;
  ②checker vtable/求值函数必须在主模块界内(base..+96M)才继续;
  ③求值隔离进 SheathEvalSafe(SEH), 崩溃降级为"未按"+一次性日志;
  ④PropIntHook 每次调用都会跑轮询(高频)——结构上暂不加节流, SEH 已兜底。
- 过程坑: 上轮的编辑把 SheathEvalSafe 插进了 PollRollSheathe stub 体内造成嵌套
  定义/重复定义, 已清理(5267 using 前置, 单一定义 5286)。
- dll `218b9262`→`2a2f76f6`。待 user 冷启动: 崩溃应消失; 看日志 ROLLSHEATHE
  (checker evaluate faulted = SEH 接住了, 求值语义需换路) 或 SHEATHCUT。

### 2026-10-31(22) 检查器求值 SEH 实证必崩 → PROPTRACE: 属性层抓居合斬り真值源 (dll 13b8ae8d)

- **加固版结论 (user: 没崩但还是气刃斩)**: 日志 `ROLLSHEATHE: checker evaluate
  faulted - treated as no-press (SEH)` — 组合23检查器求值**必崩**(未武装态内部
  不完整), 调用路线判死。
- **新路线**: 待機/気刃斬1 的切納刀走 FSM「居合斬り」属性 (cond232/275), 属性求值
  经过 mod 已挂钩的 propInt/propBool getter → **PROPTRACE**: 三个属性钩子加
  变化即记 (本地玩家+结果非零+0→非0跳变, 上限120), addresses.txt `propTrace=1`
  开关, InitMemoryMap 置位。一次成功的待机切納刀即可暴露居合斬り的真值 id/源。
  mod 构建新增 PropBoolHookMod (HkPropBool 签名表里本来就有, 之前只装 int)。
- **过程坑 (本轮三次编译失败全在这)**: dllmain 是匿名命名空间单文件, 函数/变量
  定义位置 vs #ifndef VALOR_PROBE 分支归属极易错位——PropTraceRecord 先后落进
  (a) 误编辑吞掉 PropIntHook 头、(b) enclosing #ifndef VALOR_PROBE(force path
  区, 止于 "force path via prop getter" #endif)内 probe 不可见、(c) extern 局部
  声明 C7631。终态: atomics 在 InitMemoryMap 前, 函数定义在 force-path #endif
  后(无条件区), #ifndef 内前置声明。
- **部署**: dll `2a2f76f6`→`13b8ae8d`; addresses.txt 追加 propTrace=1。
  待 user: ①待機切一次納刀(成功) ②翻滚中按一次(失败) → 看 PROPTRACE 行:
  成功切時出现的 id(预期 0x276 或邻近)=真值源 → 轮询该源或直呼 getter。

### 2026-10-31(23) 属性路线判死(会话仅 3 行环境噪声, 居合斬り不走属性 getter=事件锁存同款) → SHEATHCHK 字节踪迹 (dll 765cb787)

- **PROPTRACE 结果**: 仅 kind=0 id=0x15 ret=50 / kind=1 id=0xB2,0xBA 三行环境翻转,
  无 0x276/邻近 id → 居合斬り条件与气刃条件同款=事件锁存真值(存储未定位), 不走
  属性 getter。属性路线判死。
- **新探针 SHEATHCHK**: PollRollSheathe 重构——检查器获取+字节踪迹前置(待机/翻滚
  都采样, 先于动作门), 400ms 节流读检查器前 48B, 变化即记行(≤80); arm 写挪到
  发射路径前(不污染观测); evaluate 求值 SEH 升级为记录异常码+地址(前5次)。
- **待 user 冷启动**: ①待机按切纳刀键数次(基线=按压态字节) ②翻滚后段按数次。
  日志 SHEATHCHK 行逐字节 diff → 定位"按压位" → 轮询该位=最终形态。
- dll `13b8ae8d`→`765cb787`。

### 2026-10-31(24) SHEATHCHK 首轮: 双 mgrCtx 交替 + 按压不在前 48B → 探针升级 v2 (dll a118a634)

- **首轮解读**: 捕获的 mgrCtx 在两个对象间交替(0x7e46edd0 静态样=+0x28=24(组合数?),
  0x41e5325e60 堆样=玩家翻滚上下文); 发射路径 arm 写痕出现(8=03, 门全过), 求值返回
  未按(本构建无 fault 行); **按压在前 48B 无痕** → 状态在子对象或更深处。
- **探针 v2**: prev 按对象指针分键(修 A/B 互刷); 宽度 48→96; +0x10/+0x18/+0x20
  三个子对象指针各跟 48B; 行内附 act/mot 对齐按压时刻; SEH 升级记录异常码+地址
  (前5次, 上一版已带)。dll `765cb787`→`a118a634`。
- 待 user 重复测试(待机按数次+翻滚后段按数次) → 逐字节 diff 定位按压位。

### 2026-10-31(25) 检查器字节踪迹判死(按压不入前48B/子对象) → 按住语义假设 + SHEATHEVAL 轨迹 (dll 4836c237)

- **SHEATHCHK v2 结果**: 4 个 mgrCtx 上下文交替; 96B+三子对象 400ms 采样无按压痕迹;
  arm 写痕出现(门全过)但求值 -1。→ 调用路线死因新假设: **bit0 语义错** — ba00 按
  行 field2 置/清 bit0 (1=按压沿, 0=按住), 我恒置 1 (沿) — 翻滚期间无按压事件可吃。
- **改按住语义假设**: flag=(flag|2)&~1 (清沿置武装), evaluate = "组合当前按住?"。
  pressEdge=1 可切回沿语义 (config)。加 SHEATHEVAL 轨迹 (翻滚内求值返回值变化即记,
  cap60) — 一次测试同时校准: -1 恒定=按住也不行(检查器路线判死, 转读原始 pad 态);
  r 有值但不切=门问题; r 值与按压相关=活。
- **顺带**: 阈值 config 化后时机调整 = 改 txt 重启, 不再动 TIML/重编。
- dll `a118a634`→`4836c237`。待 user: 翻滚中**按住**切纳刀键不放(过阈值前后都行),
  试各向; 日志看 SHEATHEVAL/SHEATHCUT。

### 2026-11-01(1) 重大转折: 直接翻滚切納刀其实已经能用 (前滚 prev=18 的 3:98 实锤) → Log 加时间戳 (dll 7c2a281f)

- **user 关键反馈**: "有时候翻滚按慢了才出气刃一" = **按时按 → 勇気納刀直接出,
  按晚 → 气刃斬1** — 直接切已在工作, 有时机窗口! 推翻"翻滚不处理组合23"的判死。
- **序列复查**: 本会话两次 3:98 — prev=11 (气刃斬1, 按晚) / **prev=18 (前滚直接切)**。
  按=按住语义 (pressEdge=0) 下 vanilla 消费路径在 LMT 窗口 (f34/f28/f72) 内吃组合。
- **Log() 加 [hh:mm:ss.mmm] 时间戳** (全部行) — 对齐 按压(SHEATHCHK)/触发(MOTIONTRACE
  3:98) 时序, 量化窗口边界。dll `4836c237`→`7c2a281f`。
- 待 user: 再测若干次 (早按/晚按/各向), 时间戳对齐后量化: 窗口实际开闭帧,
  "晚按出气刃一" = 窗口关闭后按 = 符合预期还是需放宽窗口尾部。

### 2026-11-01(2) 4 次 98 触发全解剖 → 多实体 mgrCtx 竞争实锤+修复 (dll 7bd75cdc)

- **时间戳日志全量解析**: 4 次 98 = prev=1(前滚收敛后)/prev=22(后滚直接切!)/prev=1/
  prev=22 — **后滚(act22)两次直接切实锤**, 前滚(18/19)/侧滚(20/21)零触发。
- **根因**: mgrCtx 非唯一 — 日志实证 4+ 个对象随实体 tick 交替(捕获存"最后调用者"
  = 随机实体), 轮询评估的是**别人的组合23检查器**(别人的输入恒未按)。后滚两次
  = 碰巧赶上玩家上下文。也一并解释了此前所有"非单调"怪象。
- **修复**: SheathCapHook 改为 (mgrCtx, actionObj) 成对入表(mutex, 8 槽, 按 mgr
  去重); actionObj = *(owner+0xD90)+0x61C8 与 mod controller(player+0x61C8) 同源
  同偏移 → PollRollSheathe 按 controller 匹配认领**玩家自己的** mgrCtx。
- dll `7c2a281f`→`7bd75cdc`。待 user: 各向翻滚+按住切纳刀 — 预期全向可切。
  SHEATHCHK/SHEATHEVAL 探针保留可复诊。

### 2026-11-01(3) 时间戳日志定量: 机制全解 + 现状行为定案 (LMT 898b2314 + dll 4836c237)

- **最新会话测量** (逐翻滚段): roll18/19/21/20 全部 dur=483-585ms 后出 11(气刃斬1,
  R 分量在 R 窗检查点点出); **roll22 dur=1250/1300ms → nxt=98 @+0ms**(按住到后滚
  结束, 勇気納刀在窗口开启帧=动作末尾切出) — 两次后滚直切实锤。
- **机制定案 (hold 语义)**: 按住 R+○ → 居合斬り边[0](优先级最高)在 PI 窗口开启后
  首个检查点火 → 勇気納刀; 未按/按晚(动作已结束) → R 边[4] → 気刃斬1。
  窗口=LMT PI 位(10:f72-80 / 11:f28-80 / 12,13:f34-76), 槽门 2368>0。
- **"按慢了出气刃一"** = 按压落在动作结束之后(窗口已关) — 侧滚实测 583ms 处 R 出
  気刃斬1 vs 窗口 567ms 开启, 差 16ms 的竞速也在(按住时 [0] 边胜出)。
- **待决 (user 选)**: (a) 现状接受(按住=翻滚任意时刻, 窗口开即切; 按晚=气刃斬1);
  (b) 窗口提前(TIML 单键前移, 按住则更早切, 落地前松手则不切); (c) 窗口延后到
  收敛动作(act1, 需给 key1 子表+LMT4 补行/位)。
- 本轮另: Log() 全行 [hh:mm:ss.mmm] 时间戳; PROPTRACE 判死(居合斬り不走属性
  getter); 检查器调用路线判死(未武装必崩, SEH 兜底); 组合23按压态不落在检查器
  对象前 96B/子对象(v2 探针实证) — 输入检测底层(游戏如何读键)仍未逆向, 不影响
  使用(vanilla 自己检测)。

### 2026-11-01(4) 定稿收尾: 鼠标侧键轮询版部署 (dll d089847a) + 结论归档

- PollRollSheathe 终版: GetAsyncKeyState(VK_XBUTTON1/2) 按住 + 翻滚动作 + 帧阈值
  → DoAction({3,98})。删除 检查器/字节踪迹/mgrCtx 表/sheathCap 捕获残留依赖
  (捕获 hook 仍装但无害), gaugeGate/pressEdge 配置废除。config 重写(阈值表)。
- 部署 dll `d089847a`; 配置 d2e8adc3。LMT 898b2314 / FSM f25256e2 维持。
- 结论入 gauge-valor.md 翻滚切勇気納刀节。代码整理归 user。

### 2026-11-01(5) dllmain.cpp 模块化拆分: 8925 行单文件 → 23 模块 (行为零改动, 双 target 构建通过)

- **拆分方式**: `ValorGauge/tools/split_dllmain.py` 按段表搬运行(逐字节 verbatim),
  不是手抄。脚本自带三重校验: 段首行断言(防错位)、原始 #ifdef 守卫深度逐行核对
  (含 #else 分支翻转、双层嵌套)、全文覆盖审计(8925 行全部要么进模块、进头文件、
  手工补定义、要么是守卫/空行/注释)。生成物 git 6c630fc 的 dllmain.cpp。
- **布局**: src/ 下 log/util/memorymap/player/env/art/valor/spirit/gauge/sheath/
  actions/damage/trigger/fx/motion/prop/rollsheathe/sound/vitals/overlay/probe/
  probe_tools 22 个模块 .cpp+.h + prelude.h(统一 include)/all.h(声明伞)/
  game_types.h(ActionInfo); dllmain.cpp 只剩 LoadForceActionConfig/Install*/
  InitializePlugin/DllMain。CMakeLists 用 VG_SOURCES 列表双 target 共享。
- **纪律**: 函数体零改动(唯 mechanical 修正=SetSpiritLevel 默认实参移到 spirit.h、
  MotionTraceTick/AtkScanArm/PollAtkTrace/ApplySheatheMotionPatch/g_atkKeyWasDown
  去 static 改跨 TU、PropInt 三指针定义移 prop.cpp 头部); mod/probe 分界仍靠
  #ifdef VALOR_PROBE, 各模块重包守卫(原文嵌套结构经脚本验证)。
- **两处有意偏差(均无行为差异)**: ① spirit.cpp(SetSpiritLevel/ExtendLevelDropTimer)
  从 mod-only 改为双构建编译——原 probe 靠 Release /OPT:REF 丢弃未引用的
  PollValorLifecycle 才没炸链接, 现在显式可链接(叶函数无 probe 专属依赖);
  ② FormatHex32/FormatFloat32 同理落 probe_tools.cpp(原在 probe 区)。
- **坑记**: 首轮类型重定义(C2011)=头文件已带类型定义而段表没切掉原文定义行
  → HookId/ArtChargeRow/GaugeRule/MotionBind/ClkRegion/ActionSoundRule/五组
  fn-ptr 别名全部改为"类型在头文件、段表剔除"; prop.cpp 三 getter 指针定义被
  连带切掉 → LNK2019, 生成器手工补定义块。
- 构建验证: ValorGauge.dll + ValorProbe.dll Release 全绿 0 警告。部署未动
  (nativePC 仍是 d089847a), 待 user 冷启动后按需部署新 dll。

### 2026-11-02(1) COMBOPROBE 探针: 特殊納刀"逻辑动作层"定调, 检查器链静态钉死, 运行期观测上线

- **user 情报(本轮起点)**: 特殊納刀实体键不是固定 R+○——有手柄朋友是 R+A 或 R+B
  看个人习惯。⇒ combo id 不是物理键掩码, 是**逻辑动作层**(键位配置→逻辑动作→
  combo), 物理键不可枚举, GetAsyncKeyState 侧键轮询只是过渡。目标改为找到
  游戏内部"特殊納刀输入态"字段, 直接读它(零配置、兼容任意绑定/手柄)。
- **静态链补全(dump 反编译, 补 10-31 缺口)**:
  - `FUN_14026c0f0` = 分类器入口: {set,id} 取自 param_2+0xAC/0xB0(同 mod 已知
    控制器当前请求偏移, +0xBC/+0xC0 为改写位), 调 b670(匹配驱动)+bdd0。
  - `FUN_14026b670`: mgrCtx+0x80+set*0x10 = 该 set 子表基, +0x88+set*0x10 =
    行数; 动作记录 0x20 字节/动作, +0x10 起为候选行表指针数组, +0x18 起行数。
  - `FUN_14026b830`(候选匹配器)→`FUN_14026ba00`(检查器评估): 检查器 =
    `*(mgrCtx+(row.type+0xC)*0x10) + row.comboId*8`; **mgrCtx type 槽 =
    {指针数组,数量} 对, ptr=+0xC0+type*0x10, count=+0xC8+type*0x10**
    (b380 越界检查实证)。+8 旗标字节: **bit1=1→本帧该组合不可用(直接 -1)**,
    bit0 由 ba00 按 row.a(按下/持续)写; 评估 = vtable+0x30; 行胜出后
    `FUN_14026b380` 对[起点,胜出行)间旗标可用的候选逐个调 vtable+0x38(消费/复位)。
  - 结果对象构造: 0x1f8 类(vtable 0x142e4f600, @0x144c97c70)容器管理 0x230 步长
    元素数组(vtable 0x142e4fbf0); 0x142e4f640 为多类共享接口 vtable(无定位价值)。
    类名路线死胡同: 0x142e4f2xx 字符串区是 aCharacterMaking 系(角色编辑), 非输入。
  - 检查器具体类与旗标语义仍需运行期 → 本轮探针。
- **COMBOPROBE 实现(rollsheathe.cpp, mod 段)**: ①被动扫描器
  `PollSheathComboProbe`(挂 PropIntHook 帧轮询, SEH 包裹崩一次整体停): 按
  SheathCapHook 认领表取玩家 mgrCtx(actionObj==player+0x61C8), 逐帧扫四 type 槽,
  检查器 {obj,vtbl,+8 旗标u32} 变化才记 `COMBOCHK t<c> c<id>`(快路径=对象未换
  只读旗标); 首次命中记 `COMBOPROBE` 基线行(四槽数量)。②`HkSheathEval`=
  FUN_14026ba00(RVA 0x26BA00, 签名 48 字节全精确已 verify 唯一)评估钩子
  `SHEATHEVAL c<combo> row=(...) fin=<flag> -> <result>`: 只记玩家 mgrCtx 且
  (组合23 或 结果非-1)。addresses.txt `comboProbe=1` 门控, 两路共享 500 行上限。
- 部署 dll `d089847a`→`96912a68`(mod); ValorProbe 同步绿。addresses.txt 追加
  comboProbe=1。待 user 冷启动按键测试(见会话末测试脚本), 数据回来后钉死
  旗标语义→用内部输入态替换 GetAsyncKeyState。

### 2026-11-02(2) 输入系统完全解码: 逻辑动作位集 + 键位重映射表, 内部输入源上 DLL

- **首轮探针数据(11-02(1) 的局内回报)**: SHEATHCUT ×3 accepted(翻滚18@f28/21@f34/
  21@f47, 过渡版侧键轮询工作正常); 基线扫描暴露 v1 bug——首帧全检查器数组都算
  "新对象", 500 行额度一次吃光, 按键窗口零数据。但基线本身给了 c23 检查器真身:
  **vtbl=0x143371ea0**(与 c13-19 同类; 大多数检查器是 0x1431c5278, 另有 0x143371f78
  等族), t1 槽共 798 检查器; 全会话游戏从未问过组合23(SHEATHEVAL 零命中, 唯一
  一条 c290=(1,290,0,14,-1,-1)→1)。
- **vtable→evaluate 静态解剖(金矿链)**: c23 类 evaluate=`FUN_1417bfa90`:
  查 `*(u32*)(ctx+0x8954) bit20` 等门禁+一个浮点阈值后, 调
  `FUN_1417bd120(this, *(u32*)(this+0x28))` —— **检查器 +0x28 = 逻辑输入 id,
  +0x20 = 输入上下文**。`FUN_1417bd120` 过 UI 门禁(_DAT_1451c4640+0x146d5/0x146ea)
  后调万能查询 `FUN_1419a30d0(玩家ctx, 输入ctx, id, press=旗标bit0, mode=+0x30)`:
  外围玩家门禁 `*(玩家ctx+0x7658)→+0xa4/a5/a6` 任一非零=拒绝(翻滚中组合23从不被
  询问的可能原因=动作层门禁), 内层分派 press=`FUN_14122fdb0`/hold=`FUN_14122fe60`。
- **位集结构(按逻辑 id 直接索引, id>>5 选字, id&31 选位)**:
  `+0x780=当前按住, +0x7C0/+0x820=缓冲, +0x7E0=本帧按下沿`; 上层还有
  +0x860/+0x8A0 "已被查询"登记写位。**键位重映射表 RVA 0x3EEABF0: 逻辑动作
  (<0x89)→物理键, 运行时被键位配置改写**(vanilla 静态表: 逻辑23→物理9, 即默认
  手柄 B/○ 位——"R+A 还是 R+B"因人而异 = 这张表运行时值不同, 机制定案)。
  另: `FUN_141230000` 门禁拒绝 id≥0x89 ⇒ 逻辑动作 id 空间 <0x89(+0x9c 特例),
  798 检查器 ≠ 798 输入动作, combo id 与输入 id 是两个空间、检查器 +0x28 持真身。
- **COMBOPROBE v2(修额度 bug)**: 弃全量扫描, 聚焦 c23: 基线一行 `SHEATHIN23:
  mgr/checker vtbl/flag/inputId/mode30/remapPhys/ctx` + 逐帧读四位数集(按
  +0x28 的 id 位索引)变化才记 `SHEATHIN23 h/buf7c0/edge/buf820`。上限 400 行。
- **PollRollSheathe 内部输入源**: 配置 `ValorGauge.roll-sheathe.txt` 新键
  `input=side(默认)/internal/any`: internal=直读 +0x780 按住位(绑定无关, 手柄
  A/B/键鼠全通吃), any=并集(A/B 验证期, SHEATHCUT 行带 [i]/[s] 源标签)。
  FindPlayerMgrCtx helper 抽出共用。
- 部署 dll `96912a68`→`59f429c3`, roll-sheathe.txt `d2e8adc3`→`fa2729a9`
  (input=any)。待 user 冷启动: 看基线行 inputId 是否=23 + remapPhys=用户绑定;
  按纳刀键看 edge/held 转换; 翻滚按纳刀键看 SHEATHCUT[i] 是否触发(内部源局内
  验证)。通过后默认改 input=internal、GetAsyncKeyState 退役。

### 2026-11-02(3) 崩溃复盘: mgrCtx 是每次调用的短命上下文, v3 改调用期捕获

- **v2 崩溃(59f429c3)**: log 实证 c0f0 的 param_1 (mgrCtx) 1 秒内 8+ 个不同值
  交替(全部 actionObj==玩家), 部分读出 vtbl=0x0 全零 ⇒ **mgrCtx = 每次调用
  不同的短命上下文**(此前 10-31 evaluate 0xC0000005 @.rdata 同根因)。探针跨
  调用保留指针再读 = UAF, IsReadable 挡不住"已提交但已释放"的堆页, 撞未映射
  即崩。v2 还漏了实体过滤(全部实体的 tick 都在跑探针)加剧。v1 全量扫描没崩
  纯属运气。
- **有价值的临终数据**: slot23 检查器 +0x28 = **24**(combo id ≠ 逻辑输入 id,
  又一个编号空间); 该 mgr 的 remap(24)=15; 玩家门禁外的输入上下文指针在多个
  mgr 间重复出现 ⇒ ctx 可能稳定, 但要按 (ctx,id) 对+出现次数过滤验证。
- **v3 修复(d2c46973)**: ①捕获移进 SheathCapHook 调用期内(参数有效期=调用期),
  只认玩家 actionObj; ②跨调用只留 (输入上下文, 逻辑id) 值对, map 按 (ctx,id)
  计数, 上限 8 键裁弱, **seen>=8 才可信**(StableSheathPair); ③一切保留指针读
  走 SEH(SheathNibbleSEH, 无 C++ 对象函数内); ④probe/poll 加实体过滤。
  internal 源在未见稳定对时静默回退(input=any 下侧键兜底, 功能不死)。
- 待验证: SHEATHIN23 stable 行(ctx/id 是否收敛、id 到底=23 还是 24)+按键
  edge/held 转换+SHEATHCUT[i]。若 ctx 不收敛 ⇒ ctx 也是短命 ⇒ 内部源要换
  钩子期同步读方案再做一轮。

### 2026-11-02(4) 刚一抢招定因: 翻滚子表竞争行没禁干净, type=-1 补刀 + REQTRACE

- **user 关键澄清**: FSM action id 112 = 刚一的动作 = 替换気刃斬1的攻击动作
  ("可当成气刃一")。log 实证两条抢招链: key19 `SHEATHCUT{3,98}→{3,1}→112`
  (49ms), key20 `98→112`(16ms) — **{3,98} 每次都被游戏自己的分类器在出招
  检查点用 vanilla 行抢先**。
- **vanilla 子表全行 dump(静态)**: key18/19 r17=(1,144,1,0,3,1)、r16=(144→65,
  被 r15 408-gate 连禁)、r19=(52→11); key20/21/22 r9=(1,288,1,0,3,1)、
  r10=(144→1)、r12=(52→11)。此前补丁每键只动了一行(18/19 r13+r14, 20-22 r11),
  144/288→{3,1} 全部漏网; {3,1} 进 mod 链续成 112=気刃斬1 位 = user 看到的
  "翻滚收刀出气刃斩"。288 在 18/19 是 R→気刃斬1(66) 行(11-01 已禁),
  20-22 是 288→{3,1} — 144/288 疑似纳刀输入的翻滚语境组合(combo 语义未钉死)。
- **修复**: kPatches +7 条竞争行禁用(type 翻 -1 = b830 直接跳过; 不用 (-1,-1)
  target 因 gate 语义会连禁下一行)。REQTRACE 上线(actions.cpp): set=3 请求
  id∈{1,11,66,98,112} 记 caller RVA(分类器/FSM/mod 可辨, cap120)。SHEATHEVAL
  过渡键改 result-only(fin 是 b830 递归标志每帧 0/1 交替, 旧键 c300 每帧刷行
  炸穿 400 行额度 → stable 行被饿死, 这就是 11-02(3) 没 stable 行的原因)。
- **内部源已局内验证**: SHEATHCUT[i] ×2 出现 = StableSheathPair 收敛 + bit 读
  有效, 调用期捕获方案成立。
- 部署 dll `d2c46973`→`6b358efa`。待验证: 翻滚按纳刀键应稳定出 98→勇気納刀,
  无 112 抢招; 若仍被抢, REQTRACE 行直接指认发送者。

### 2026-11-02(5) 补丁地址手算事故复盘 + combo52={3,11} 劫持者实锤

- **user 质询时间戳(11-02(4) 后仍不对)**: log 确有毫秒级时间戳; 重看 v4 会话
  (6b358efa) — {3,1} 确实消失了, 新劫持者 = **REQTRACE 3:11 callerRVA=0x26b640
  (=FUN_14026b590 内的 DoAction 发请求点)**, {3,98} 落地 +13~16ms 被 {3,11} 盖掉,
  且 {3,11} 每 ~1.5s 规律复发(翻滚出招检查点反复命中同一行)。
  **来源 = combo52 行 (1,52,1,0,3,11)(key18/19 r19, key20/21/22 r12)** — 上轮
  "无直接证据"故意留下的行, 正是这轮的实锤劫持者。{3,11}=收刀态 R2 请求号,
  拔刀态下进 mod 链出刚一/気刃斬1 动作。
- **手算事故**: 上轮 7 条行地址 stride 算错(17×0x18 心算成 0x168, 实为 0x198;
  9×0x18=0xD8 非 0x90), 5/12 全 skip("unexpected bytes"日志坐实)。教训:
  **行地址必须脚本算+dump exe 逐条 verify, 不许手算**(本轮 11 条已全部
  script-verify ALL OK)。
- **内部源进展**: SHEATHIN23 stable 行出现(ctx=0x7f972000 inputId=24
  remapPhys=15), 且 h=1 精确抓到一次按键 — 调用期捕获+位集读完整闭环。
- kPatches 重写: 7 条修正地址 + 4 条 combo52 = 16 条(原 5 不动)。部署 dll
  `6b358efa`→`6ef7d2e4`。待验证: boot 应见 "16/16 classifier rows patched";
  翻滚按纳刀键 {3,98} 后不应再有任何 REQTRACE 抢招, 动作应走 98→勇気納刀。

### 2026-11-02(6) SHEATHGATE 总闸落地 (user 定案方案): 纳刀后 0.5s 拒绝気刃斬1/刚一族

- **user 定案**: 别再追分类器行了——DoAction 总闸, 触发纳刀后 0.5s 内拒绝所有
  気刃斬1/刚一, 其他情况不拦。理由充分: v5 会话 REQTRACE 实锤 3:112 每 ~1.6s
  从分类器(b590)复发且 98 落地 +16ms 必跟发, 行禁用是打地鼠; FSM 链接边/缓冲
  输入还不走分类器。
- **实现**: ①rollsheathe.cpp 定义 g_sheathGateUntilMs/gateMs(500)/block 表
  (默认 1,11,12,66,112 = 观测到的全部劫持形态 + 気刃斬1 请求号推断值 66),
  SheathGateArm/SheathGateShouldBlock; ②actions.cpp DoActionHook 在
  ApplySheathRewrite **之前**拦(原始 11/12 直接拒, 否则被改写成 112 绕过字面),
  return false + 限频日志 SHEATHGATE swallow; ③窗口触发双路 = SHEATHCUT 落地
  或 动作层 tracker 看到 3:98(覆盖分类器自发 98), probe 构建无此逻辑。
- 配置: roll-sheathe.txt `gateMs=500` / `block=1,11,12,66,112` (热重载可用)。
- 16 行补丁与 REQTRACE 保留(行禁用降噪 + 溯源留证)。
- 坑记: 首轮编译双错 — SHEATHGATE 全局定义放在 LoadRollSheatheConfig 之后
  (C2065), probe 构建 tracker 也调 SheathGateArm(LNK2019, 用 #ifndef 守卫)。
- 部署 dll `6ef7d2e4`→`516ef594`, roll-sheathe.txt→`1059129a`。待验证: 翻滚按
  纳刀键 → 98 后 0.5s 内任何 112/11/66 请求被吞(日志 SHEATHGATE swallow),
  动作稳定落在勇気納刀。

### 2026-11-02(7) 破案: 收刀链其实已成功, 劫持=窗口外的残余; 闸门 3s + UI 实时显示

- **user 观测修正**: 滑窗/残余理论不成立——"3s之后动作都换了两轮了", 劫持是
  立即的。但 16:43 会话序列显示 **98→6→5 勇気納刀链每次都成功**(SHEATHGATE
  swallow 3:11 ×5 同场实证), 3.3s 后用户再翻滚时 {3,11} 才追上来。两个观测
  的调和: user 看到的"出气刃斩/刚一"发生在后续翻滚里, 且固定 0.5s 窗口确实
  盖不住任何超过 0.5s 的检查点。
- **user 定案**: ①gateMs 拉到 3000(3s 够绝大多数动作切换); ②**UI 实时显示
  按键检测**——回应"我们到底找到按键检测了吗"的怀疑, 检测到了亮出来。
- **实现**: overlay.cpp 新增 SHEATHIN 行(Y=136): `SHEATHIN id=%d h=%u b=%u
  e=%u GATE %.1fs`, 检测到输入=绿/闸门生效=黄/空闲=灰; rollsheathe.cpp 新增
  SheathGateStatus(供 overlay) + gateMs 默认 3000; 滑窗重臂
  (PollSheathGateRefresh, PropIntHook 每帧)保留——输入位见到即续期。
- 部署 dll `516ef594`→`53654369`, roll-sheathe.txt gateMs=3000(`8590e0d4`)。
- **待验证判定树**: ①按纳刀键 UI 不变绿 = 按键检测本身错(inputId 24 非该键),
  回炉找检测; ②变绿且 GATE>0 时仍出気刃斬1/刚一 = 该动作不走 DoAction
  (LinkMotion 旁路), 闸门路线到头; ③绿+GATE 有效+不再出刚一 = 收工。

### 2026-11-02(8) user 局内验证通过: 気刃斬1 劫持清零, 检测跨绑定成立, 计时语义定案

- **user 报告**: ①换纳刀键为 T 也能正确显示(UI 黄) = **逻辑输入层检测绑定无关
  成立**(重映射表消化实体键); ②**再没切到过気刃斬1/刚一** = SHEATHGATE 3s 有效;
  ③GATE 从按下时刻起 3s 倒计时, 按住不续期。
- **计时语义定案**: 位集位(h/b/e)=边沿/缓冲痕迹, 游戏消费即清零(v3 会话 h 单帧
  实证), 按住期间 nibble=0 → 滑窗不续期 → 等效"按下+3s"。user 对 0.5s 旧败因
  的猜想(按太早+窗口过期后被当 R)与证据吻合: 后滚 f72 阈值→出招检查点 >0.63s
  正好越过 0.5s。
- **已知代价(记录)**: 纳刀键按下后 3s 内拔刀刚一(收刀态 R2={3,11}→112)也被闸;
  user 未报告受影响。松键+3s 后一切恢复。
- 文档: gauge-valor.md 翻滚切勇気納刀节重写为 ROLLSHEATHE+SHEATHGATE 定稿版
  (行为/实现/按键检测语义/数据侧全灭教训)。AGENTS 进行中同步。

### 2026-11-02(9) INPUTBITS 全位扫描上线 (user 提案: 监测 R 按住/松开条件)

- **user 提案**: 部署 FSM 有 R 按住监测条件(R押してない, .rdata 0x1430B9BE0 注册,
  306 纳刀链松手边用), 可以监测它。user 补充: 该条件是 R 松开时触发。
- **实现**: R 条件的底层 = 同一输入上下文位集的 R 逻辑位取反, 故直接全扫:
  PollSheathComboProbe 每帧读 +0x780(按住)/+0x7E0(边沿) 全 137 位(id<0x89,
  SEH 批量), 变化记 `INPUTBITS on=[id h/e]` 行 → 一次按键测试即建 [实体键→逻辑id]
  映射, R 的 id 一眼可判, "纳刀键是否点亮 R"猜想直接可验。UI 新增 IN=[..] 行
  (Y=158, 青色, 实时按住 id 列表); SheathHeldIds 查询; probe 行数上限 400→1500。
- 用途: ①验证 user 猜想(纳刀键→R?); ②建全键映射后可做精确闸门(按 R 位而
  非盲 3s 窗); ③R押してない/R按住 两方向都覆盖(位亮/位灭边沿)。
- 构建 6ef7d2e4 后继版, 部署因游戏运行 DLL 被锁未完成, 挂后台等退游戏自动拷。

### 2026-11-02(10) 键→逻辑id 映射实测 + 劫持机制终局定案

- **user 实测 (INPUTBITS)**: R 按住 = 逻辑 id {1,6} (即 R按住/R押してない 条件
  底层位); 纳刀键 = **无常驻位**, 仅 id24 的 h 位闪一帧即被游戏消费 (SHEATHIN
  h 跳变后全零); 16/120 = 两键公共常驻状态位(非按键)。
- **user 猜想("游戏觉得按的是 R")证伪** — 纳刀键不点亮 R 位。**真机制**: 纳刀
  combo 的 vanilla 子表行目标本来就是 R 族请求号 — {3,1}=拔刀态 R2(気刃斬り→
  気刃斬1), {3,11}=收刀态 R2(拔刀→SheathRewrite 改写成刚一112)。11=收刀 R2
  请求号(09 年代 FSM 试验档案一致)。闸门 block={1,11,12,66,112} 恰好全覆盖。
- **计时语义再确认**: 纳刀输入即旋即消费 ⇒ 无常驻位可依, 闸门只能时基
  (按下+3s), 滑窗重臂对手感无感(位闪现即续期)。
- **已知代价(user 知情)**: block 含 1 = 纳刀键按下后 3s 内拔刀态 R(気刃斬り)
  亦被拦; stance 流程内 R 走居合 101/102 不受影响; 碍事则删 block= 里的 1。
- UI: IN 行 R 位(1/6)亮时行尾标 " R" 并转红。
- 新 dll `ccb17fed`(游戏运行中, 后台等待部署)。

### 2026-11-02(11) SHEATHINTENT 意图锁存: 早按缓存到起身时机 (user 定案设计)

- **user 剩余问题**: 翻滚中途按纳刀, 起身时应纳刀而非站着不动。归因: 纳刀输入
  即按即消费(位闪一帧), 按早于阈值帧时 SHEATHCUT 无信号可依(尤其 T 键绑定时
  side-key 路径也失效); 3s 闸门拦住了攻击但纳刀请求根本没发 => 站着不动。
  user 方向: 找出气刃斬1 的时机(=翻滚出招取消点), 那个时间点才是发刀点。
- **实现**: ①g_sheathIntentUntilMs 意图锁存 = PollSheathGateRefresh 每帧见
  纳刀位(任意 nibble 位)且当前动作在阈值表内(即翻滚中) => 锁存 gateMs 时长 +
  首次记 SHEATHINTENT armed roll=<id>(cap20); ②PollRollSheathe 输入判定改为
  InternalSheathHeld() || intent —— 早按 => 阈值帧凭锁存补发, 晚按/按住 => 原
  行为不变。锁存时长复用 gateMs(3s), 跨入下一次翻滚也会补发(意图本就是纳刀)。
- 时机微调仍走配置阈值(18/19=28 20/21=34 22=72, 即气刃斬1 窗口近似), 不用重编。
- 部署 dll `53654369`→`6f1101a6`。待验证: 翻滚前段按纳刀 => 起身必纳刀, 无站着
  不动; log 应见 SHEATHINTENT armed => SHEATHCUT[i] 序列。

### 2026-11-02(12) SHEATHEINTENT 竞速免疫布防: 分类器评估命中即锁存 (user 实测定案)

- **user 实测补充**: ①vanilla 双键组合与快捷键的 UI 显示不同(IN 列表不同但
  SHEATHIN 都黄) = 双键点亮原始键位(R={1,6}+○), 组合层再合成逻辑动作 — 逻辑层
  与组合层分开, 符合预期; ②**意图锁存有漏**: 首按大概率无效, "3s 倒计时中按才
  有效" => 位闪现 vs 轮询存在消费竞速: 分类器在出招检查点先消费 => PollRollSheathe
  (InternalSheathHeld)与 PollSheathGateRefresh 两位点都没看到 => 既无 SHEATHCUT
  也无锁存; 倒计时中有效 = 上一次锁存还活着, 与本次按压无关。
- **修复(竞速免疫)**: SheathEvalProbeHook(ba00 钩子)内, 分类器自己报告纳刀族
  combo 命中(result != -1 且 combo ∈ intentCombos 默认 23,52,144 可配)且
  mgrCtx 属玩家且当前动作在阈值表内 => 同步锁存意图+拉闸。分类器调用栈内同步
  执行, 输入此刻必然存在, 消费竞速不可能发生。位集 sighting 路径保留兜底。
- 双键组合说明: 双键 vs 快捷键路径不同但都汇入 c23(REQTRACE 3:98 callerRVA=
  0x26b640 = 分类器自发 = (1,23,→98) 行命中实证), intentCombos 含 23 双覆盖。
- 部署 dll `6f1101a6`→`b7b6042f`, roll-sheathe.txt 加 intentCombos 行。待验证:
  翻滚任意时刻(尤其后摇首按)按纳刀 => 必纳刀; log SHEATHEINTENT armed c<combo>。

### 2026-11-02(13) log 证据反转: 纳刀链其实几乎每次都走, 漏的是"阈值帧未到翻滚已结束" => 起身补发

- **log 全量复盘(17:58-18:00 会话)**: 14 个 SHEATHINTENT 里 9 个走到 SHEATHCUT
  accepted, 2 个由分类器自发 98 完成(34.856 同毫秒 action=98, 故无 SHEATHCUT 行)
  —— **纳刀机制本身近乎全中**。所谓"失败"= 剩余几个意图过期无 98: 按压落在
  阈值帧之后/翻滚提前结束, PollRollSheathe 的 {动作∈阈值表 && mot>=阈值} 窗口
  被跳过 => 意图过期 => 观感"站着不动"。
- **另一发现**: 纳刀成功后 user 按住翻滚+方向/R => 收刀态触发出 拔刀系动作
  (id 7 移動抜刀 / id 11 收刀R2, 36.3-37.9s 实证) —— 部分观感"没纳刀"实为
  纳刀后残余输入拉出的拔刀动作, 非纳刀失败。
- **起身补发 (SHEATHGETUP, user 定案"起身时能纳刀"的直译)**: 意图存续期间,
  动作从翻滚(阈值表)离开且纳刀链(98/5/6/9/10)未开始 => 立即补发 {3,98}+
  tracker 同步+拉闸, 每意图一次(s_getupFired 门闩, 意图过期复位)。
  PollSheathGateRefresh 每帧执行(不依赖 StableSheathPair 之外的门)。
- 部署 dll `b7b6042f`→`fcad1aeb`(游戏运行中, 后台等待部署)。
- 待验证: 翻滚任意时刻按纳刀 => 起身必纳刀, log SHEATHGETUP fire => accepted。

### 2026-11-02(14) Blender PI 窗挖掘 + 后摇本体=动作24 定案, 配置补 24=0

- **Blender PI 导出(翻滚 LMT 11/12/13/10, nodetree w03_00, Data 节点输入挂
  TIMLActionNode 链, input_action 名含 PlMotionInput)**: 使能窗转折点
  entry11(前滚18/19)=f26/f28/f31, entry12/13(侧滚)=f26/f34/f42, entry10(后滚)
  =f70/f72/f77 —— 与现行阈值 28/34/72 基本重合(vanilla 允许气刃斬1 的时机)。
  m6E63FBC7(mFlag1) f11 起 {b12,b18} = 翻滚的 R+方向簇早开(与文档簇位吻合)。
- **结构定案**: 翻滚本体仅 ~27f 即切 **动作 24**(后摇/起身本体, key24 子表含
  (1,144,0,0,3,1) = vanilla 后摇 R→気刃斬1 的真路由); 阈值 28≈翻滚自然终点,
  后摇中按压此前不锁存不发刀(24 ∉ 阈值表) => "后摇按了没反应/3:0 瞬间才切"。
- **修复(纯配置)**: roll-sheathe.txt 加 `24=0` —— 后摇中见输入即发+意图可锁存;
  连带效应: ①按住跨翻滚进 24 即发(早于旧 getup 时点); ②18→24 边界的 getup
  补发被 curIsRoll(24) 自然抑制(无双重发); ③意图过期且后摇中无按压 => 正常
  走完 24 不纳刀。roll 阈值(28/34/72)仍可手动调(=PI 窗起点, 再早压不过 f26)。
- 配置 `76eb76a8`(新增 24=0 行)。dll 维持 `fcad1aeb`。待局内验证: 后摇中
  按纳刀 => 立即纳刀。

### 2026-11-02(15) 关键trace: 翻滚直落3:0无24相位, 阈值帧过而不发 => SHEATHDIAG 上树

- **trace(18:48:10)**: roll20(10.50, entry12) → 用户 10.85 按(24h 位闪) → 意图
  锁存 → **11.769 直落 act=3:0(entry 360)** — **无 24 相位**(前测 18→24 为链滚
  特例); 期间 INPUTBITS 10.969 后静默 = 无位变化(非饿死, 探针 18:48:20 仍活)。
  意图存活期(至13.85)内 mot≥34 早满足, PollRollSheathe 却没发 => 内部某门吃掉,
  非线程饥饿。
- **SHEATHDIAG 上树(fcad1aeb→f31b1038 已部署)**: PollRollSheathe 两点——①
  (id∈阈值表 && intent) 每30帧记 {id,set,inDA,mwa,heldInt,ent,im}; ②过全部门
  临开火前记 {mot,thr}。一次 boot 定位吃门者(mot 读数/inDA/mwa 三嫌疑)。
- f8a01170(SHEATH24 版, 目标 id==24)作废——无 24 相位可诊断。

### 2026-11-02(16) 真凶定案: fired 旗标复位死锁在阈值门后 —— "首按无效"的最终解释

- **SHEATHDIAG 证据链**: ①19:00:40 roll21 意图存活 0.65s+, 门全绿, 无 fire-path
  行 => 流程卡在阈值门(mot<34); ②19:06 roll18 成功段 mot 29→78 连续可读(无饿死
  无 anim 换), 且该段无 SHEATHCUT => **fired 卡死 true**。综合: 复位代码
  `mot < lastMot-5` 位于阈值门(mot>=threshold)之后 => 上一发后 lastMot 冻结≈
  阈值 => 复位条件(mot<阈值-5)与门条件(mot>=阈值)永久矛盾 => **fired 置位即
  永久卡死**; 此前"时好时坏"全靠 24∈阈值表(24 的 mot 从 0 起能穿过阈值门执行
  复位)与链滚 mot 回卷偶发解卡。PropIntHook ~百次/帧(diag %30 节流失效的副证)。
- **修复(9c6a2552)**: mot 读数前置到实体检查后(所有玩家帧跟踪), fired 回卷
  复位随之前置 —— 98/6/5/idle 的 mot 均从 0 起, 任何动作切换落差即复位,
  "每翻滚一发"语义保持; 两处 SHEATHDIAG 移除(job done)。
- 部署 dll `33f0980c`→`9c6a2552`。待验证: 连续多次翻滚按纳刀, 每次都应发刀。

### 2026-11-01 刚一(345) 普通格挡→強G 化: 防掉血数据侧定案 (待 user 冷启动验证)

- **起因**: user 反馈強G(39)不掉血但刚一普通格挡(35-37 削韧族)仍掉血, 要求恢复防掉血;
  优先数据侧。考古 DLL 侧: GUARD 子系统(8f4cfb0 落地: VitSet 0x141216940 拦截+伤害否决/
  回滚+耐力回退+转身钳制)已于 f76c937(10-23)整体移除, mod 版现无任何血量/耐力/命中钩子
  —— DLL 复活路线存在但非首选。
- **考古定案(重要纠偏)**: 10-13"345 加三件套→39 強G 零伤"是 **user 局内验证通过的成品**
  (10-14 双向验证: mFlag b30 + mFlag1 b28 两处都在→38-40, 任缺→35-37; staminaParam 非判定
  必需); 当时"回滚"只是双向验证的最后一步+防御重心移到 306 架势, **不是強G 路线有缺陷**。
  10-24 盘点 node91/92/93(act38/39/40)零出边"播完无处可去"——306 现役強G 先例无异常,
  先零 FSM 改动上, 体验不对再补出边。
- **现值考古(Blender, dodge-sheath.blend=10-31 22:35 权威工程)**: entry345 →
  `TIML::nTimelineParam::PlMotionCommon.348`(nodetree 定位, .348 是 dedup 名): mFlag idx1
  f0=0x9008→f12=0 (=0x90080000 削韧窗), **mFlag1/mStaminaParamIndex 轨不存在**。
- **补丁(三件套, 复刻 10-13 验证签名=长枪 entry28)**: mFlag idx1 f0 0x9008→0xD008(+b30);
  新建 mFlag1 idx1 f0=0x1000→f12=0(b28 窗镜像); 新建 mStaminaParamIndex idx0 f0=10
  CONSTANT(=50 耐力/秒, 306 同款)。全部 CONSTANT 插值(铁律 9)。blend 原位保存,
  备份 `MHW-Valor-0.0.2-dodge-sheath.blend.bak-1101-pre-strongguard345`。
- **导出坑两个**: ①`--factory-startup` 会卸载 freehk 插件→freehk_tether 属性不存在→脚本崩;
  ②LMTFileNode **inject=True 注入模式**: filepath 必须指向已存在有效 LMT(拷部署基线注入),
  否则 `G_MASTER_FILE_ERROR: invalid injection target` 不写文件。已入 tools-workflow。
- **验证与部署**: timl-diff 164 TIML entries **仅 entry 345 差异**且恰为三件套(diff 工具
  自动标签"零伤格挡?(b28)") → 部署 nativePC md5 `d31946aa`(上任 `898b2314`)。回退件
  `_rollback_898b.lmt` = 备份 blend 重导, md5 与上任逐字节一致(**导出确定性验证通过**,
  回退实体化)。
- **行为预期**: 刚一防御成功 → 3:38/39/40 強G 反应(零削韧零掉血, 持续掉耐力——原普通
  格挡路线不掉耐力, 此为行为差异点); 反应动画=定格硬直式(非小后仰)。node91/92/93 零出边,
  反应播完的回退行为待局内确认(306 先例无问题); 备选=node91/92/93 补出边 或 DLL 侧精简
  否决(仅叮信号+35 族反应时回滚, 吸取 d496f93 react-latch 过宽教训)。
- 待 user: 完全冷启动 → 刚一防御窗吃刀 → 应进 38/39/40 定格格挡, **不掉血不掉韧**, 耐力
  持续消耗; 反应结束后可正常操作。

### 2026-11-01 (2) 強G 副作用确认 + chip 掉血逆向破案 + CHIP probe 实验轮部署

- **強G 化副作用 (user 局内)**: 345 三件套 (d31946aa) 防御成功进 38/39/40 零伤 ✓, 但反应
  播完后 **FSM 落到收刀回避动作** = 10-24 盘点 node91/92/93 零出边"无处可去"的实锤。
  user 定调: 要的是**普通格挡 (35-37 反应) 不掉血**, 不是強G 化。
- **chip 掉血逆向破案 (Ghidra, 现役 421810 dump)**: 受击主函数 **FUN_141217e50**
  (param_1=受击上下文/+0x60=player, param_2=hit) 的玩家分支: 防御减伤公式
  `dmg × 80/(80+def)`(fVar1=1.0@DAT_142e4fe58, +80@DAT_142f8aba4, 属性 1-5 各有
  抗性 getter) 后有**免伤分水岭**: `threshold = FUN_1411c10c0(weaponCtrl)`;
  `threshold==0 ∨ threshold<damage ∨ damage<1.0` → 掉血; **damage≤threshold → chip=0**
  (35-37 反应不变)。**FUN_1411c10c0 = PropInt(propMgr,0x94) thunk**(尾跳
  FUN_141922190, EDX=0x94 直达穿透; 属性 id 0x94=148, 非技能空间; FSM 属性注册表
  DAT_145012048=memorymap g_addrPropRegistry 0x5012048)。**属性 0x94 原版无生产者**
  (技能枚举 148 缺号同构) → 阈值恒 0 → 原版格挡必有 chip。**強G (38-40) 走另一分支
  (bVar8: HP delta 写 0) 全免**——与 TIML 位观察闭环。无任何文件配置面 (EXE 静态表
  DAT_145012048 运行时填充, 属性 148 无来源)。
- **调用面**: FUN_1411c10c0 **全程序唯一 xref = FUN_141217e50 内 0x141218C00** (sig
  `48 8B 49 08 45 33 C9 45 33 C0 BA 94 00 00 00 E9` 16 定值字节 dump exe 唯一命中)。
  遗留未知: 普通被打 (非格挡) 是否也过此判定 (代码看是同一计算路径, 需局内分辨)。
- **CHIP probe 实验轮**: 新模块 `chipguard.cpp/.h` (全 VALOR_PROBE; mod 侧空转):
  HkChipThreshold hook (memorymap 枚举+sig+lastRva 0x11C10C0), detour 记 log
  (`CHIP caller=+0x... threshold=N act=S:ID`, 120ms 节流), 配置
  `ValorProbe.chipguard.txt` (`trace=` / `force=N`: 返回 max(orig,N)), **Shift+F7 热重载**。
  坑: dllmain 79/110 的 #ifdef 分支里 152-154 三个 Install* 函数名带 Probe 实际在
  **#else (mod) 分支**——首次 patch 落错块 C3861; probe 安装正位 =
  InitializePlugin 的 probe 块 (InstallPropHistogramHooks 邻)。overlay probe 分支加
  PollChipGuardHotkey()。两 target 零错误零警告。
- **部署**: mod ValorGauge.dll (md5 `6c3e7c62`, 11-01 19:17 build) 改名
  `.hold-chipguard-exp` 暂存; ValorProbe.dll md5 `df952a1c`; chipguard.txt trace=1 force=0。
- **待 user 两轮实验**: ①纯 trace: 裸被打 / 刚一格挡吃刀 / 強G 吃刀 三场景看
  CHIP 行 (caller 是否恒 +0x1218C05, threshold 恒 0, act 值); ②force=99999 (Shift+F7):
  刚一格挡吃刀应 0 chip 且反应仍是 35-37; **同时裸被打**——若也 0 伤 = 判定需条件化
  (防御动作族窗口), 若照常掉血 = 恒 force 即可干净落地 mod 版。
- 后续路线: CHIP 落地 mod 后 345 回滚 0x90080000 削韧态 (普通格挡反应+零 chip 终态),
  node91/92/93 出边不再是必选项。

### 2026-11-01 (3) 345 強G 化回滚定案

- user 定调: 剛一恢复普通格挡态, 零伤交给 CHIP 路线。部署 LMT 回滚 `_rollback_898b.lmt`
  → md5 `898b2314` ✓ (拷贝时游戏在运行——LMT 只读资源无写回风险, 但铁律 4 冷启动
  才生效, user 已知)。blend 侧同步撤三件套 (mFlag idx1 f0 0xD008→0x9008, 删 mFlag1
  idx1/mStaminaParamIndex idx0 两轨) 并保存, **重导验证 = 898b2314 与部署逐字节一致**
  (blend=导出=部署三方闭环)。motion-data/AGENTS 已改注"強G 化实验已回滚"。
- 实验环境就位: 现在刚一 = 0x90080000 削韧格挡 + ValorProbe CHIP hook (trace=1),
  正好是第一轮 trace 实验需要的形态 (普通格挡吃刀看 CHIP 行)。

### 2026-11-01 (4) CHIP 第一轮局内定案 + 条件化版部署 (mod 版前的最终验证轮)

- **第一轮 trace 实锤 (user 局内 + log)**: 所有掉血场景都过 chip 判定 (caller 恒
  `+0x1218c05`, threshold 恒 0): 刚一格挡吃刀 (act=3:112)、攻击中被打 (3:11)、待机被打
  (3:0/3:6)——**格挡与普通受击共用 chip 路径, 恒 force = 全免伤, 必须条件化**。
  **強G 成功的受击不出 CHIP 行** (engine bVar8 分支在 chip 判定前分走, HP delta 写 0)
  —— 22:40 两次 act=3:98 的 CHIP 行 = 架势中侧向受击掉血的那些, 与反编译互证。
- **条件化版 (probe)**: 门 = ModWeaponActive() + 当前动作 ∈ ids (配置, 默认 112) +
  动作帧 ≤ window (秒, instance+8 30Hz 时钟; 刚一防御窗 TIML f0-f12 = 0.20s)。
  gated → 返回 max(orig, force); ungated → orig 原样。gated 命中不打日志 (掉血行
  `gated=0` 供局内核对未 gate 场景)。配置 `ValorProbe.chipguard.txt` 加
  `window=0.20` / `ids=112,86,...`, Shift+F7 热重载不变。
- **跨模块链接两处 (470d852 模式)**: `ModWeaponActive`/`ResolveActionInstance` 从
  player.cpp `#ifndef VALOR_PROBE` 块移出为双构建可链接 (函数体零改动, 只依赖
  IsReadable/g_modWeaponType/IsReadable)。坑重踩: atomic int64 CAS 的 expected
  参数必须非 const 左值 (两次编译错同一根因)。
- **部署**: ValorProbe.dll md5 `78c3960d`, 配置 force=99999/window=0.20/ids=112。
  mod 仍 hold (`6c3e7c62` = `.hold-chipguard-exp`)。
- **待 user 验证 (冷启动)**: ①刚一防御窗 (出招后 0.2s 内) 吃刀 → 零掉血 + 普通
  35-37 小后仰反应; ②刚一防御窗外 (0.2s 后) 被打 → 照常掉血 (gate 没放); ③裸被打/
  空手挨打 → 照常掉血; ④強G (98 架势正面) → 原样零伤。日志核对: gated 场景无
  CHIP 行, 未 gate 掉血有 `gated=0` 行。通过后 mod 版平移 (chipguard 双构建化 +
  mod 配置 ValorGauge.chipguard.txt)。

### 2026-11-01 (5) 防御窗窄窗定案 + f42 加宽 (be22e28a)

- **第二轮现象 (user)**: "大部分格挡不生效, 按早才生效 + 一个没印象的特效"。
  log 实锤: 掉血行全为 `gated=0 act=3:112` = 吃刀时动作帧已 >0.2s, 引擎防御窗
  (TIML f0-f12 off-key) 已关 → 防御判定失败直接挨打 (chip hook 无关, 它只影响
  格挡成功后的掉血); gate 窗与引擎窗一致所以同步不放。**窄窗 = 0x90080000 原版
  配方固有, 非 bug**。
- **"没印象的特效" = 引擎原生零伤格挡呈现**: 反编译 chip=0 分支 (else, bVar5=true)
  置 local_258 bit0 → 函数尾 FUN_140add800 (player-emitter, 与 fxNotify 0xADD670
  同族) 播专属特效。原版阈值恒 0 该路径从未点亮, CHIP 抬阈值首次触发——即零伤
  生效的视觉标志。強G (bVar8 分支) 放的是另一套。
- **加宽**: 345 mFlag off-key f12→f42 (0.70s, 见切级窗感), timl-diff 仅 345 窗
  [0,12)→[0,42), 部署 md5 `be22e28a` (上任 `898b2314`)。gate window 同步 0.70
  (probe 配置, Shift+F7 热重载; LMT 需冷启动)。窗宽手感不合口 = blend 里改
  off-key 帧号重导即可 (gate window 同步)。

### 2026-11-01 (6) 零伤分支抑制反应实锤 + CHIP 退役改 Rollback 设计

- **第二轮现象翻案 (user)**: "格挡不生效" = 0.2s 窗内吃刀触发了没见过的特效但
  **没有 35-37 后仰**——不是防御判定失败! 结合反编译定案: **引擎零伤分支
  (threshold≥damage) 的原生设计 = 伤害 0 → 上游不排队格挡反应**, 只走
  local_258 bit0 → FUN_140add800 特效 (掉血路径才排 35-37)。即 force 阈值越生效
  格挡后仰越消失——用户要的"后仰+零伤"组合引擎原生不存在 (四个互斥分支: 裸受击/
  chip+35-37/零伤+特效无反应/強G 38-40)。
- **翻案附注**: "偶尔生效 (按得早)" = 吃刀帧落 >0.2s (窗关, gate 没放) → 走 chip
  路径 → 有后仰 (原版观感"生效"); 窗内反而被 force 拦成"特效+无反应" (=用户眼里
  "不生效")。gated=0 log 两行与此吻合。
- **方案翻转: 阈值 hook 退役 (force=0 留 A/B), 改 GuardRollback**: 格挡走完整
  原生路径 (叮+35-37+chip), 每帧拍 HP 快照 (非反应态时刷新), **tracker 观察到
  cur∈{35,36,37} 且 HP<快照 (快照新鲜 <800ms) → HP box+0x64 写回快照**。精准性:
  窗外挨打走身体控制器 1:339 族 (武器 tracker 看不到), 不会误触; 反应态内连击
  持续压回; 快照 800ms 窗外 (反应期喝药) 不覆盖。老 GUARD (8f4cfb0) 回滚式
  GUARDBLOCK 的复活, 条件更窄 (prev 仅限刚一切入, 快照每帧刷新替代单槽覆盖缺陷)。
  零伤特效随之消失 (不再进零伤分支)。血条会闪一下回满 (掉血数字+回写), 老方案
  user 局内验证过的观感。
- **实现**: chipguard.cpp 重写 (CHIP hook trace-only + PollGuardRollback 每帧,
  overlay probe 分支调用; probe 侧内联 ProbeVitalsBox/kHpSlot=0x7630 等价访问,
  vitals.cpp 整模块 mod-only 不搬)。配置 ValorProbe.chipguard.txt: trace=0/
  force=0/rollback=1, Shift+F7 热重载。
- **部署**: ValorProbe.dll md5 `21f2f5ed`。LMT 维持 `be22e28a` (f42 加宽窗,
  0.7s)。mod 仍 hold (`.hold-chipguard-exp` = 6c3e7c62)。
- **待 user 冷启动验证**: 刚一防御窗 (0.7s) 内吃刀 → 叮 + 35-37 后仰 + **血不掉**
  (血条闪一下回满, log 出 CHIPROLL 行); 窗外/裸被打 → 照常掉血无 CHIPROLL;
  98 架势強G → 原样。通过后 mod 版平移。

### 2026-11-01 (7) 致死否决补全: rollback 的死亡窗口关闭 (强G 逆向结论附)

- **user 判定**: 血条闪可接受, **chip 致死不可接受** (rollback 事后写回赶不上死亡
  处理); 指令 = 看"強G 为什么不掉血"。
- **強G 零伤机制定案 (log+反编译闭环, 不再深挖)**: 強G 吃刀**连 chip 判定
  (FUN_1411c10c0) 都不进** = 伤害在更上游防御仲裁按载体 TIML 強G 位整条分流
  (零伤+38-40 请求+耐力)。复制它 = 強G 化路线 = 收刀回避副作用, 死路。
  FUN_1412192f0 (92KB 异常/反应分发) 与 FUN_141217e50 内 bVar8 (按攻击者动作
  107/109/113 的特殊处理) 均非強G 路径。
- **死亡问题的正解 = 复活老 GUARD 致死否决 (user 记忆中的"钩血条写入条件拒写")**:
  VitSet `0x141216940` (leaf, 语义: flag!=0 → clamp(max(v,1),max); **flag==0 &&
  value<1 → cur=0 = 致死写**)。detour: 本地玩家 HP box + **GuardGateActive
  (动作=112 && 帧≤window 0.70s)** + lethal 形态 → **丢弃写入** → HP 保持 >0 →
  死亡判定不触发 → 35-37 反应照排。非致死 chip 照常 → rollback 闪回。普通挨打
  致死 (窗外) 照死不误。
- **实现**: HkVitSet (sig 36B 全定值除 rel8/disp32, dump 唯一命中; HookSig
  pattern[48] 上限内); chipguard.cpp 加 VitSetHook + GuardGateActive 公共门
  (probe 侧 ProbeVitalsBox 前置)。坑: HookSig pattern 48B 上限 (63B 版 C2078);
  Ghidra 助记转换两处失真 (COMISS modrm C8 非 CA / MINSS 5D 非 5C) —— sig 必须从
  dump 实读字节构造。配置加 `lethalveto=`。
- **部署**: ValorProbe.dll md5 `f383bf65`, 配置 rollback=1/lethalveto=1/force=0。
- **待 user 冷启动验证**: ①刚一防御窗内小刀 → 后仰+闪回 (CHIPROLL); ②窗内致命大刀
  → **不死**, HP 停旧值 + 正常后仰 (CHIPVETO 行); ③窗外/裸被打致死 → 照死 (veto
  无门不放); ④98 架势強G 原样。全过后 mod 平移 (chipguard 双构建化)。

### 2026-11-01 (8) 防御窗回滚 0.2s 原版窗 (user 定调)

- user 选择原版 0.2s 窗 (f42→f12), blend 同步回改并保存; 导出产物 md5 = `898b2314`
  (与回滚验证件逐字节一致, blend=导出=部署三方闭环)。部署 md5 `898b2314`。
- probe 配置 window=0.20 (lethal veto 门=刚一前 0.2s; rollback 无窗依赖)。
  坑: 配置文件字符串替换污染注释行 (replace 命中注释内子串拆行) → 重写干净版。
- 待 user 局内: 0.2s 窗内小刀后仰+闪回 / 窗内致命刀 CHIPVETO 不死 / 窗外照死 /
  強G 原样。

### 2026-11-02 chipguard 定案入 mod (guard chip 保护常驻)

- **第一局验证 (user, 防御最低装备)**: log 11 条 CHIPVETO 全部拦截致死写 (value 全
  负 = 引擎写 0 形态), 零 CHIPROLL (低防下每刀都致命, veto 后 HP 未动无回滚需求)
  —— 0.2s 窗内格挡不死实锤。
- **mod 平移**: chipguard.cpp/.h 双构建化 (mod 侧 = VitSetHook 致死否决 +
  PollGuardRollback; threshold hook/Shift+F7 留 probe), 配置
  `ValorGauge.chipguard.txt` (rollback/lethalveto/window/guardId/force/trace),
  guardId 可配 (默认 112)。mod Shift+F6 全量热重载含 chipguard; overlay mod 分支
  加 PollGuardRollback。坑三连: InstallChipGuardHook 拆分时 replace 顺序错位出
  悬空 #else (整函数重写解决); chipguard.h 声明仍被 probe 宏包着 (mod C3861);
  配置文件字符串替换污染注释行。
- **部署**: mod ValorGauge.dll md5 `26fc6fb2` (上任 hold `6c3e7c62` = 
  .hold-pre-chipguard-final), probe 撤除, ValorGauge.chipguard.txt 在位。
  LMT 维持 `898b2314` (0.2s 原版窗)。behavior: 刚一防御窗内 = 后仰+闪回
  (CHIPROLL) + 致死否决 (CHIPVETO 不死); 窗外/裸被打全原版; 強G (98/306) 原样。

### 2026-11-02 (2) vetoall: 刚一真·不掉血 (写入层全拦, 血条零闪)

- user 反馈 rollback 可接受但希望完全不掉血。升级: VitSetHook 同一钩子从"只拦致死
  形态"放宽到 **vetoall=1 时窗内一切 HP 递减写全丢** (value<当前 box+0x64 即递减;
  回血递增写放行)。安全性实证继承自 lethal veto 轮: **反应排队在伤害计算层决定,
  写入层拦截不影响 35-37 呈现** (致死写被拦后仰照排已局内实锤)。rollback 保留作
  保险 (vetoall 下 HP 不降自动静默)。窗 0.2s 内非格挡来源 (背后击飞) 误伤面可忽略。
- 部署 mod md5 `408bfc5b`, 配置 vetoall=1。待 user 冷启动: 窗内吃刀 = 后仰 + 血条
  完全不动 (CHIPVETO 行含非致死拦截)。

### 2026-11-02 (3) 方向 A/B 未定案, 回归 rollback 终态 (mod 408bfc5b)

- **vetoall 方向漏洞 (user 指出)**: 窗内背后被打也零伤 (写入层无方向信号)。
  probe A/B (CHIPDIAG 位窗 dump): 7 刀中 5 刀 0x8953=0x30 (bit4/5 = 0xDC/0xDD
  事件确实落位)、2 刀无——但采样顺序与正/背对应关系无法控制 (怪自行转身),
  位语义 (正面=有还是背面=有) 未定案, 方向门证据不足不落地。
- **终态 = rollback 模式 (vetoall=0)**: 方向精准性由引擎方向判定内嵌提供——
  只有真实切入 35-37 (引擎判格挡成功=正面锥内) 才回滚; 背后击飞不进 35-37 →
  照常掉血照常死, 零误伤; 代价 = 非致死 chip 血条闪一下 (CHIPROLL)。
  lethalveto 独立于方向保留 (致死写不论方向都丢——背后致命一击也保命, user
  已接受该形态)。vetoall 留配置档 (想要血条零闪可自行开启, 接受背后 0.2s 免伤)。
- mod 恢复 md5 `408bfc5b` (配置 vetoall=0/rollback=1/lethalveto=1), probe 撤除。

### 2026-11-02 (4) 终局重排: 強G 化刚一 + 勇气纳刀普通化 + 钩子性能修复 (user 定案)

- **rollback 局内未生效**: user 实测格挡成功仍受伤——CHIPROLL 路径自实现以来从未
  被 log 命中过 (前两轮窗内刀全为致死被 veto 拦, 非致死场景未测过), rollback 本体
  首个真实测试即失败 (切入检测或时序有问题, 未及排查——**user 直接定案换路线**)。
- **user 终局重排**: **刚一 (345) = 強G 化** (三件套零伤, 接受 38-40 定格反应及播完
  落收刀回避); **勇气纳刀 (306 架势) = 普通防御** (削韧掉血, 不再零伤白嫖)。強G 零伤
  = 引擎原生, chipguard 钩子整个不需要。
- **卡顿根因与修复**: user 报告特效多时游戏卡顿——VitSet 钩子热路径 (全游戏每次
  伤害写入, 含打怪) 每次做 ResolveLocalPlayerViaManager 链解析 + 两次 g_actionMutex
  抢锁, 伤害事件风暴 = 锁竞争放大。修: ①GuardGateActive 去锁去链 (良性竞争读,
  陈旧锁最坏=门放行一次=原版行为, 安全方向); ②**enabled= 主开关** (0=不装任何
  钩子), dllmain 改为先 Load 配置后 Install。強G 化后配置 enabled=0 = 零钩子零开销。
- **blend 双改** (dodge-sheath.blend): 345 = 三件套 (mFlag f0 0xD0080000→f12 off +
  mFlag1 b28 窗 f0→f12 + staminaParam=10, 复刻 d31946aa 已验证配方);
  306 = mFlag 0x52400006→**0x92400006** (-b30+b31, 普通格挡核) + 删 mFlag1 b28 轨
  (staminaParam=10 保留闲置)。timl-diff 恰 306+345 两 entry 差异。
- **部署**: LMT md5 `7f60cefd` (上任 `898b2314`); mod md5 `37ab8fa6` (enabled=0,
  钩子全关但代码保留, 未来要防死/闪回改 enabled=1 即开)。
- 待 user 冷启动: 刚一防御窗吃刀 → 38/39/40 零伤強G 反应 (播完落收刀回避为已知
  形态); 306 架势吃刀 → 普通 35-37 后仰+掉血; 卡顿应消失 (零钩子)。
