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
**user 局内验证通过**（白→红直跳驱动赤以上、红刃折返、充能全链路；部署 DLL md5 `2477ac6e`）。

- **入场**：白刃时充能把条从 <0.999 推过 ≥0.999（跨域检测，天生满条不触发）→
  `SetSpiritLevel(3, "charge-full")` 直达红刃（等级+timer=1.0+宽限+红刀光 FX）。
- **充能**：chargehit/chargestart 规则把条按刻度推高；valor 档（第 4 参维持额）只在
  等级 ≥3 生效；黄刃无角色，只是红→白折返的过路态（吃满额）。
- **出场**：红刃到期或居合/兜割扣级落到黄（原生 level-1+timer=1.0）→ PollValorLifecycle
  一帧内 `SetSpiritLevel(1, "valor-exit", 0.05)` 折回白刃（tracker 钉 1 防重复日志）。
- **进白清条惯例**：任何进白条都清 0.05（无刃→白转换直接给 0.05；黄/红→白掉级沿
  `valor-recharge` 压回）→ 条只能靠打攒满。
- **白刃常驻 keep-white（10-25 追加，user 局内验证通过）**：**HUD 刻度条颜色只在集会所/任务入场时从
  现值初始化，任务中写等级不重刷**（FSM 条件/刀光正常，仅 HUD 色滞后）——定调不深挖
  HUD 刷新路径，用 workaround：太刀永不静置 0 级。PollValorLifecycle 补 manager 链
  观测回退（原先无动作锁直接 return，集会所/任务初帧是盲区），任何 0 级观测沿
  → `SetSpiritLevel(1, "keep-white", 0.05)` 立即回白 → 进任务前 gauge 已是白，
  HUD 入场显示正确（FLiNG 式"进任务前设好"）。⚠mgr 全局 RVA 按 build 候选解析
  （旧常量滑掉曾让链恒死、keep-white 被首拔刀卡住，见 memory-map 本地玩家行）。
  副作用为设计内：长时间不打条 0.05→0 后重挂白（几秒周期，白刀光 FX 随之重复）。
- **overlay 模式行**：mod overlay 左上 Action 行下方 `SPIRIT lv<N> <相位> <条>%`，
  每帧实读 0x2370/0x2374，按等级配色（白=CHARGE/黄=PASS/红=VALOR/灰=NONE，
  出界=BAD 即结构漂移报警语义）；`DisplayWeapon()` = 动作锁 → manager 链回退，
  集会所也显示。任务中白→红时 HUD 恒白属已知（真相看此行）。
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

## 战技槽 / art gauge（2026-10-26 DLL 侧落地，外置字段）

- **字段**：DLL 内 `g_artGauge`（atomic float 0..100，**不占游戏内存**——游戏侧无此系统，
  不复用练气/等级任何槽）。overlay 文字行 `ART n%`（SPIRIT 行下方：充能=蓝，满=绿
  READY）；文字先行，画条是后续里程碑。
- **充能**：挂 **DealDamage 钩子**（`NotifyArtChargeApplied`，2026-10-29 从 dmgCalc 侧
  搬家——FUN_141cc5f80 = HunterPie FUN_DEAL_DAMAGE 同一根"伤害落地"函数，9 参签名
  `void(em* target, int damage, float* pos, isTenderized, isCrit, unk0, unk1, char, attackId)`；
  arg1=受击 em 直接读 +0x12280 id 过滤（arg1 本身就是 em，无需 dmgCalc 侧的三候选探测），
  arg2=伤害 int（**按伤害充能的地基，先行接好**），攻击者=dmgCalc 钩子锁存
  `g_lastHitAttacker`（dispatcher 先 calc 后 apply；DDART 日志盯锁存质量））。
  按 `ValorGauge.art.txt` charge 行加量，100 封顶。**charge-dmg 伤害缩放
  （2026-10-29）**：`charge-dmg <标定伤害> [封顶%]` 生效时额度 ×(伤害÷标定)，封顶截断
  （现行 `charge-dmg 100 15`：100 伤害=原额度 5%，200=10%，cap 15%；伤害 0 不充）。
  **标定数语义 = 当前显示攻击力÷系数**（user 公式"伤害/1366×K"：1366=武器+药+猫饭组合
  攻击力，组合值 UI 现算不落地——玩家实体/武器/w03p/公式ctx/属性对象 X 五轮猎捕全空，
  退场纪律知识留档 sig_check）。**暴击归一化（10-29）**：DealDamage 签名第 5 参 isCrit
  在手，dmgNorm=damage÷1.25 后进缩放，暴击不再使充能摆动（DDART crit=/dmgNorm= 可验证）。
  **气刃值侧 10-30 user 定案回归手写行**：chargehit-dmg 行已撤（伤害缩放只保留在战技条
  charge-dmg），chargehit/chargehit-from 加**第 5 可选参 [暴击乘数]**——暴击刀额度 ×乘数
  （缺省 1.0），顺序 `<set> <id> <非勇气> [勇气] [暴击乘数]`。
  日志 `ARTGAIN`（含 dmg=；满条 crossing 记 FULL）/ `DDART`（前 8 条：em/dmg/锁存）。
  怪打地形会出伤害数字=同走此函数，靠 id bitset 淘汰；0 伤害/弹刀命中不再充。
- **目标过滤（2026-10-29，两轮收敛）**：**所有充能统一挂 DealDamage 钩子的 em 直读门**
  ——timerhit/gainhit/chargehit/art/旧 HITCHARGE 全在 `NotifyAppliedHitCharges` 里，
  arg1 就是 em，+0x12280 直读 id 查大型怪 bitset（训练场桩 id59 在 bitset=照常充；
  小型怪不充）。dmgCalc a2 侧的三候选探测**已退役**：log 实锤 a2 是小 victim 上下文
  对象不是 em（同一命中 a2=0xE51F240 vs em=0x41C2EA1D30），候选探子对象读出随机小
  整数导致判定逐击翻转（同指针 0xD52F240 判过 0 也判过 1）= 地形漏充。
  **原生练气回充同杀**：游戏自己的命中回充（GaugeAddHook，caller +0x2124f13，val>0）
  对地形命中照样发（log: GAUGEEVT val=0.26 跟在地形命中后）→ 按 DealDamage 锁存
  `g_lastAppliedTargetMonster` 归零（负值消耗/见切回填/rewrite 不碰；锁存默认 true=
  未判定前放行）。**练习目标特例（10-29 user 定案：原版打中木桶也回充）**：放行名单
  = bitset ∪ {57,58}（训练场木桶，`IsChargeableTargetId`；59 大桩本就在 bitset）。
  日志 `DDART em= id= dmg= latch=`（前 8 条）/ `GAUGERULE terrain:
  suppressed native gain`（限 40 条），计数 `g_nonMonsterSkips`。
- **释放闸门（输入侧"user 定案：组合键不进游戏"）**：R+△（键鼠 Ctrl+左键）在游戏内部
  生成 **3:87 请求**（気刃突刺，10-25 输入地图定案）。条 <100 时 DoAction 钩子直接**吞掉**
  该请求（在 tracker 之前 return false）：不落地、不进缓冲、不进 tracker——vanilla 招与
  请求路径 FSM 入边都进不来；条满放行，游戏接受即清零（`consume 1`）。日志
  `ARTGATE swallow`（限频）/ `ART released`。本机玩家锁 + 武器门直查（不等 session lock，
  首按即闸）。
- **FSM 侧接线铁律**：战技节点必须走**请求路径入边**（零入边节点 ActionNo=87 匹配）；
  **不可用按键属性链接边**（R+△ 属性读原始按键，绕过闸门——条不满也会触发）。
- **任务门/战斗区门（2026-10-27，三轮）**：条是**战斗区会话状态**。显示判定
  `InCombatZone()` = `InQuestState()`（`*(base+0x500ED30)`→State@+0x54==2，`questStateMgr`
  可覆盖）**∪** 可战斗 stage 白名单 `PlayableStageId()`（`*(base+0x500ECA0)`→int32
  stage@+0xAED0，`zoneMgr` 可覆盖）：101..109 猎场+导之地（**任务图=探索图同 id**）、
  201..203 竞技场、401..417 剧情特殊区、**504 训练场**（不是任务也能打——user 定案）。
  排除：主菜单 0、据点 301..306、房间 501..503/505/506。
  **清零判定 `PollArtQuestGate()` 三个触发器**：①进入**真任务**边沿（quest id
  @+0x4C 跨 0→正——探索/训练场跑在 quest 框架里但 id 空，HunterPie `isQuestInvalid=
  Id<=0` 同源；**探索→同图任务 stage 与 State 都可能不变，只有 id 边能抓到**，user bug
  实证）②换区边沿（stage 变化；-1 读数不参与边沿，加载画面不误触发）③战斗区外（原有）。
  日志 `ART gauge wiped (quest start / zone change / not in combat zone)`。地址读全挂
  （游戏更新 RVA 滑掉）时边沿全静默、只剩区外门退化工作。闸门本身不看区域态（条 <100
  照吞 R+△）。
- **配置 `ValorGauge.art.txt`**（缺文件=系统全关；Shift+F6 热重载）：
  `gate <set> <id>...`（吞请求清单，默认 `3 87`）/ `charge <set> <id> <percent>`
  （id 0=set 通配，精确行优先，可多行）/ `consume <0|1>`。首轮部署默认
  `gate 3 87` + `charge 3 0 2`（任意 set-3 连接命中 +2%）+ `consume 1`。
- 手柄/键鼠原始输入层**未拦截**（XInputGetState@0x14273DCC9、DirectInput8Create@0x14273DC3E
  可 MinHook，SESSIONS 10-25 输入地图）——闸门只看请求层；若日后 FSM 要用属性边，再上
  输入层拦截（键盘需 dinput GetDeviceState COM vtable 钩，工作量中等）。

## 退役/休眠系统（防再发明）

- `hit-charge=-1`：旧"练气条→满充强制红刃"系统退役（ActivateValorMode 裸写代码保留未调）。
- charge-takeover/gainhit 充能：round-1 遗产，代码保留休眠（配置不开 option 即关）。
- GUARD 防御帧 + sheath-counter 弹反：10-23 整体删除（转换全转 FSM 内置）；guard.txt/
  sheath-counter.txt 是死配置留盘。
- 勇气纳刀按住检测的 DLL GetAsyncKeyState 路线：可退役（`R押してない` 纯数据边，见 fsm-input.md）。

## 翻滚切勇気納刀 (ROLLSHEATHE + SHEATHGATE, 2026-11-02 定稿)

**行为**：翻滚(动作 18/19=前滚 LMT11、20/21=侧滚 LMT12/13、22=后滚 LMT10)中按住
**纳刀键**(任意绑定, 默认鼠标侧键)且 motion 帧 ≥ 阈值 → DoAction {3,98} 勇気納刀。
阈值默认 18/19=28、20/21=34、22=72(动作帧)，配置 `nativePC/plugins/
ValorGauge.roll-sheathe.txt`(enable/`input=side|internal|any`/各 id 阈值/
`gateMs`/`block`)，改后重启生效。

**SHEATHGATE（总闸，11-02 user 定案）**：纳刀输入被看到的时刻起 `gateMs`(3000)ms
内，DoAction 层拒绝 `block`(默认 1,11,12,66,112 = 気刃斬1/刚一族全部观测形态)，
98 永远放行。**为什么需要**：{3,98} 落地后，分类器(b590)/FSM 链接边/缓冲输入会在
翻滚出招检查点把残余纳刀输入反复消费成 {3,11}(收刀态 R2=刚一, REQTRACE 实证
callerRVA=0x26b640 每 ~1.6s 复发)；0.5s 窗口不够(后滚 f72→检查点 >0.63s)，
3s 实测全盖住。副作用：纳刀键按下后 3s 内拔刀刚一(收刀态 R2)也被拦。

**实现**：①`PollRollSheathe()`：本地玩家 controller 当前动作(0xAC/0xB0)+
motion(anim+0x10C)+输入源 → g_originalDoAction 直呼+tracker 同步。输入源
`input=`：side=GetAsyncKeyState 侧键(过渡版)/internal=游戏逻辑输入位集/any=并集。
②**内部输入读取（正道，绑定无关）**：SheathCapHook(c0f0) **调用期内**同步捕获
检查器(mgrCtx+0xD0 槽[23])的 +0x20 输入上下文/+0x28 逻辑输入 id(实证=24)，
按 (ctx,id) 对计数 seen≥8 才可信；位集读 `ctx+0x780/0x7C0/0x7E0/0x820` 按 id
位索引(按住/缓冲/边沿/缓冲2)。**铁律：mgrCtx 是每次调用的短命上下文，跨调用
保留指针=UAF(崩过)**。③SHEATHGATE + 滑窗重臂(PollSheathGateRefresh)+UI 行
(SHEATHIN, 绿=检测到/黄=闸门)。

**按键检测语义（11-02 实证）**：位集位是**边沿/缓冲痕迹，游戏消费即清零**
(h 只亮一帧)，按住不清零≠按住状态；闸门等效"按下+3s"。换绑定(T 键)检测照常
= 逻辑动作层与键位重映射表(RVA 0x3EEABF0, 逻辑<0x89→物理键)消化了实体键。
检查器 +0x28 的逻辑输入 id(24) ≠ 分类器 combo id(23)，两个编号空间。

**为什么数据侧全灭 (5 轮教训，勿重蹈)**：FSM 边(属性不走 getter)、TIML PI 位
(回避动作类不消费组合23)、分类器行(已禁 16 行但每键 51+ 键都有同类行, 打地鼠;
且 FSM 链接边根本不走分类器)、检查器求值(短命上下文+未武装态必崩)。**DoAction
总闸是唯一能盖住全部消费者的位置**。16 行补丁与 REQTRACE 保留(降噪+溯源)。
排查：UI SHEATHIN 行；Log(SHEATHCUT[i/s]/SHEATHGATE swallow/REQTRACE)；
addresses.txt `comboProbe=1`(SHEATHIN23 stable/位集转换)。
