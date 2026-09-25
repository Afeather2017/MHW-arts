# 内存地址查找向导（游戏更新后照此逐步找回）

> **适用场景**：游戏打补丁后 DLL 的 `MEMORYMAP` 自检报错，或需要在全新环境从零重建。
> **前置条件**：新版游戏的 `MONSTERHUNTERWORLD.EXE`、本目录的 Python 工具
> （`pip install capstone`）、Ghidra（静态分析用）。
>
> **使用方法**：先跑第 0、1 章，然后**只做** `nativePC/plugins/ValorGauge.log`
> 里 `MEMORYMAP` 报告为 BAD / `SANITY` 告警对应的章节。每节格式统一：
> 【锚点】怎么认出它 →【步骤】点哪里 →【验证】怎么确认没找错 →【写回】改哪里。
>
> 附录 A/B/C 是全部依赖项的速查表；附录 D 是 F5 动态挖掘向导（静态路走不通时用）。

---

## 0. 准备环境

| 工具 | 用途 | 备注 |
|---|---|---|
| Ghidra | 反编译新版 exe | 任何近版本都行 |
| Python 3.11 + capstone | 本目录所有工具 | `pip install capstone` |
| x64dbg | 运行时 dump（第 1 章用） | 断点 `VirtualFree`/手动 Scylla dump 皆可 |

工具一览（都在 `Reverse/`，**默认读取 dump exe**，不是盘上加密 exe）：

| 工具 | 用途 |
|---|---|
| `vadump.py <va> [len]` | 按 VA 读字节/字符串（UTF-8/Shift-JIS 自动解码） |
| `disasm_rva.py <rva> [len]` | 反汇编任意 RVA |
| `find_xrefs.py <va>` | 扫出 .text 里所有引用某 VA 的指令位置 |
| `scan_277.py` | 扫 `cmp r32, imm32` 立即数（改脚本里的常量可扫任意值） |
| `sig_check.py --emit` / `--verify` | 生成/验证钩子特征码 |
| `fsm_table.py` | 解析每武器 FSM 条件描述符表 |
| `MonsterHunterWorld_dump.exe(.c)` | 上一版本的转储 + Ghidra 反编译（280MB，用作对照） |

## 1. 生成新版 dump 与反编译（一次性）

**为什么必须 dump**：盘上 exe 的 `.text` 段是加密的，直接反汇编全是乱码。
`disasm_rva.py` / `vadump.py` 读的都是解密后的内存转储。

1. 启动游戏到主菜单，x64dbg 附加进程。
2. 记下主模块基址（Symbols 窗口里 MONSTERHUNTERWORLD.EXE 的 start 地址）。
3. 右键模块 → Dump Memory to File（或用 Scylla 的 dump），存为
   `Reverse/MonsterHunterWorld_dump.exe`（先备份旧版为 `_old`）。
4. Ghidra → 新工程 → Import 该 dump → 选中 x86:LE:64 → 自动分析（默认选项，
   跑完约 20-60 分钟）→ File → Export Program → C/DECOMPILER 输出
   `MonsterHunterWorld_dump.exe.c`（本项目用它做全文搜索）。
5. 打开 `vadump.py` / `disasm_rva.py`，确认 `EXE` 路径与 `BASE = 0x140000000`
   （dump 时若基址非默认，需把工具里的 BASE 和所有旧 VA 按差值平移）。

**验证**：

```powershell
python disasm_rva.py 0x462700 0x20   # 应看到 mov rax,rcx / cmp edx,[rcx+0xB8] 等可读指令
```

> 后文所有「旧版地址」均指上一版本，用于对照，不要照抄。

---

## 2. 函数钩子

### 2.1 propInt / propBool / propFloat（FSM 属性 getter，强制路径依赖，最先修）

- **锚点**：三个函数相邻（旧版 `0x1922190 / 0x19221F0 / 0x1922240`，间隔 ≤0x60），
  每个开头都有 `F6 41 60 01`（`test byte ptr [rcx+0x60], 1`）。
- **步骤**：
  1. Ghidra → Search → For Bytes → 搜 `F6 41 60 01 74`。
  2. 会出现三簇命中；每簇内取相距 ≤0x100 的三个函数头。
  3. 反编译验证（见下）。
- **验证**：反编译形状 = `component = *(param + 0x126D8)`（或由调用方传入）、
  `key = *(int32*)(component + 0x84 + id*24)`、然后查全局注册表并返回
  `*(int16*)(row + 0x24 + index*2)`。int/bool/float 三个变体仅返回类型不同。
- **写回**：`sig_check.py` 的 TARGETS 里更新三个 RVA → `--emit` → `--verify`
  → 把新特征码抄进 `dllmain.cpp` 的 `kHookSigs[]` 的 `propInt/propBool/propFloat`
  三项，重编译。急用可先写 `addresses.txt`：`propInt=0x新RVA` 等。

### 2.2 doAction（ActionController::DoAction）

- **锚点**：内部片段 `48 8D 41 07 48 C1 E0 04 46 3B 04 08`。
- **步骤**：Ghidra → Search → For Bytes 搜上述串 → 命中地址 **减 10** = 函数头。
- **验证**：反编译为 `(void* controller, ActionInfo* action)` 两参，访问
  `action+0x10`（set）/`+0x14`（id）。
- **写回**：`InstallActionHook()` 里的 pattern 数组（若片段本身变了，从函数头
  取 24-32 字节、跳过 call/jmp/rip 相对位移做新特征码）。

### 2.3 condEval（条件树节点评估器，旧版 FUN_14118BDA0 / RVA 0x118BDA0）

- **锚点**：属性名字符串 `見切り回避成功後の攻撃ヒット`（UTF-8，内容跨版本不变）。
- **步骤**：
  1. Ghidra → Search → For Strings → 过滤 `見切り`，记下字符串 VA
     （旧版 `0x1430BA8F0`；或 `python vadump.py 0x1430BAxxx 0x200` 按内容找）。
  2. 对该字符串 Show References to → 命中一个**巨型名字→索引解析器**
     （旧版 FUN_140B2E4E0；特征：几百次字符串比较，命中即 `return 小整数`，
     `見切り回避成功後の攻撃ヒット` 返回 `0x277`）。
  3. 对解析器 Show References to → 调用者里找**条件树递归评估器**
     （特征：比较节点 vtable 类型、递归子节点、`case 0x10`/`0x11` 处理 AND/OR），
     即 FUN_14118BDA0。
  4. RVA = 函数 VA − 0x140000000。
- **验证**：`InstallCondEvalHook` 的 detour 能收到 `(weaponType, outArr, node)`
  三参调用，且 `COND631` 日志在见切相关动作时出现。
- **写回**：同 2.1（`condEval` 项）。

### 2.4 poolQuery（无伤/防御查询，旧版 RVA 0xAEE050）

- **锚点**：无分支小函数，`(pool, reactionType) -> float`，内部一个大
  `cmp type,5 / ja` 开头的 switch。
- **步骤**：在 dmgCalc（2.5）反编译里找对它的 `call`；或搜立即数 `0xAEE050`
  无效时，用旧版函数体前 48 字节在 Ghidra 里 Search → For Bytes（前几个非
  通配字节）模糊定位。
- **验证**：`FSDQ` 日志在见切动作期间出现。
- **写回**：`poolQuery` 项；该钩子仅诊断，失败可暂时不修。

### 2.5 dmgCalc / hitDispatch（伤害计算 / 受击分发，诊断）

- **锚点**：二者互为上下游——hitDispatch 内部大 switch（返回码 0xD = 环境检测），
  dmgCalc 是它下游最大的浮点密集函数。
- **步骤**：从 hitDispatch 的返回码 switch 认出它（旧版 0x1F71960）；
  在其引用的函数表/调用链里找 dmgCalc（旧版 0x1F66DD0）。
- **写回**：`dmgCalc` / `hitDispatch` 项。仅诊断，可最后修。

### 2.6 音频三件套（postEventId / soundPost / postEventName，纯诊断）

- **锚点**：AK 包装函数互相调用；soundPost（旧版 0x26D4C50）开头
  `48 83 EC 48 8B C2 45 8B D1`。
- **步骤**：字符串搜 `PostEvent`/Wwise 相关引用；或从任一音频调用点向上追。
- **写回**：可放弃（不再用于触发，只影响 WWISEEV 日志）。

### 2.7 特征码生成与验证（写回的标准流程）

```powershell
cd Reverse
# 1) 把本章找到的新 RVA 填进 sig_check.py 的 TARGETS
python sig_check.py --emit      # 生成 signatures.json 并打印命中
python sig_check.py --verify    # 必须全部 OK 且唯一命中（命中数>1 = 特征码太弱，加长）
# 2) 把输出的 pattern/mask 抄进 dllmain.cpp 的 kHookSigs[]，重编译部署
```

临时方案（不重编译）：`nativePC/plugins/ValorGauge.addresses.txt` 写
`名字=0xRVA`，名字见附录 A。优先级：override > 特征码 > 旧 RVA。

### 2.8 gaugeAdd（气刃量累加器 FUN_142122410，气刃系统的总闸门）

- **是什么**：weapon+0x2368（气刃量 float）唯一的 float 写入方。每招增量/消耗、
  居合回填、见切清空全部流经它 → 钩它 = 记录/改写整个气刃系统（GAUGEEVT/GAUGERULE）。
- **锚点**：函数头 `40 53 48 83 EC 40 0F 29 74 24 30`（push rbx; sub rsp,0x40;
  movaps [rsp+30h],xmm6），随后 `48 8B D9`（mov rbx,rcx）；
  deref 链 `mov rax,[rcx+0x9B0]` → `mov rcx,[rax+0x7D20]` → call；
  写回 = `movss [rbx+0x2368],xmm6` + `minss` 钳 1.0（常量旧版 0x142E4FE58=1.0）。
- **孪生陷阱**：FUN_14211EF90 是 +0x236C 版同款累加器（10 个调用点），前 44 字节完全
  相同！区分点：① call 转移字节（旧版 `E8 DF 0F 0A` vs 孪生 `E8 5F 44`）——注意这个
  rel32 在游戏更新后**会变**，特征码因此失效属正常，走 RVA/override 兜底；
  ② 里面的 addss 偏移 `68 23`（+0x2368）vs `6C 23`（+0x236C）——手工找新版时用它验。
- **步骤**：
  1. 找每招事件处理器（形如旧版 FUN_1417FDAE0：`param表=func_0x14211C060(weapon)` 后
     `FUN_142122F90(weapon, *(float*)(param表+0x8xx))`，0x8xx ∈ 0x808..0x834）。
  2. 进 FUN_142122F90（wrapper），它内部 call 的就是累加器。
  3. 反汇编确认写的是 +0x2368（不是孪生的 +0x236C）。
- **验证**：反编译形状 = 参数>0 时乘 `func(mgr)` 再加到字段、钳位 [0,1.0]、
  写 `+0x236C = *(paramTable+0x804)`；训练场砍一刀 GAUGEEVT 日志出对应 val。
- **写回**：sig_check.py TARGETS 加 `"gaugeAdd": 0x新RVA` → --emit → --verify →
  抄进 kHookSigs[] `gaugeAdd`；急用 `addresses.txt` 写 `gaugeAdd=0x新RVA`。

---

## 3. 数据地址

### 3.1 propRegistry（FSM 属性注册表，旧版 RVA 0x5012048）

- **锚点**：propInt（2.1 已修好）的函数体**内部直接引用**它——旧版引用指令在
  `propInt+0x2D`（`find_xrefs.py 0x145012048` 的 24 个命中之一）。
- **步骤**：
  1. 在 Ghidra 打开 propInt，找函数体里第一条 `mov/lea reg, [rip+…]` 指向
     `.data` 段的全局 → 记下 VA → RVA = VA − 0x140000000。
  2. 或 `python find_xrefs.py 0x145<新版RVA>` 全量列出引用位置。
- **验证**：`python vadump.py <新RVA+0xB8> 4` 读条数，应在 200 万以内且
  >200；`+0xC8` 处是指针（行表）。DLL 的 MEMORYMAP 报告会自动做这两项检查。
- **写回**：`addresses.txt` 写 `propRegistry=0x新RVA`（或改 `InitMemoryMap`
  里的默认值后重编译）。
- **连带**：`propLookup`（旧行查找函数 0x462700，结构 `cmp edx,[rcx+0xB8] /
  mov rax,[rax+0xC8] / mov rax,[rax+rcx*8]`）——按此结构在新版搜字节
  `3B 91 B8 00 00 00` 即可；`propName` 同理在 propInt/注册表相关代码附近。

### 3.2 audioSingleton（音频管理器单例槽，旧版 RVA 0x51C458）

- 无 rip 引用（可能经数据表间接引用）。锚点改用 soundPost（2.6）反编译里
  加载单例的指令。纯诊断，可放弃。

---

## 4. 结构体字段（SANITY 告警对应章节）

结构体字段**没有**独立特征码——靠语义锚静态推导，或 F5 挖掘器动态对比。

### 4.1 静态路

1. 在反编译里搜字段的**消费代码**（附录 C 的语义锚列了每个字段"谁在读它"）。
   例如 mgr+0x660/661/662 的读者是输入改写器：找它的方法 = 搜 `cmp [rax+0x660]`
   同簇指令，或从 2.3 的条件评估器家族顺藤摸瓜。
2. `scan_277.py` 可扫任意立即数：把脚本里的 `0x277` 改成目标常量（如
   `0x2370`）即可列出所有 `cmp r32, 立即数` 位置。
3. 事件位数组 player+0x8938：锚点是置位函数（旧版 func_0x141F604B0，
   形如 `bts [rcx+0x8938+(bit>>5)*4], edx`）——搜立即数 `0x8938`。

### 4.2 动态路：F5 挖掘器（附录 D）

### 4.3 字段语义锚速查

| 字段 | 语义锚（怎么验证找对了） | 静态入口 |
|---|---|---|
| player = controller−0x61C8 | DoAction 第一参就是控制器 | 2.2 的反编译 |
| weapon = *(player+0x76B0) | 指向对象的 +0x2370 ∈ [0,3] | 改写器 FUN_14118C320 读它 |
| weapon+0x2370 气刃等级 | 只在 0/1/2/3 之间跳变 | 搜 `0x2370` 立即数 |
| weapon+0x23A0 蓝光强度 | 叮瞬间跳 ~1.0 后衰减；空挥恒 0 | F5 挖掘（附录 D） |
| weapon+0x2368 **气刃量** float 0..1 | 每帧写 1.0 → HUD 气刃条钉满（2026-09-24 实测）；UI getter 读它 ×100 | 2.8 累加器写它；见 4.4 |
| weapon+0x236C 第二累加器 | 孪生累加器写它；校验器拿它与 +0x2368 做 uint 比较 | FUN_14211EF90（2.8 的孪生） |
| weapon+0x2374 等级持续时间 | 每帧 −0.000556（30s 周期）；降级时从参数回填 1.0 | 降级函数（refill from param+0x9AC/0x9B4） |
| weapon+0x2378 兜割回气秒 | >0 期间气刃每帧 +dt/参数(0x840/0x844) | 每帧 updater（附录 C） |
| weapon+0x2388 居合回气秒 | 同上（参数 0x850/0x854）；HUD 检测 >0 切回气显示 | FUN_142123510（0x65 动作写 param+0x84C） |
| weapon+0x237C / +0x2384 回气档位 | byte：0=白黄/普通，非 0=红刃/掐点 | 每帧 updater 的 rate 参数选择分支 |
| player+0x7810 → 参数表指针 | **双重解引用**：paramTable = *(player+0x7810) | 累加器反编译的 `mov rax,[rcx+0x9B0]` 后续 deref |
| mgr = *(player+0x7D20) | +0x7C0 ∈ {0,1} | DoAction 附近代码 |
| mgr+0x28→pool+0xE4 无伤秒 | float ≥ 0；注入 0.5 即变 0.5 | F5 挖掘 |
| mgr+0x660/661/662 锁存 | 见切/居合后输入改写时置位 | 2.3/改写器反编译 |
| player+0x8938 事件位数组 | dword0 bit15=见切成功；+0x8954 bit26=居合否定 | 搜 `0x8938` 立即数 |
| player+0x7626 配置门 | 常年 0/1 | 2.3 函数开头的分支 |
| player+0x126D8 属性组件 | +0x84+id*24 键槽（stride 24） | propInt 反编译 |

### 4.4 太刀气刃量字段块（weapon+0x2368..0x239C，2026-09-24 实测+反编译）

动态验证法（GAUGELOCK，最快）：DLL 每帧写目标字段 1.0，看 HUD 哪根条被钉住/哪个
状态被激活——气刃量、回气状态都是这么实测确认的。静态验证法见 2.8。

| 偏移 | 语义 | 备注 |
|---|---|---|
| +0x2368 | float 气刃量 0..1 | HUD 条 ×100；进居合架势被 int 2 覆盖 = 清零（复用同 dword） |
| +0x236C | float 第二累加器/消耗槽 | 语义未全解；校验器 FUN_140B980C0 拿它与 +0x2368 比较 |
| +0x2370 | int 气刃等级 | 0 无/1 白/2 黄/3 红 |
| +0x2374 | float 等级持续时间 | 30s 线性衰减；清零/降级触发降级函数 |
| +0x2378 | float 兜割回气剩余秒 | vanilla 兜割后 10s/红刃 15s |
| +0x237C | byte 兜割回气档 | 0=白黄 rate+0x840，非 0=红 rate+0x844 |
| +0x2380 | int 等级快照 | 0x59..0x5C 动作期间 = +0x2370 |
| +0x2384 | byte 居合掐点档 | 0=普通 rate+0x850，非 0=掐点 rate+0x854 |
| +0x2388 | float 居合回气剩余秒 | 普通居合 15s / 掐点 30s（param+0x848/0x84C） |
| +0x2398..0x239C | byte 居合事件标志 | FUN_142123510 置位 |

---

## 5. FSM 条件/属性体系（深挖时用）

- 属性名表：`.rdata` 里成片 UTF-8 日文串（旧版 0x1430B9xxx-0x1430BAxxx）。
  名字→索引解析器 = 引用这些串的巨型比较函数；`見切り回避成功後の攻撃ヒット`
  = 0x277，邻居：練気ゲージ判定(見切り=0x274、(必殺=0x275、居合斬り=0x276。
- 属性注册记录（旧版 FUN_140B2A030）：`{"cFSMPlBase::transitionDummy",
  名字串, 0x83003, 参数, 访问器(xor al,al; ret), …}` 按注册顺序编索引。
  访问器是恒 false 桩 = 该条件不是存储布尔，值在运行时逻辑里（如输入改写器）。
- 每武器条件描述符表：函数 `return wtype*0x130 + 表基址`（旧版基址
  0x143FD5590）→ `python fsm_table.py`。

---

## 6. 输入→动作路由链（收刀拔刀刚1 的游戏侧机制，按需重建）

> 收刀改写本身不钩这里任何函数（mod 在 doAction 钩子里改 ActionInfo，纯内存读门控）。
> 本章是"更新后想再动请求层/数据侧改写收刀行为"时的重建指南。核心事实：
> **收刀按 R2 → {3,11} 由代码+静态表路由，武器 FSM 完全不参与**（待機1000 零链路）。

- **请求层链条**（旧版地址，更新后按【锚点】重找）：
  1. 初始化：FSM 管理器初始化器（旧版 FUN_14118DCA0 / IB 变体 FUN_14118DDC0）
     注册两层：set-1 重映射回调（FUN_14118C320，IB 态 player+0x7626!=0 时换
     FUN_14118D3A0）+ set-3 每武器请求表
     `FUN_14026AE00(obj, 3, *(row+0x48), *(u32)(row+0x50))`（obj = mgrCtx+0xA00）。
  2. 每武器行表：`FUN_142165600(type)` = `type*0x130 + 0x143FD5590`
     ——**与第 5 章条件描述符同一张表基址**。LS（type=3）行 = 0x143FD5920：
     +0x48 = 请求表 0x143F4D6A0、+0x50 = 条目数 113。
     （静态条目疑似 0x28 stride {key, 子表指针, 数量, pad}，但拷贝循环按 0x20——
     stride 矛盾未决，重验时先指令级核对拷贝步长。）
  3. 消费链：FUN_14026B670（请求 {set,key} → 数组 obj+(set+8)*0x10 取条目，逐候选）
     → FUN_14026B830（候选 0x18B 6×i32 解析，特判 -0x14(-20)/-10/-1 标记；
     **-10 = 居合斬り開始标记**）→ FUN_14026B590（最终 {set,id} 经
     func_0x140269CE0 = DoAction 发出）。
- **重映射器分发怪癖（改 enum 时必读）**：weapon 类型 switch 前有
  `mov eax,[weapon+0x9F8]; dec eax; cmp eax,0xA; ja 透传; jmp [跳转表+eax*4]`
  （旧版跳转表 0x14118D098，11 项）——**反编译 case N = 武器类型 N+1**。
  反编译 case 2 就是 LS 的居合架势重映射（0x2368!=0 时 0→100,1→101,2→102,
  3→103,4→104,0x12-0x22→0x69..0x7d/0x43,0x35/36→0xa0/0xa1,0x8d→分档；
  **id 11 不在任何重映射表 → 透传**）。武器类型枚举此表按 1-based。
- **收刀判据（mod 实现）**：weapon+0xC71 不可信（2026-09-25 实测收刀态读 0，
  极性/语义从未局内成立）。实际用的是纯 mod 侧判据：**tracker 里上一个动作是
  set 1**（1:0 待机 / 1:4 走 / 1:8 跑 / 1:5 预备 = 收刀 locomotion 族；
  持刀态 locomotion 不发 DoAction 请求，set-3 前趋 = 持刀）。
- 【锚点】新版 dump.c 里搜数组公式 `(x + 8) * 0x10`：命中 FUN_14026AE00 的
  拷贝循环和 FUN_14026B670 的消费者；再找调用 FUN_14026AE00 且参数含
  `*0x130 + 常量` 行表函数的初始化器。
- 【验证】LS 行 +0x50 应为合理条目数（旧版 113），+0x48 指向 .rdata/.data 成片
  结构；局内收刀按 R2，DoAction 日志应出现 {3,11}（改写关闭时）。

---

## 7. 动作帧控制与 motion 时钟（FRAMESEEK，2026-09-26 新增）

> **先读这个**：FRAMESEEK 功能**不新增任何钩子地址**——双写全部发生在既有
> doAction / propInt 钩子的运行时指针上，靠**结构偏移**读写。会过期的不是
> VA，是偏移。所以本章重点是偏移的再推导步骤；2.2 的 doAction、2.1 的
> propInt 修好后，本章没有任何 addresses.txt 要改。
> 结论先行：**动作有两条时间线** —— 逻辑帧 `instance+8`（秒，30Hz 逻辑步进）
> 和 motion 时钟 `*(float*)(*(player+0x468)+0x10C)`（TIML 帧数，+2.0/逻辑tick，
> 60/s）。**骨架动画只采样 motion 时钟**；只写 instance+8 画面纹丝不动
> （2026-09-26 实测定案）。

### 7.1 ActionController 与动作实例布局

| 位置（player 基址） | 语义（旧版偏移） |
|---|---|
| player+0x61C8 | ActionController 本体（slot0；slot1 = +0x6878，步长 0x6B0） |
| +0xA8 | phase：0 idle / 1 请求已接受 / 2 运行中（缓冲路径不改 phase，勿用它判断 begin 已跑） |
| +0xAC / +0xB0 | 当前 set / id（= player+0x6274 / 0x6278） |
| +0xB4/+0xB8, +0xBC/+0xC0 | pending；缓冲请求（DoAction 在 phase!=0 时写后者） |
| +0xC4/+0xC8, +0xD0, +0xD8 | 前一动作 set/id、前一实例指针、前一动作帧（+0xD8 只写不读） |
| +0x68+set*0x10 | instArray 指针；`instance = *(instArray + id*8)`；条目数在 +(set+7)*0x10 |
| +0x6A8 (=player+0x6870) | 动作自动结束阈值 float（<0 关；实测 3:81/3:104 期间恒 -1） |
| instance+8 / +0xC | **动作帧 float（秒）**；本帧 dt。begin（FUN_14026A1E0）清零 +8，每帧更新器 `frame += dt` |
| instance+0x18 | finished 字节；instance+0x28 = player。+0x10 在武器动作上实测不可读（相机类才是 core 指针），勿依赖 |

- **【锚点】getter 三连**（找控制器布局最快的入口）：反汇编搜
  `imul rdx, rax, 0x6B0`（字节 `48 6B D2 B0 06 00 00`）。命中三个相邻函数：
  `lea rax,[rcx+0x6274]`（{set,id} 指针）/ 读 +0x6278（id）/ 读 +0x6274（set），
  另有变体先 `add rcx,0x61C8` 再转调实例解析器
  （`movsxd rax,[rcx+0xAC]; cmp eax,-1; …; mov rax,[rcx+rax*8+0x68];
  mov rax,[rax+rdx*8]` = FUN_14026A420 形状）。
- **【锚点】DoAction 本体**（2.2 找到后）顺带实锤整套布局：范围校验
  `(set+7)*0x10`、实例存在性 `[rax+r8*8]`、phase 分支 `[r9+0xA8]`、
  旧帧快照 `movss xmm0,[r8+8]` → `[r9+0xD8]`。
- **【锚点】每帧更新器**（验证 `frame += dt` 与 begin 清零）：从 DoAction 的
  phase 字段交叉引用找更新循环（旧版 FUN_141F78600 内联了 FUN_14026A820 /
  FUN_14026A1E0(begin, 清零+8) / FUN_14026A290(end) / FUN_14026A660(收尾旧动作)）。
- **【验证】** 局内 F6 强制任意动作，log `FRAMETICK act=.. frame=.. dt=..`：
  frame 从 dt 起按 dt 递增、每次动作（重）开始归零。旧版 dt = 0.0333334 = 1/30
  （**单位是秒，30Hz 逻辑帧**；TIML 的 60fps 帧 n = n/60 秒）。
- **【写回】** `dllmain.cpp` 的 `ResolveActionInstance` / `TryFrameSeek` /
  `TryQueuedForceWrite` 里的字面偏移（0x61C8/0xA8/0xAC/0xB0/0x68/0x10C/8/0xC）。

### 7.2 motion 时钟（骨架真正采样的时间线）

- 值：`mot = *(float*)(*(player+0x468)+0x10C)`。计 TIML 帧数（+2.0/逻辑tick、
  60/s），motion 每次（重）启动归零；动作中途换 motion 也会归零重计。
  原生 getter 旧版 0x141BFF780：`mov rax,[rcx+0x468]; …; movss xmm0,[rax+0x10C]`
  （游戏用它判断"步进动作未完成"）。**帧 0x10C 旁还有 4 个同步 +2 时钟**：
  +0x110 / +0x124 / +0x128 / +0x22C（旧版 CLK 报告；疑似动画层/混合同步时钟，
  只写 +0x10C 已视觉生效，其余语义未挖）。
- **【锚点】静态**：搜"以 [rcx+0x468] 取基址、返回 [rax+0x10C] float"的小 getter
  （函数极短，int3 包夹），或从 0x6B0 getter 家族同地址邻域找。
- **【锚点】动态（更新后的万能法，推荐）**：CLOCKMINER 自带自愈——F6 强制一次
  动作，看 log `CLK anim468 base=0x.. :+0x.. step=2 (hits)`：**恒定步长 +2 的
  偏移就是新版 motion 时钟**（结构挪了它自己会报新偏移）。`FRAMETICK` 行的
  `mot=` 字段即读此偏移。
- **【验证】** seek 后 `FRAMESEEK settled ... mot=..` 的 mot 应 > 目标（秒×60），
  且画面从中段起手。
- **【写回】** `dllmain.cpp` 两处：FRAMETICK 的 motTime 读取、FRAMESEEK v3 的
  mot 写入（含 ×60 秒→TIML 帧换算）。若骨架再不跟随：扩 `g_clkRegions[]`
  挖矿区域（weapon 全体 / player+0x470 / anim 深层）重新报告。

### 7.3 每武器动作类表（深挖入口，无运行时依赖）

- LS set-3 类表旧版 0x142EF5C30（113 项，表项 0x20 stride {id u32, set u32,
  classPtr, pad}，id 与类一一对应）；行 = `FUN_142165600(type)` =
  `type*0x130 + 0x143FD5590` 的 +0x40(表)/+0x50(数)——**与第 6 章请求表同一行**
  （共享 count 字段）；+0x70/+0x78 = slot1 类表（8 项）。
- 类对象在 .data（旧版 0x144FExxxx 区）；**类名 ASCII 就嵌在 .rdata vtable 后面**
  （`nHmAction::Wp03:...` 家族；vtable 4 个 code ptr 后跟名字）。
  set-1 locomotion 类表注册点 = FUN_141BFCF80 的 14 处调用（每武器一张，
  旧版 0x142E6C640 等）；换武器注册器 = FUN_141F5A410（读 weapon+0x9F8）。
  动作 id→LMT entry **不存在平表**、类对象里也无 LMT 字段（已扫描排除）——
  LMT 选择在类工厂/核心对象代码里（工厂族 0xCD0 字节运行时对象，旧版
  FUN_1417FADA0 一家）。
- **【锚点】** 新版 dump 里字符串搜索 `nHmAction::` → 命中处往回找 vtable 头
  （4 个 code ptr）→ 找引用该 vtable 的 .data 指针 = 类对象 → 找按 0x20 stride
  引用类对象的表 = 类表。
- **【验证】** LS 类表第 86 项（见切）的类名应含 SlashAim（旧版
  cActWpOnSlinger 名域相邻，注意对准 vtable 头）。

### 7.4 动态恢复总路线（mod 自带仪器，优先走这条）

1. 更新后 F6 强制一次动作 → 看 `FRAMETICK`：dt 值（旧版 1/30）、frame 是否
   递增/归零 ⇒ instance+8 布局是否还成立。
2. 看 `CLK` 报告 ⇒ 新版 motion 时钟偏移（step=+2 那批）。
3. 改 force-action.txt 加第 4 参（如 `3 81 0 0.5`）→ F6 → 画面从中段起手 +
   `FRAMESEEK settled` ⇒ 双写通路活着。
4. 画面又不跳（时间线再断）⇒ `g_clkRegions[]` 加区域重挖。
5. 经验教训（重踩过）：**属性钩子 ~10 次/帧 ≠ 逻辑帧**——挖矿/计数必须按
   "frame 值变化"门控；**±dt 检测会漏掉 TIML 帧制（+2.0/tick）的时钟**——
   用恒定步长法；**写帧要在 begin 之后**（首 tick 的值是上一轮残值，
   begin 清零会洗掉早写的值——现在的"钉住到追上"逻辑对 begin 时序自愈）。

---

## 8. 最终验证标准

1. `MEMORYMAP hooks:` 一行 12 项全 `ok`（或有意放弃的诊断项）。
2. `MEMORYMAP propRegistry count=… rowTable=ok`、`audioSingleton ok`。
3. 进训练场：见切空挥（静默）→ 叮（屏幕出反击提示、出刚1）→ 居合 101/102 同测。
4. 收刀拔刀刚1：红刃+气刃>0.15 收刀 R2 → 86 出招+无敌
   （log `SHEATHREWRITE ... nohit=0.2s`）；非红刃/气刃不足 → 原版拔刀气刃1
   （log `SHEATHREWRITE reject`）。
5. 无 `MEMORYMAP SANITY` 告警。
6. FRAMESEEK：force-action.txt 带第 4 参（如 `3 81 0 0.5`）→ F6 → 画面从中段
   起手，log `FRAMESEEK settled ... mot=..`；`FRAMETICK` 的 frame/dt/mot 数值
   形状正常（dt=1/30 递增、mot=+2/tick）。
7. `python sig_check.py --verify` 全 OK（供下次更新用）。

---

## 附录 A：addresses.txt 键名 → 旧版 RVA

`doAction`（自动）、`condEval` 0x118BDA0、`poolQuery` 0xAEE050、
`dmgCalc` 0x1F66DD0、`hitDispatch` 0x1F71960、`postEventId` 0x28B8DD0、
`soundPost` 0x26D4C50、`postEventName` 0x28B8FE0、`propInt` 0x1922190、
`propBool` 0x19221F0、`propFloat` 0x1922240、`gaugeAdd` 0x2122410、`fxNotify` 0xADD670、
`audioSingleton` 0x51C458、`propRegistry` 0x5012048、`propLookup` 0x462700、
`propName` 0x1979770。

## 附录 B：结构体字段旧版偏移

见 4.3 / 4.4 表；动作帧控制系（控制器/实例/motion 时钟）见第 7 章 7.1/7.2 表。player 系偏移从 controller−0x61C8 起；weapon = *(player+0x76B0)；
mgr = *(player+0x7D20)；pool = mgr+0x28。

**运行时 w03p 参数表**（paramTable = *(player+0x7810)，双重解引用）：

| paramTable+ | 语义（w03p 文件偏移 = 运行时 − 0x367） |
|---|---|
| +0x800 | 气刃衰减分母（Unk 120，file 1177；+0x236C 每帧 −dt/此值） |
| +0x804 | 当前消耗槽（累加器每次调用写进 weapon+0x236C；file 1181 = Spirit Gauge Decay） |
| +0x808..+0x834 | 12 个每招气刃增量/消耗槽（file 1185..1225；每招读哪个槽硬编码在事件处理器里） |
| +0x838/+0x83C | 兜割回气时长 10s / 红刃 15s（file 1233/1237） |
| +0x840/+0x844 | 兜割回气速率 白黄/红（file 1241/1245，下值快） |
| +0x848/+0x84C | 居合回气时长 15s / 掐点 30s（file 1249/1253） |
| +0x850/+0x854 | 居合回气速率 普通/掐点（file 1257/1261，下值快） |

**已知坏字段**：weapon+0xC71 —— 旧版反编译推断"0=拔刀/非0=纳刀（FUN_142150D10
锁存）"，但 2026-09-25 局内实测收刀态读 0，不可用作收刀判据。收刀判定用
tracker set-1 前趋（见第 6 章）。若更新后想再验它：SHEATHPROBE 日志行会打印
c71 实测值，收刀/拔刀各采几组对比即可定案。

## 附录 C：本项目沉淀的关键机制（找新版对应物时的"目的地"描述）

- 输入改写器（旧版 FUN_14118C320）：`weapon+0x2368` 时把输入 0-4 改写为
  100-104；`mgr+0x660/661/662` 锁存时把输入改写为 761-770；
  **player+0x8938 bit15 决定成功(763/162)与失败(764/163)分支**。
  注意改写器里架势判断是 `cmp byte ptr [rdi+0x2368], 0`（按字节）。
  **分发怪癖：类型 switch 前有 `dec eax`，反编译 case N = 武器类型 N+1**
  （case 2 就是 LS；详见第 6 章）。
- **收刀态输入路由（2026-09-25 定案）**：收刀按 R2 → {3,11} 由代码+每武器静态
  请求表路由（113 条 @ 旧版 0x143F4D6A0，消费链 b670→b830→b590→DoAction），
  武器 FSM 零参与。收刀判定 = tracker set-1 前趋；weapon+0xC71 不可信。
  mod 的收刀拔刀刚1（sheath-rewrite）建立在 doAction 钩子 + 该判据上，
  不新增任何钩子地址。
- **帧 0 无伤与弹反互斥（2026-09-25 事故定案）**：pool+0xE4 no-hit 是命中查询层
  的上游剔除（="游戏层面你没被攻击过"）；帧 0 注入会让攻击到不了防御仲裁 →
  无弹反交互 → 无叮 → bit15 永不置位 → 见切/居合反击链全灭。
  叮类招式的保护必须后置于叮（bit15 先行、注入殿后）；前置保护只适用于
  无弹反呈现的自定义招（如收刀刚1 的 0.2s 窗口）。
- **双时间线（2026-09-26 定案）**：动作逻辑帧 = instance+8（秒，30Hz），
  motion 时钟 = *(float*)(*(player+0x468)+0x10C)（TIML 帧，+2/tick，motion 重启归零）；
  **骨架只采样后者**。FRAMESEEK = force 后检测新 begin（帧回落 ≤2dt）→ 双写
  （逻辑帧=秒、mot=秒×60）→ 钉住直到时间线追上。详细恢复步骤见第 7 章。
- 见切成功判定 = 事件位 15；居合否定 = 事件位 250（player+0x8954 bit26）。
- 强制出招 = 在 propInt 钩子里直接调用原始 DoAction（栈上 ActionInfo，勿保存指针）。
- **气刃系统（2026-09-24 全链路破解，详见 2.8/4.4）**：
  - 所有气刃增减经唯一累加器（2.8）写 weapon+0x2368（float 0..1，HUD ×100 显示）。
  - 每招的增量/消耗 = 事件处理器硬编码参数表槽位（+0x808..0x834），数值在 w03p。
  - 居合架势 = FUN_14211CC60 往 +0x2368 写 int 0/2（覆盖 float = 清零气刃），
    每帧 updater 按 int ==1/==2 衰减——float 量与架势计数复用同一 dword。
  - 兜割/居合后的自动回气 = weapon+0x2378/+0x2388 计时器，每帧 updater 递减并
    给气刃量加 dt÷rate参数；HUD 检测计时器 >0 切回气显示。
  - 动态实测手法：每帧写候选字段 1.0 看 HUD/状态反应（GAUGELOCK）；钩累加器看
    每招增量（GAUGEEVT）。

## 附录 D：F5 动态挖掘向导（静态路走不通时的万能后备）

1. 编译部署带挖掘器的 DLL（本仓库版本自带）。
2. 训练场，对一个怪/木桩：
   - 空挥目标动作 ×3（如见切挥空）→ 按 **F5**（打相位标记）
   - 目标场景 ×3（如叮到）→ 再按 **F5**
3. 读 `nativePC/plugins/ValorGauge.log` 里 `MINER ANALYSIS` 报告：
   相关度 = 叮组命中率 − 空挥组命中率，排最前的候选即为信号。
4. 拿到候选偏移后：加进 `PollReactionWatch` 的探针列表，先用
   `EPROBE` 日志复核 4-5 组，再接 `OpenCounterWindow` 触发链。
   （居合 bit250、蓝光 weapon+0x23A0 都是这样挖到的。）
