# 气刃 / 勇气系统（当前设计 plan B + 全部机制定案）

> 过程见 `SESSIONS.md`（09-24 起气刃章节、10-24/25 勇气充能、10-25 条件值追捕 12+ 轮）。

## 术语与字段（user 定案，勿混）

- **气刃值（白刃期=勇气充能条）= 等级计时器 `weapon+0x2374`**（掉级倒计时条，条刻度 0..1；
  红刃期同一根条 = 勇气模式剩余时间）。
- **练气槽（红条）= `weapon+0x2368`**（气刃斩消耗/普攻回充/见切回填，**完全原生，不管**）。
- **气刃等级 = `weapon+0x2370`**（0无/1白/2黄/3红）。
- **战技槽（规划中）**：另挂一个**新字段**做蓝条战技条（战技系统用），不复用练气——
  UI/充能/消耗全套都要新建。

## 条件真值（10-25 追捕终局，12+ 轮）

- FSM 条件属性**必须是注册表里存在的字符串**：只有 `オーラレベル白以上`(0x269, lv≥1) 与
  `オーラレベル赤以上`(0x26a, lv≥3)。**`オーラレベル黄以上` 不存在**——写了恒假、转换实例
  建不出来（09-22 `R+↑` 同类教训）。
- **裸写 +0x2370（含 timer/镜像/+0x2358 tier/+0x2CE4/+0xAD4）不驱动条件真值**；
  **SetSpiritLevel 配方（14211c5f0 复刻）驱动**（round-4 J二連 + 赤以上在 DLL 红刃下成立）。
  条件值的引擎存储从未定位（挖掘 10 窗口、行页守卫、HW 断点三路全败；堆行路线退役）。
- 原版作者（MHW-valor）的"黄刃检测"= `白以上 AND 練気ゲージ判定(気刃大回転斬り AND R`
  —— 用量条判定伪造颜色语义。FSM 侧 user 在 09-20 结论上的实证反转：**链接条件测按键
  属性，但条件属性里的練気ゲージ判定族读的是练气槽**。

## 勇气模式 plan B（2026-10-25 user 定案，现行）

**白刃 = 充能相位，红刃 = 勇气模式**（FSM 勇气分支由 user 挂 `オーラレベル赤以上`）。

- **入场**：白刃时充能把条从 <0.999 推过 ≥0.999（跨域检测，天生满条不触发）→
  `SetSpiritLevel(3, "charge-full")` 直达红刃（等级+timer=1.0+宽限+红刀光 FX）。
- **充能**：chargehit/chargestart 规则把条按刻度推高；valor 档（第 4 参维持额）只在
  等级 ≥3 生效；黄刃无角色，只是红→白折返的过路态（吃满额）。
- **出场**：红刃到期或居合/兜割扣级落到黄（原生 level-1+timer=1.0）→ PollValorLifecycle
  一帧内 `SetSpiritLevel(1, "valor-exit", 0.05)` 折回白刃（tracker 钉 1 防重复日志）。
- **进白清条惯例**：任何进白条都清 0.05（无刃→白转换直接给 0.05；黄/红→白掉级沿
  `valor-recharge` 压回）→ 条只能靠打攒满。
- **gainhit 只喂练气**（0x2368；76/77/109 行服务刚1 的练气门槛）——round-1 的"练气满→升黄"
  与 0→白带满条转换已删除，勇气入场唯一路径 = 条跨域。
- 白→黄的自然升级招（若 FSM 保留）不折返；黄→白自然掉级仍清条（进白惯例）。

### SetSpiritLevel 配方（DLL 函数，复刻 FUN_14211c5f0）

写 `+0x2370=lv` + `+0x2374=timerValue`（升级 1.0/进白 0.05）+ `+0x2390`=w03p+0x884 宽限 +
`g_auraSet(weapon, fxId, 0,0,0)` + 锁 `+0xAD4`：白 fx100/ad4=100，黄 0x65/0x66，红 0x68/0x69。
（id 全部抄自 14211c5f0 各分支；原生升级不刷计时器是我们 refresh=1.0 的有意偏离。）

## 练气槽原版机制（原版自理，DLL 只在规则里干预）

- 累加器 FUN_142122410（正增益先乘 mgr 标量；clamp 0..1；刷 +0x236C）。**全游戏仅 3 调用方**：
  wrapper 消耗 / 动作结束回填（见切成功 +1.0 + 计时延长 +0x888）/ TIML type-7 命中增益。
- 消耗 wrapper FUN_142122F90：bit0 门 ∧ cost>0 ∧ 练气≥cost 才扣（扣成功才置 +0x2398=
  拖尾/红光武装）；86 见切额外直写清空练气（绕过累加器，GAUGEEVT 不可见）。
- 回气状态：兜割后 `+0x2378`（普通 10s/红 15s）、居合后 `+0x2388`（15s/掐点 30s，
  掐点=`+0x2CED`）；updater 按 rate（w03p +0x840..854）给练气回气。
- 等级衰减：每帧 `dt×(1/Duration[level])×(1/FUN_1411c27c0(mgr))`；到 0：lv≥2 → lv-1 +
  timer=1.0（+原生通知）；白 → 直接归 0 连 timer 清零。`+0x2390`>0 期间不衰减。
- 原生消耗 id 实测表（GAUGEEVT）：0(-0.15/-0.2) 1(-0.2) 11/12/16(-0.15) 67(-0.15) 68(-0.2)
  69(-0.25) 70(-0.2) 73/74/75(-0.15/-0.2) 86(-0.1 后清空) 94(-0.2)。自定义 105/106/108/109/112
  原生零消耗（复用 entry 的 type-7 增益照发 +0.075~0.11）。

## gauge-rules.txt 规则语法（部署于游戏 nativePC/plugins/，Shift+F6 热重载）

```
rewrite <set> <id> <delta> [origVal]   # 动作期间气刃事件改写（origVal 精准匹配 ±0.005）
cost    <set> <id> <amount>            # 动作 begin 扣练气（直调累加器：无 bit0 门/无足额预检）
fx      <set> <id> <fxId>              # begin 发身体特效（ADD670）
gate    <set> <id> <minGauge> <nohit>  # begin 练气达标→注入no-hit+清槽。⚠见切类禁用:
                                       # 帧杀保护在仲裁上游,会杀死叮→bit15→整条反击链
timer   <set> <id> <seconds>           # begin 延长掉级倒计时（秒,按当前等级时长换算）
timerhit <set> <id> <seconds>          # 命中才延长（多段叠加）
gainhit <set> <id> <amt> [valorAmt]    # 命中喂练气（空挥不触发; id 0=通配 set-3）
chargehit <set> <id>[@prevId] <bar> [valorBar]  # 命中充勇气条; @prev=语境单元格
chargehit-from <set> <prevId> <amt> [valorAmt]  # 语境默认行
chargestart <set> <id> <amt> [valorAmt]         # 起手无条件充（永远吃不到命中的招）
trail   <set> <id>                     # 当前动作时每帧写 +0x2398=1（拖尾武装）
regen   <set> <id> <seconds> <variant> [rate]   # variant 0=兜割+0x2378 / 1=居合+0x2388
option charge-takeover 1               # 清零原生 TIML 命中增益(caller 0x2124f13),gainhit 独充（休眠）
```

四档解析：**ctx 单元格(id@prev) > from 默认 > 普通招行 > 通配**。prev 读
controller+0xC4/+0xC8（真实转换才写）。日志 tier= 字段逐发核对。

**同招双 id（user FSM-Studio 连错根因）**：架势→袈裟斩走 FSM 链接执行发节点
ActionNo=82/83/84；连段按键路径同招=请求 id 7/8/9。ctx 行按入口路径配 id；from 行免疫。

## 刚1 相关（现行方案）

- **DRAWGANG1 拔刀刚一**（draw-gang1.txt）：任何状态的气刃斩1 族请求（3:11/3:12）+ 红刃 +
  练气>0.15 → 放 1 帧原生拔刀（携带态翻转=原生 3:11 类职责）→ 强制 3:112。不看动作历史
  （收刀态集合判不可靠已退役）。gang1 记账：按下即武装 0.15，settle 防 vanilla 空挥清槽。
- **SHEATHREWRITE 收刀改写**（sheath-rewrite.txt 默认 `112 0 3 0.15 0 11`）：收刀态 R2 的
  {3,11/12} 红刃+练气达标 → 改写目标招；nohitSec=0（防御走原生格挡）。收刀判定=tracker
  上一动作 set 1（weapon+0xC71 不可靠已弃用）。
- 计数招=3:112（LMT345，TIML 带防御帧 0x90080000 f0→f12），原版见切 86 行为回归原版。
- 原版叮触发链保留：bit15(见切)/bit250(居合)/NegrEvent/CondEval → 反击窗 + 追加 no-hit +
  force 刚1。**pool+0xE4 no-hit = "游戏层面没被攻击过"**，与弹反计数互斥（帧杀保护必须
  后置于叮）。

## 退役/休眠系统（防再发明）

- `hit-charge=-1`：旧"练气条→满充强制红刃"系统退役（ActivateValorMode 裸写代码保留未调）。
- charge-takeover/gainhit 充能：round-1 遗产，代码保留休眠（配置不开 option 即关）。
- GUARD 防御帧 + sheath-counter 弹反：10-23 整体删除（转换全转 FSM 内置）；guard.txt/
  sheath-counter.txt 是死配置留盘。
- 勇气纳刀按住检测的 DLL GetAsyncKeyState 路线：可退役（`R押してない` 纯数据边，见 fsm-input.md）。
