# 游戏更新后的地址恢复手册

游戏打补丁后，代码会整体移动（所有 RVA 偏移），结构体字段也可能增删。
本插件的防御分三层，断了哪层就按对应流程修：

1. **特征码扫描（自动）** — 10 个函数钩子在运行时按字节签名定位，补丁移动代码不影响。
2. **`nativePC/plugins/ValorGauge.addresses.txt`（手动兜底）** — 不用重编译就能覆盖任何地址。
3. **MEMORYMAP 自检（诊断）** — 启动时报告每个地址的解析结果和健康度；运行中
   语义探测一旦发现字段不像它自己，立即在日志里指认偏移。

---

## 更新后的标准流程

1. 正常进游戏，打开 `nativePC/plugins/ValorGauge.log`，找 `MEMORYMAP` 行。
2. **全部 `ok`** → 函数没变，直接玩。再看有没有 `MEMORYMAP SANITY` 行
   （没有 = 结构体字段也没变，彻底完事）。
3. **某 hook 显示 `signature miss - falling back to RVA`** → 函数被移动了：
   - 临时：在 `ValorGauge.addresses.txt` 写一行 `名字=0x新RVA`（见下方名字表）。
   - 根治：用新 exe 重新提取特征码（见"特征码维护"）。
4. **hook 全 ok 但行为不对 / 出现 SANITY 告警** → 结构体字段偏移变了：
   按"结构体字段表"用语义锚重新推导该字段。

---

## 函数钩子表（自动特征码解析）

| 名字 | 用途 | 旧RVA | 备注 |
|---|---|---|---|
| doAction | ActionController::DoAction（动作执行/强制中断） | 0x3100F0 族 | 扫内部片段，命中-10 即函数头 |
| condEval | FSM 条件树节点评估 FUN_14118BDA0 | 0x118BDA0 | |
| poolQuery | 无伤/防御查询 FUN_140AEE050 | 0xAEE050 | |
| dmgCalc | 伤害计算入口 FUN_141F66DD0 | 0x1F66DD0 | 诊断 |
| hitDispatch | 受击分发 FUN_141F71960 | 0x1F71960 | 诊断 |
| postEventId | AK PostEvent(id) | 0x28B8DD0 | 诊断 |
| soundPost | 音频包装 FUN_1426D4C50 | 0x26D4C50 | 诊断 |
| postEventName | AK PostEvent(字符串) | 0x28B8FE0 | 诊断 |
| propInt | FSM 属性 int getter FUN_141922190 | 0x1922190 | 强制路径依赖 |
| propBool | FSM 属性 bool getter FUN_1419221F0 | 0x19221F0 | |
| propFloat | FSM 属性 float getter FUN_141922240 | 0x1922240 | |

## 数据地址表（RVA + overrides 兜底）

| 名字（addresses.txt 键） | 旧RVA | 含义 | 自检 |
|---|---|---|---|
| audioSingleton | 0x51C458 | 音频管理器单例指针槽 | 槽可读、值非空 |
| propRegistry | 0x5012048 | FSM 属性注册表（+0xB8 条数，+0xC8 行表） | 条数 200~200 万且行表可读 |
| propLookup | 0x462700 | 注册表行查找 FUN_140462700 | 随注册表间接验证 |
| propName | 0x1979770 | 属性 id→名字 | 同上 |

`ValorGauge.addresses.txt` 示例（`#` 注释，RVA 用 0x 或十进制都行）：

```
# after title update 5
condEval=0x118C5A0
propRegistry=0x5013400
```

---

## 特征码维护（函数被移动时）

特征码由 `Reverse/sig_check.py` 生成与验证（通配符自动覆盖 rel32 转移和
rip 相对位移——正是补丁会改动的字节）：

```powershell
cd Reverse
# 1) 拿到新版本里函数的新 RVA 后，改 sig_check.py 里的 TARGETS
python sig_check.py --emit     # 生成 signatures.json
python sig_check.py --verify   # 必须全部 OK 且唯一命中
# 2) 把新特征码抄进 ValorGauge/src/dllmain.cpp 的 kHookSigs[]，重编译
```

**怎么找新 RVA**：新版 exe → Ghidra 反编译（与 MonsterHunterWorld_dump.exe.c
同流程）→ 用下面任一锚点定位函数 → 函数地址 - 0x140000000 = RVA：

- `condEval`：搜字符串 `見切り回避成功後の攻撃ヒット`（.rdata 0x1430BA8F0 一带
  的名字表），其 xref 链到名字→索引解析器 FUN_140B2E4E0，再被 FUN_14118BDA0 调用。
- `poolQuery`：被伤害计算 FUN_141F66DD0 附近代码高频调用，特征是无分支小函数，
  返回 float。
- `propInt/Bool/Float`：三者相邻（0x1922190/1F0/240），开头都有
  `F6 41 60 01`（测试对象标志）+ 对 `component+0x84+id*24` 的读取。
- `dmgCalc/hitDispatch`：互为上下游，dispatcher 里有大 switch（0x6A 等返回码）。
- `doAction`：现成特征码 `48 8D 41 07 48 C1 E0 04 46 3B 04 08`，命中处 -10。

盘上 exe 的 .text 是加密的——反汇编一律用 dump exe（`disasm_rva.py` 默认读它）。

---

## 结构体字段表（无法特征码，靠语义锚推导）

player = 行动控制器地址 - 0x61C8（控制器由 DoAction 钩子第一参数给出）。

| 字段 | 含义 | 语义锚（怎么验证/重找） |
|---|---|---|
| player+0x61C8 | 行动控制器（内联） | IsLongSwordController 校验 owner+0x61C8==控制器 |
| player+0x76B0 | 武器指针 | 指向的 +0x2370 ∈ [0,3] |
| player+0x7D20 | 反应管理器 mgr | mgr+0x7C0 ∈ {0,1} |
| player+0x8938 | TIML 事件位数组 | dword0 bit15 = 见切成功；+0x8954 bit26 = 居合否定(bit250) |
| player+0x7626 | 全局配置门（IB 分支选择） | 常年 0 或 1 |
| player+0x126D8 | 属性组件 | +0x84+id*24 键槽（stride 24，capstone 已验证） |
| weapon+0x2370 | 气刃等级 int32 | 0无/1白/2黄/3红，只在这四个值间跳 |
| weapon+0x2374 | 红气刃持续时间 | 缓慢单调递减 float |
| weapon+0x2388 | 无敌帧计时器 | 见切/居合期间 >0 |
| weapon+0x23A0 | 蓝光强度 float | 叮的瞬间跳 ~1.0 后衰减；空挥全程 0 |
| weapon+0x2368 | 特殊纳刀姿态 byte | 纳刀特殊姿态时为 1 |
| mgr+0x7C0 | 成功待机 | 动作开始置 1 |
| mgr+0x28 → pool+0xE4 | 无伤窗口剩余秒 | float ≥ 0，注入即变 0.5 |
| mgr+0x660/661/662 | 特殊追击锁存 | 见切/居合后续输入改写器 FUN_14118C320 消费 |

字段重新推导的两条路：

1. **静态**：在反编译里找引用该字段的函数（用上面的语义锚或立即数扫描
   `scan_277.py` 改成目标常量），看新版的指令位移。
2. **动态**：F5 挖掘器成对对比。流程：训练场，空挥见切×3 → 按 F5 →
   叮到见切×3 → 按 F5 → 读 MINER ANALYSIS 报告，相关度排行第一的就是信号。
   （居合 bit250、蓝光 0x23A0 都是这么挖出来的。）

---

## 工具清单（Reverse/）

| 工具 | 用途 |
|---|---|
| `disasm_rva.py <rva> [len]` | 反汇编 dump exe（读加密盘上 exe 会得到乱码） |
| `vadump.py <va> [len]` | 按 VA 读字节/字符串（UTF-8/Shift-JIS 自动解码） |
| `sig_check.py` | 特征码生成/验证（--emit / --verify / 单个 RVA） |
| `fsm_table.py` | 解析每武器条件描述符表（0x143FD5590, stride 0x130） |
| `scan_277.py` | 在 .text 扫 `cmp r32, imm32` 立即数（改脚本常量可扫任意值） |
| `MonsterHunterWorld_dump.exe(.c)` | 未加壳转储 + Ghidra 反编译（280MB） |
