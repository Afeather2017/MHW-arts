# FSM 路由 / 条件属性 / 输入系统

> 过程见 `SESSIONS.md`（09-19/09-20/09-22/09-25/10-12/10-13/10-25 条件值追捕）。
> FSM 编辑工具：`FSM-Studio/`（独立 git 仓库，浏览器编辑 .fsm/.xml，直接保存可载入二进制）。

## 两条进入路径与编号解耦

- **零入边节点**（居合 64/65、回避、跳跃等）= 动作请求按 ActionNo 匹配进入；
  **链接进入** = 源节点条件点火。node-less id（98/99/100/103…无节点）照播动作——
  **动作层与 FSM 各自 authored 对齐，不靠 id 互查**。
- **链接条件测按键属性，不测请求 id**（09-20 XML 实证）。node-entry beats request
  （节点入口动作覆盖原请求）。
- **同招双 id**：FSM 链接执行发目标节点 ActionNo（袈裟斩=82/83/84）；按键输入路径
  同招=请求 id（7/8/9）。规则/日志按入口路径配 id（from 语境行免疫此二义性）。
- LinkMotion（MOT 节点）= 第三条通道：FSM 直播 LMT entry，不经过动作层
  （详见 motion-data.md）。

## 组合键/方向派生（已解决，照配方做即可）

**配方（user-verified 贯通）**：目标招 entry 的 PlMotionInput mFlag1 非零键 OR 方向组使能簇
（NFH 撒网值 0xCFFC10CC / 0xCFFC30CC）→ FSM 在该招节点接 R+方向 边。完成。

- **FreeKinetics 已有实现**：窗口编辑器『任意位窗』`freehk.timl_bit_window`（PI 轨
  mFlag..mFlag6 通用位窗）+ BIT_SEMANTICS 位语义速查（PI.mFlag1 已带位簇标签）；
  完整解锁簇逐键配方表在 `timl-editor/README.md`。
- **使能位只对当前播放动作生效**——FSM 边挂在哪招，位就打在哪招（首测"无效"的根因）。
- **簇的开窗帧参考引擎取消点**（≈攻击结束+1：106=f44、107≈f34；110 最终以
  PI 首窗 f38、主衔接窗 f58 附近为准，2026-10-31 局内验证），
  **不能打在取消点之前**（[R+方向] 是活值属性，窗一开 FSM 边即点火硬切源动作后摇），
  也**不必为躲误判推迟**（簇晚于取消点=中间出现"早按被 [R] 短版吃掉"的窗口；簇=取消点
  时检查点上方向必赢——部署 FSM 中 cond308 长版边在 cond307 短版边之上，勿调换）。
  基线 b12 与连段缓冲位保持 vanilla 早开，勿剥（剥 b12/b26=杀勇気納刀/R+△，剥
  b14/b16/b30=杀预输入缓冲；2026-10-27(8)(9) 死区方案已弃用，见 SESSIONS/motion-data）。
  10-31 后续 A/B 修正：107/110 共引 106 PI 后纳刀恢复，随后各自复制独立 Action。
  110 单移 mFlag2 无效；四条 PI 轨前中段键整体后移 14 帧后，全部派生和勇気納刀
  时机均局内正确。107 仍约早两帧，user 接受当前表现；纳刀有效位与方向使能位
  的最小集合尚未拆出。部署 LMT md5 `ad6c379b`。
- 配方必须落在 blend 侧（导出物后打补丁活不过下次导出，10-17 丢簇复盘）。
- 最小位集未 bisect，部署维持全簇。背景一句话：09-22 "LS 分类器不写 R+方向" 的旧结论
  已翻案——当年失败 = 属性名未注册 + 动作没带使能位两个问题叠加。
- 键盘玩家的 R+△ 在内部同样生成 3:87 请求，DoAction 层可改写/吞（SHEATHREWRITE 同款
  模式）。否定态属性（R押してない 等）同机制：FSM 引用+动作使能位即激活，纯数据。
- **战技闸门（2026-10-26）就挂在请求层**：DLL 在战技条 <100 时吞 3:87（R+△/Ctrl+左键，
  详见 gauge-valor.md 战技槽节）。所以**战技节点必须走请求路径入边**（零入边 ActionNo=87
  匹配）；若改用 R+△ **属性链接边**，属性读的是原始按键、绕过闸门，条不满也会触发。

## 条件属性注册表（写 FSM 前必查）

- **属性名必须是 .rdata 注册表里的字符串**：LS 条件成员块 0x269-0x277 全集 =
  オーラレベル白以上/赤以上 + 練気ゲージ判定族(0x26b-0x275) + 居合斬り + 見切り成功。
  **没有黄以上/裸等级**。名字解析：FUN_140b2e4e0 全局 strcmp 链 / FUN_140b306a0 分武器组；
  字符串表 ~0x1430BA8F0。
- 输入属性族：R / △ / ○ / R+△ / R+○ / △+○ / △+○+方向 / 回避+↑ / レバー前キープ /
  納刀中 / 各押してない / スタミナ切れ / 気刃斬りII/III 等（原版 29 属性清单见 nfh-analysis）。
- 条件求值时机 = FSM checkpoint；按住到 checkpoint 才生效（buffered）。
- 条件值真值：白以上=lv≥1、赤以上=lv≥3，**由 SetSpiritLevel 配方驱动**（裸写不认），
  存储未定位（追捕 12+ 轮终局，详见 gauge-valor.md）。
- 練気ゲージ判定族读练气槽 +0x2368——原版勇气作者的"黄判定"配方就建在它上面。
- 未用属性速览（第三类防御解锁点在 FSM 路由层）：見切り回避成功後の攻撃ヒット(0x277)
  是反射成员非注册表值。

## 受击/反应路由（10-12 定案）

- 受击反应**不走 FSM 条件**：伤害系统按受击结果直发请求 {3,38}（零入边）。
  三向对照：零伤格挡→38/39/40 族；削韧格挡→35/36/37 族；无防御→身体控制器（武器 FSM 不参与）。
  FSM 只能接管"反应之后"（补 38 节点+出边接反击，南风焓 38→69 同款）。
- 长枪式"格挡成功→反击突刺"是类代码行为（cChargeCounter 族）；LS 等价物=強G 位引出
  38-40 请求 + FSM 出边（纯数据，见 motion-data.md 防御配方）。

## FSM-Studio / XFsm 操作要点

- 最小链接条件：顶层 OperationNode（None 单子/AND=16/OR=17）→ Add Child → VariableNode
  （Property=注册名，Owner=cFSMPl_W03，全部 Is* 不勾，Index=0）。
- 两节点全边交换：右键 `与其他节点交换边…`（mpLinkList 整表对调+mDestinationNodeId 重指；
  条件/顺序随链接走，ActionNo/MotionNo/名称不动）。**直接改闲置节点 ActionNo 有崩溃前科
  （强化射击节点），要换身份用边交换**。
- 属性文本改动必须 byte-verify 部署文件（`print(open(fsm,'rb').read().count('R+\u2191'.encode()))`）。
- XFS 格式全破解（FSM-Studio/README.md + research/）：defs 块可再生、ClassEntry 树、
  canonical 重排会重编 var ordinal（游戏自带文件已 canonical）。
- 试验性 FSM 节点 ActionNo 11 曾致人物卡死（11=收刀 R2 请求号，按号匹配进坏节点）——
  加路由只接线不建节点。
- XFsm（游戏内）崩溃修复史与空 render-error.log=原生 crash 的判读见 SESSIONS 09-14。

## 输入系统（逆向地图，10-25 初版 / 11-02 全解码）

- 轮询模型：DirectInput GetDeviceState（键盘）+ XInputGetState（手柄）每帧拉状态；
  键鼠→手柄位 = 游戏内部映射。静态导入经 delay-load 包装（XInputGetState@0x14273DCC9、
  DirectInput8Create@0x14273DC3E 固定 VA 可 MinHook）。
- **逻辑动作层（11-02 解码，DLL 读输入的正道）**：物理键 →（键位重映射表
  **RVA 0x3EEABF0**，逻辑动作 <0x89 每项 4 字节物理键号，运行时被键位配置改写；
  vanilla 静态表逻辑 23=特殊納刀→物理 9=默认手柄 B/○ 位）→ **逻辑动作位集**，
  按逻辑 id 直接索引（id>>5 选字 / id&31 选位）：`输入上下文 +0x780=当前按住、
  +0x7C0/+0x820=缓冲、+0x7E0=本帧按下沿`（另有 +0x860/+0x8A0 被查询登记位）。
  特殊納刀 = 逻辑动作 23；"R+A 还是 R+B 因人而异" = 重映射表运行时值不同。
- 分类器检查器（mgrCtx type1[combo]，mgrCtx+0xD0 槽）= 逻辑动作条件的对象化：
  +0x28=逻辑输入 id、+0x20=输入上下文、+8 旗标 bit0=按下/持续；评估链
  evaluate→`FUN_1417bd120`→万能查询 `FUN_1419a30d0`，外围玩家门禁
  （+0x7658→+0xa4/a5/a6）+ UI 门禁，动作层再叠 PlMotionInput 使能位——**翻滚中
  组合 23 从不被游戏询问 = 这些门禁叠出来的，不是输入层没收到键**。
  **DLL 读输入直读位集，不调查询函数**（绕开全部门禁）；实装见
  `ValorGauge/src/rollsheathe.cpp` COMBOPROBE 节 + roll-sheathe.txt `input=internal`。
- 分类器静态表 0x143F4D6A0（LS 行，113 键）；(1,23,1,0,3,98) 行 = 特殊納刀切入；
  翻滚 18-22 原无此行（DLL PatchRollSheatheRows 进程内补过，未证实有消费者）。
- **实测键→逻辑 id 映射（INPUTBITS，11-02 局内）**：R={1,6}（`R按住/R押してない`
  条件底层位）、特殊納刀快捷键=24（remap→物理 15；**边沿型，被消费即清零**，
  无常驻按住位）、16/120=常驻状态位非按键、127/116/125=方向类。可查询空间
  id<0x89（0x9c 特例）。
- **发现方法 recipe（游戏更新后按此重找，约一轮逆向+一轮局内）**：
  1. 分类器入口 `FUN_14026c0f0`(RVA 0x26C0F0, 签名见 memorymap kHookSigs
     sheathCap)：param_1=mgrCtx、param_2=动作对象(+0xAC/+0xB0 当前请求)。
  2. 匹配链全在 0x26Bxxx 邻域：b670(驱动)/b830(0x26B830 候选匹配)/ba00(0x26BA00
     检查器评估)/b380(胜出消费)。mgrCtx type 槽 {指针数组,数量} 对 =
     +0xC0+type*0x10 / +0xC8+type*0x10；检查器=数组[comboId]，+0x28=逻辑输入
     id、+0x20=输入上下文、+8 旗标。
  3. 检查器类（c23 实测 vtable 0x143371ea0）evaluate → `FUN_1417bd120` →
     万能查询 `FUN_1419a30d0(玩家ctx,输入ctx,id,press,mode)` → press
     `FUN_14122fdb0` / hold `FUN_14122fe60` → 位集读法（上条）+ 重映射查表
     `FUN_141230070`（表 RVA 0x3EEABF0）。
  4. **运行期实测（关键一步，纯静态定不了位语义）**：DLL 在 c0f0 钩子**调用期内**
     捕获检查器（跨调用保留指针=UAF 崩过），按 (ctx,id) 对计数 seen≥8 过滤，
     位集全扫 + UI/日志显示 → 用户逐个按键 → [实体键→逻辑 id] 映射当场建表。
     工具已留在 `rollsheathe.cpp`（COMBOPROBE/INPUTBITS/SHEATHIN UI 行，
     addresses.txt `comboProbe=1`）。
  5. 门禁参考（读位集可全绕）：UI `_DAT_1451c4640`+0x146d5/0x146ea、玩家
     +0x7658→+0xa4/a5/a6、查询空间门 `FUN_141230000`。
- DLL 侧自动按键：纯 SendInput 扫描码（虚拟键窗口消息游戏不认）；counter-repeat.txt
  Ctrl 脉冲、gameio.py（tools/game-control/）同通道。
- 分工定案（user）：LLM 实时玩游戏不现实——user 物理操作 + 程序化时机/内存/断点控制。
