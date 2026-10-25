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

## 组合键/方向派生（09-22 结论 + 10-12/13 翻案后的完整模型）

1. **combo→请求分类器 per-weapon**（代码侧静态表，LS 只路由原版 wp03 引用过的组合）。
2. **动作侧使能位**：输入属性随当前动作的 PlMotionInput 使能位写入——R+方向组需要
   该招 entry 的 mFlag1 位簇（配方见 motion-data.md）。09-22 的 "R+↑ 永远假" 两个根因
   各占一半：属性名未注册（已修）+ 动作没带使能位（翻案主因）。
3. 键盘玩家 R+△ 内部生成 3:87 请求（気刃突刺），DoAction 钩子可见——不满足条件的
   请求可在 DoAction 层改写/吞掉（SHEATHREWRITE 同款模式）。
- 否定态属性（R押してない 等）注册于 .rdata，输入系统每帧无条件写 cFSMPlBase
  （registration accessor 全是 stub）——FSM 引用+动作使能位即激活，纯数据。

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

## 输入系统（逆向地图，10-25）

- 轮询模型：DirectInput GetDeviceState（键盘）+ XInputGetState（手柄）每帧拉状态；
  键鼠→手柄位 = 游戏内部映射。静态导入经 delay-load 包装（XInputGetState@0x14273DCC9、
  DirectInput8Create@0x14273DC3E 固定 VA 可 MinHook）。
- DLL 侧自动按键：纯 SendInput 扫描码（虚拟键窗口消息游戏不认）；counter-repeat.txt
  Ctrl 脉冲、gameio.py（tools/game-control/）同通道。
- 分工定案（user）：LLM 实时玩游戏不现实——user 物理操作 + 程序化时机/内存/断点控制。
