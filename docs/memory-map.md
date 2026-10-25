# 内存地图与结构偏移定案

> 结论级速查。过程证据链见 `SESSIONS.md` 对应日期；跨更新恢复流程见
> `Reverse/ADDRESS_RECOVERY.md`。所有 RVA 基于当前游戏版本（dump exe 反编译）。

## 地址解析纪律（mod DLL）

- 所有钩子**三级解析**：字节签名（`kHookSigs`，sig_check.py 生成/验证）→ 记录 RVA →
  `nativePC/plugins/ValorGauge.addresses.txt` 覆盖（`name=0xRVA`）。
- 启动打 `MEMORYMAP` 报告（每钩子 ok/BAD + 关键数据地址）。结构偏移漂移走语义探针
  （spirit level 出界、standby、no-hit 异常值报警）。
- 反编译一律用 `Reverse/MonsterHunterWorld_dump.exe.c`（盘上 exe 带壳，.text 加密）。
- detour 必须核对原函数返回值寄存器（rax/xmm0），声明成 void 丢返回值 = 调用方消费
  返回值时野指针崩溃（FxNotifyHook 大剑蓄力案）。

## 玩家/武器链

| 项 | 地址/偏移 |
|---|---|
| 本地玩家 | `*( *(0x145073ED0) + 0x50 )`（manager 链，可能随版本滑动；失效时 fallback 首 set-3 + 武器门） |
| 动作控制器 | `player + 0x61C8`（slot0；slot1 = +0x6878，stride 0x6B0） |
| 当前动作 set:id | `player + 0x6274 / 0x6278`（= controller+0xAC/+0xB0） |
| 武器指针 | `*(player + 0x76B0)`（equipment 块） |
| gauge mgr | `*(player + 0x7D20)`；no-hit 池 `mgr+0x28`，注入槽 `pool+0xE4` |
| 动画组件 | `*(player + 0x468)`；**motion 时钟 = `*(float*)(anim+0x10C)`**（TIML 60fps 帧，动作重启归零；instance+8 是秒线 30Hz 纯记账） |
| 参数表 | `*(*(weapon + 0x9B0) + 0x7810)`（getter FUN_14211c060；**player+0x7810 是另一物**=位置缓存，勿混） |
| 事件位阵列 | `player + 0x8938`（bit0=气刃增减总门；bit15=见切成功锁存；bit11/13=挥刀窗；居合否定 = `player+0x8954` bit26=bit250） |
| FX emitter | `*(player + 0x8808)`（通知函数 FUN_140ADD670） |
| 状态乘数档 | `player + 0x8940` bit17/18/19 → w03p +0x874..0x880 |
| 拖尾组件 | `*(player + 0xE490)`，bit3 在 `[*(+0xE490)+8]` |
| 卡肉字段 | 实体(玩家/怪通用) +0xB2F8/B2FC=冻结倍率，+0xB300=Spd，+0xB310/B314=主/副倒计时（tick 自动复位 1.0） |

## ActionController 布局（player+0x61C8）

- `+0xA8` phase（0 idle/1 请求已接受/2 运行中）；`+0xAC/+0xB0` 当前；`+0xB4/+0xB8` pending；
  **`+0xBC/+0xC0` 缓冲请求**（DoAction 忙时写这里；也是排队的原生写入点）；
  **`+0xC4/+0xC8` 前一动作**（游戏在**真实转换**时自写，缓冲 accept 不动——语境充能靠它）；
  `+0xD0` 前一实例 core；`+0xD8` 前一结束帧；`+0x6A8` 自动结束阈值（<0 关）。
- DoAction = `0x140269CE0`（Ghidra 里非独立函数，按调用点 `(x+0x61C8,&stack)` 搜）。
  phase==0 直写 current，否则写缓冲；真正 begin 在每帧更新器（0x141f78600/0x14026a820）。
- 动作实例：`+8` 帧秒（begin 清零）、`+0xC` dt、`+0x10` core、`+0x18` finished、`+0x28` player；
  实例 vtable：+0x30 begin / +0x38 phase检查 / +0x40 end / +0x48 收尾。
- **帧跳转**：无原生"带起始帧播放"；写 `instance+8`(秒) **并**写 mot 时钟（秒×60），
  fresh-begin（帧回落 ≤2dt）后钉住写入；前向跳帧会跳过中间 TIML 事件（弹反窗会废）。

## 武器结构（LS = wp03，weapon+）

- `+0x9F8` 武器类型 int（LS=3；remapper 分发 `dec eax` 后跳表，反编译 case N = 类型 N+1）。
- `+0x2370` 气刃等级 int32（0无/1白/2黄/3红）。`+0x2374` 等级计时器 float 0..1
  （=**勇气充能条**，每帧按 1/Duration 衰减，bit0 门 + `+0x2390` 免衰减宽限）。
- `+0x2368` 练气槽 float 0..1（气刃斩消耗/普攻回充/见切回填；**同 dword 被居合架势当
  int 0/1/2 复用**——架势期即清零标记；remapper 按 byte 检查它）。`+0x236C` 当次消耗镜像
  （累加器每次调用刷成 playerParam+0x804）。
- `+0x2378` 兜割回气秒 / `+0x2388` 居合回气秒（updater 递减并给 +0x2368 回气，HUD 切显示）；
  `+0x237C` 色刃标志、`+0x2380` 等级快照、`+0x2384` 居合档选择。
- `+0x2390` 升级后免衰减倒计时；`+0x2398`（byte）**气刃消耗成功标志**（wrapper 扣费成功置 1，
  move-end 清零；拖尾/红光的运行时门）；`+0x2399` 见切成功锁存 / `+0x239C` 一次性消费锁存；
  `+0x23A0/+0x23A4` 见切/居合成功超级装甲时间。
- `+0x2350` 气刃量原版累积 float / `+0x2358` 量档位（0-3，原版"蓝条攒满升色"真判定字段，
  是 +0x2370 的缓存镜像之一——但**条件真值不读它**，见 gauge-valor.md）。
- `+0xAB8` 纳刀系动作锁存（98/100/101/102 期间置 1，序列回 1:0 清 0）；
  `+0xC71` 状态翻转**脉冲**（瞬间 0↔1，不是常驻指示器，勿当拔刀/纳刀态读）；
  `+0xA50`/`+0x23C0` 非 0 = 强制纳刀期望（0x23C0 = 特殊纳刀序列标志，唯一清除 = 全量重置
  FUN_142124590）；`+0x2D0` bit0x20/0x40 = 纳刀中/拔刀中。
- `+0xAD4` 常驻刀光色（100白/0x66黄/0x69红；0x67/0x6A 过渡/清除）。
- `+0xAF0/+0xFA0/+0x1450` 三个挂点表控制器（刀/鞘/配件推测；vtable+0x110 写 ctrl+0x181）；
  `+0x2CE4` 开刃阶段机等级捕获镜像；`+0x2CED` 掐点标志；`+0x2D24` 大居合档（0xC/0xD/0xE）。

## w03p 参数（playerParam+，磁盘文件加密 Blowfish-ECB）

- 消耗簇（file+871=runtime skew）：`+0x800` Unk120(+0x236C 衰减分母)、`+0x804` Blade1_Cost、
  `+0x808..0x834` 每招消耗/增益槽（14 个 begin 处理器各传一格）；`+0x840/+0x844` 兜割回气
  rate×2 色档、`+0x848/84C/850/854` 居合回气×4；`+0x858/85C/860` Gauge_Duration_白/黄/红；
  `+0x864` 居合/兜割扣级；`+0x874..0x880` 状态乘数四档；`+0x884` 升级免衰减秒；
  `+0x888` 见切成功计时延长；`+0x8A8/+0x8B8` 见切/居合成功超级装甲秒。
- plp（玩家参数，同款加密，`plp_decrypt.py`）：Stamina_Time_Reduce_mCore @0x250 共 18 行
  （9=LancePowerGuard 最快 150，4=0 不掉，越界如 256=不掉）。加密 key 表 .rdata 0x1431E0000，
  Blowfish P 数组 0x144BE0580，key-schedule init 0x1421F81CE（w03p 全武器同 key，`w03p_tool.py`）。

## 关键函数（RVA，除非另注 VA）

| 函数 | RVA/VA | 语义 |
|---|---|---|
| DoAction | VA 0x140269CE0 | 动作请求入口（hook 点） |
| FUN_142122410 | 0x2122410 | **+0x2368 练气累加器**（仅 3 调用方：wrapper 消耗/动作结束回填/TIML 增益） |
| FUN_14211EF90 | 0x2111EF90 | +0x236C 同款累加器（签名差 `E8 DF 0F 0A`） |
| FUN_142122F90 | 0x2122F90 | 每招消耗 wrapper（bit0 门→清 +0x2398→足额才扣并置 1；86 见切额外直写清空练气） |
| FUN_142123510 | 0x2123510 | 动作结束处理器（见切锁存消费 +1.0/+计时延长；居合/兜割回气档） |
| FUN_1421237B0 | 0x21237B0 | 每帧总更新器（等级衰减/掉级/回气/收刀期望裁决） |
| FUN_14211c5f0 | 0x211C5F0 | **SetSpiritLevel 配方母本**（等级+计时器+宽限+auraSet FX） |
| FUN_142110a90 | 0x2110A90 | auraSet（weapon, fxId, 0,0,0；无直接 xref 指针调用） |
| FUN_142122E40 | 0x21222E40 | 居合/兜割扣级 handler（≥Consumption 时扣级+timer=1.0） |
| FUN_142124b20 | 0x2124B20 | TIML 事件分发器（type7 命中增益；GAUGEEVT caller +0x2124f13） |
| FUN_140ADD670 | VA 0x140ADD670 | FX 身体特效通知（返回句柄!）；0x140AE0D70=纯 thunk |
| FUN_1421224d0 | 0x21224D0 | 通用红光发射器（+0x2398 门 → ADD670 0x7D2/0x7DC） |
| FUN_14118c320 | VA 0x14118C320 | 输入重映射器（IB 态换 FUN_14118d3a0；player=mgrCtx+0xD90） |
| FUN_141F6BD80 | VA | bit15 唯一写入方（sheathed locomotion 时代码） |
| FUN_14203d000 | VA 0x14203D91A 命中 | 注册表行写入器实为瞄准写入（堆行路线退役证据） |
| FUN_141f75530 | VA 0x141F75530 | 卡肉安装器（row+0x1E0/0x200 块 → 实体字段） |
| FUN_142035720 | VA | 卡肉分发器（kind 1/2 → a/b 块） |
| FUN_142131cc0 | VA | 动作步进（步长 = dt × [+0xB2F8] = 慢放本体） |
| FUN_14231f730 / 0x14231f7e0 | VA | TIML 校验器/重定位器（hash 必须在全局注册表 + 行 u0==注册表项+0xC，任一不中**整个 LMT 拒载**） |
| FUN_142233e90 / 0x142234010 | VA | 键游走器/插值器（bufferType 步长表 0x143505C40 stride 0x18；delta==0 哨兵键终止） |
| FUN_14191f7d0 | VA | PlMotionCommon 参数注册函数（name→槽位→类型码，全 65 项入 timl-editor/TIML_PARAM_REGISTRY.md） |
| FUN_140b200c0 | VA | LinkMotion_W03 工厂（MotionNo@+8/Phase1@+0xC，vtable 0x1430B8E20） |

## 每武器静态表

- 行地址 = `0x143FD5590 + type*0x130`；LS 行 0x143FD5920：+0x40 = set-3 类表 0x142EF5C30
  （113 项，stride 0x20，槽位=武器枚举号），+0x48/+0x50 = set-3 请求表 0x143F4D6A0（113 条），
  +0x70 = slot1 类表。类名 ASCII 在类对象 vtable+0x20；enum 名在 entry+0x18→blob+0x20。
- 条件属性描述符表同 0x143FD5590 块；全局 fallback 0x1434DA930；名字解析 FUN_140b2e4e0
  （strcmp 链）/分武器组 FUN_140b306a0；字符串表 ~0x1430BA8F0。
  **LS 条件成员块 0x269-0x277**：0x269=オーラレベル白以上、0x26a=赤以上、0x26b-0x275=練気ゲージ判定
  （気刃斬りI..見切り）、0x276=居合斬り、0x277=見切り回避成功後の攻撃ヒット。
  **没有黄以上、没有裸等级属性**。转换类 vtable 0x1431C5668/0x1431C56B0（check
  0x140F0DC60/0x140F0DCD0）只是编辑器/候选仲裁用，玩法求值不走它们。
- 反射注册表：base+0x5012048（count +0xB8，rowTable *(+0xC8)，counter=row+0x24+idx*2）；
  组件 key 解析 `*(int32*)(*(player+0x126D8) + 0x84 + id*24)`（stride 0x18）。
- cAttackParamPl 运行时（col ATK 行，0x270B）：+0x08 Index、+0x18 Attack_Type、+0x20 MV、
  +0x24 Fixed、+0x28 Part_Modifier、+0x68 Stun、+0x6c Exhaust、+0x1c8 Soften、+0x1d0 Elem、
  +0x1d4 Status、+0x1d8 Mount、+0x232 MindsEye、+0x240 Flinch、**+0x1EC/+0x20C HitLagSpd a/b、
  +0x1F0/+0x210 HitLag a/b**、卡肉块 +0x1E0/+0x200。类名串 "cAttackParamPl" @0x142F12DD0，
  Load FUN_14027d880 / Save FUN_14027dd20，vtable 0x142F12DE0。
