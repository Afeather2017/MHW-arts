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
| weapon+0x2368 特殊纳刀 | 姿态中为 1 | 改写器的居合重映射分支 |
| mgr = *(player+0x7D20) | +0x7C0 ∈ {0,1} | DoAction 附近代码 |
| mgr+0x28→pool+0xE4 无伤秒 | float ≥ 0；注入 0.5 即变 0.5 | F5 挖掘 |
| mgr+0x660/661/662 锁存 | 见切/居合后输入改写时置位 | 2.3/改写器反编译 |
| player+0x8938 事件位数组 | dword0 bit15=见切成功；+0x8954 bit26=居合否定 | 搜 `0x8938` 立即数 |
| player+0x7626 配置门 | 常年 0/1 | 2.3 函数开头的分支 |
| player+0x126D8 属性组件 | +0x84+id*24 键槽（stride 24） | propInt 反编译 |

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

## 6. 最终验证标准

1. `MEMORYMAP hooks:` 一行 10 项全 `ok`（或有意放弃的诊断项）。
2. `MEMORYMAP propRegistry count=… rowTable=ok`、`audioSingleton ok`。
3. 进训练场：见切空挥（静默）→ 叮（屏幕出反击提示、出刚1）→ 居合 101/102 同测。
4. 无 `MEMORYMAP SANITY` 告警。
5. `python sig_check.py --verify` 全 OK（供下次更新用）。

---

## 附录 A：addresses.txt 键名 → 旧版 RVA

`doAction`（自动）、`condEval` 0x118BDA0、`poolQuery` 0xAEE050、
`dmgCalc` 0x1F66DD0、`hitDispatch` 0x1F71960、`postEventId` 0x28B8DD0、
`soundPost` 0x26D4C50、`postEventName` 0x28B8FE0、`propInt` 0x1922190、
`propBool` 0x19221F0、`propFloat` 0x1922240、`audioSingleton` 0x51C458、
`propRegistry` 0x5012048、`propLookup` 0x462700、`propName` 0x1979770。

## 附录 B：结构体字段旧版偏移

见 4.3 表。player 系偏移从 controller−0x61C8 起；weapon = *(player+0x76B0)；
mgr = *(player+0x7D20)；pool = mgr+0x28。

## 附录 C：本项目沉淀的关键机制（找新版对应物时的"目的地"描述）

- 输入改写器（旧版 FUN_14118C320）：`weapon+0x2368` 时把输入 0-4 改写为
  100-104；`mgr+0x660/661/662` 锁存时把输入改写为 761-770；
  **player+0x8938 bit15 决定成功(763/162)与失败(764/163)分支**。
- 见切成功判定 = 事件位 15；居合否定 = 事件位 250（player+0x8954 bit26）。
- 强制出招 = 在 propInt 钩子里直接调用原始 DoAction（栈上 ActionInfo，勿保存指针）。

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
