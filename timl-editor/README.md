# TIML 窗口编辑器 (FreeKinetics 扩展) + 导出验证链

覆盖五个功能: **防御帧 (b28 角度格挡) / 无敌帧 (b26+2048门) / 攻击帧 (EventCollision.mFlag
整值窗) / 按键窗 (PlMotionInput 位窗) / 霸体帧 (b24)**。完全复用 FreeKinetics 的 TIML
表示 (TIML action → `FreeHKTiml.m<hash>` fcurve, bigFlags 类按 array_index 0/1 拆
低/高 16 位), 不引入任何新序列化 — 编辑结果直接走 addon 原有导出。

## 安装

**源码正本 = FreeKinetics 仓库** (功能高度耦合, 不在 MHW-arts 留副本):
`%APPDATA%/Blender Foundation/Blender/4.3/scripts/addons/MHW-FreeKinetics/` 自带 .git,
`timl_window_editor.py` + `__init__.py` 挂钩 + `freeHKActionNodes.py` 排序修复已提交
(main: ff42eff 编辑器 / d1bc9ce parseBigFlag 排序修复)。重开 Blender 即生效。
改编辑器直接在 addon 仓库改, 本目录只保留验证工具链与文档。

**附带修了 addon 两个既有 bug** (见文末"addon 修复"), 对未手工单边改过双通道的
文件零影响 (已用 0.0.2 重导出逐字节回归验证)。

## GUI 用法

Blender 里 N 面板 → **FreeHK** 标签 → "TIML 窗口编辑" (3D 视口和 Dope Sheet 都有):

1. `TIML Action` 选择器: 搜索选 TIML action (如 `TIML::nTimelineParam::PlMotionCommon.292`)。
   名字尾号与 entry 的对应关系看节点树 w03_00 里 TIMLActionNode 的接线。
2. `起/止` 两格是共享帧输入 (下面所有按钮都用它), 整数帧, 60fps (12帧=0.2s)。
3. 按类别出按钮:
   - **PlMotionCommon**: `无敌窗` (b26+2048门, 自动清窗内 b28) / `霸体窗` (b24) /
     `防御窗` (b28 角度格挡, 自动清窗内 b26) / `加2048门`。
   - **EventCollision**: `槽` (0=关 1/2/3=开) + `应用` → mFlag 整值窗。
   - **PlMotionInput**: `轨` (0=mFlag 1=mFlag1 2=mFlag2) + `位` + `开窗/关窗`。
     位义: b8=翻滚 b14/16/30=连段预输入 b17+31=特殊派生窗。
   - **组合键解锁簇** (PlMotionInput 面板专属): `起` + `开簇→末尾/关簇→末尾`。
     = 起帧起的已有非零键 OR `0xC3FC00CC` (R2+方向使能; 0 值关窗键不动), 配方见下文。
4. 面板底部"当前窗口"实时列出该 action 解码出的全部窗口。

无头用法 (脚本/批处理):

```python
bpy.ops.freehk.timl_iframe_window(action_name="TIML::nTimelineParam::PlMotionCommon.292", start=0, end=12)
bpy.ops.freehk.timl_armor_window(action_name=..., start=20, end=40)
bpy.ops.freehk.timl_guard_window(action_name=..., start=60, end=72)
bpy.ops.freehk.timl_attack_window(action_name=..., slot=3, start=50, end=70)
bpy.ops.freehk.timl_input_paint(action_name=..., track=0, bit=8, start=10, end=30, on=True)
bpy.ops.freehk.timl_gate(action_name=..., frame=0)
bpy.ops.freehk.timl_list_windows(action_name=...)   # 清单 → 控制台
```

## 窗口配方 (实证来源: LMT_TIML research + AGENTS.md 局内验证)

| 功能 | mFlag 位 | 门键 | 互斥处理 |
|---|---|---|---|
| 无敌帧 | b26 (0x04000000) | mTrigger 2048 (窗内, 位置无关, 一次即够) | 清窗内 b28 |
| 霸体帧 | b24 (0x01000000) | 无 | — |
| 防御帧 | b28 (0x10000000) 角度格挡 | 无 (格挡不走 2048 门) | 清窗内 b26 |
| 攻击帧 | EventCollisionNN.mFlag | — | 整值 0/1/2/3 |
| 按键 | PlMotionInput.mFlag/mFlag1/mFlag2 | — | 位窗 |

规则: 新建 PlMotionCommon.mFlag 轨种 f0=基线 `0x02400006` (b25|b22|b1|b2); 编辑只动
目标位, 同键其他位保留; 一律整数帧 (小数帧是已知崩溃源); 新键继承邻键的插值/back/period
(vanilla: mFlag 族 transition=EXPO(5), mTrigger=SINE(6))。

**2026-10-15 位义更正**: 原版见切 140 的 `mFlag f0=0x80400000` 是 **b31+b22**, 不是
b26 — 旧研究笔记把 nibble 读错了 (b26=0x04000000, 0x00400000 是 b22)。原版见切的
无敌是 FSM/动作层授予的 (TIML b26 清零无效, 09-18 结论), 与此互证。

## 导出验证链 (0.0.1 → 0.0.2)

```bash
# 1) 无头导出 (与 GUI 导出按钮同代码路径 exportNodeToFile)
BLEND="E:\MHW-arts\MHW-valor-blender\MHW-Valor-0.0.1.blend" \
  OUT="E:\MHW-arts\timl-editor\_verif_001.lmt" \
  E:\MHW-arts\tools\blender-4.3.2-windows-x64\blender.exe -b --python _export_blend_lmt.py
# 同理 0.0.2-345 → _verif_002.lmt

# 2) 逐 entry TIML diff (解析器 = TIML-Studio 的 27 测试字节级验证过的那套)
node E:\MHW-arts\TIML-Studio\dist\timl-diff.js _verif_001.lmt _verif_002.lmt
#    [--entry 105 140 ...] 可只看指定 entry
```

报告读法: `-` = A(0.0.1) 有 B 无, `+` = B 新增, `~` = 轨道逐键差异 (含类别块整体
搬移时的 "轨道仅X有")。已产出: `_verif_diff_001_002.txt` — **5 个 entry 差异**
(105 摘 b26 / 140↔345 类别块互换 / 342 加攻击窗+2048门 / 306 输入窗重排 +
mFlag 0x12400006→0x52400006 (b30)) 与 hist-MHW-Valor.md 的手工轨迹一致。

**验收基线 (2026-10-15 通过)**: 在 0.0.2 拷贝上跑五类操作各一次 (entry 105), 导出
后 diff 只含这六个操作的精确效果, 全文件其余 entry 逐字节不变 (`_acceptance.lmt`,
`_acceptance.log`)。流程注意: **ops 改的是内存 blend, 导出必须同会话** (或先保存)。

## 位语义 (2026-10-15 汇总)

面板有"位语义速查"栏 (按所选 action 的类别出), 置信度标注: **确证**=局内A/B验证 /
**推断**=强证据未终验 / **未解**=仅观测到模式。编辑报告与窗口清单同样带标签。

**防御系 (PlMotionCommon, mFlag)**:

| 位 | 语义 | 置信度 |
|---|---|---|
| b26 | 无敌 (no-hit, 需 mTrigger 2048 门) | 确证 |
| b24 | 霸体 (super armor) | 确证 |
| b28 | 角度格挡 (正面格挡/背面受击) | 确证 |
| b25/b22/b1/b2 | 基线位 (保留) | 确证 |
| b27 | 霸体? (115 开窗 / 108,109,338-340 尾脉冲) | 未解 |
| b31 | GP? 见切/收刀族全程持有; 他版306 = b31+b26 组合GP窗 | 未解 |
| b30 | 未知 (0.0.2-345 的 306 新增 0x52400006) | 未解 |

**防御系其他轨**: PC.mFlag1 `b30+b28` = 零伤格挡配方成分 (他版306: 0x50000000,
配 mStaminaParamIndex=256) [推断]; PC.mFlag2 = 事件位写入轨 (值=要写的 event bit),
b15 = 见切成功锁存/纳刀态信号 [推断]; mTrigger: 2048=无敌门 [确证], 1/2=通用事件码
[推断], 65537=见切起手码 [未解]。

**mStaminaParamIndex (0x85A14934) = 強G掉耐力档位 (2026-10-17 user 实测定案)**:
38-40 状态决定"掉不掉", 这条轨决定"掉多快" — **无轨 = 游戏默认档 (慢), 有轨 = 查表**。
表真值 (plp 解密读出, 18 行, 行序以 PlPlayerParam.cs 为权威, 见 AGENTS 2026-10-17(2)):
1=8, 2=30, 3=50, 4=0(不掉), 5=0, 6=50, 7=70, 8=100, 9=150(长枪强格挡, 最快),
10=50, 11=50(钩爪吸取), 12=10(钩爪反击), 13-15=20/40/60, 16=1, 17-18=60/100
(单位=耐力/秒, 1-based); 256 越界→不掉。user 局内: 9 最快/8>10/1 慢 吻合;
7 vs 8 与 10 vs 11 两处体感与表值有出入, 待对照复测。306 现用 10 (=50/秒)。GUI 注意: 面板"耐力档"滑条是**配方输入** (-1=不动),
不是数据现值 — 现值看滑条下方的"mStaminaParamIndex 数据: …"标签;
调掉速 = 填值 + 起/止窗 (306 用 起=0+至末尾) + `仅写耐力档` 按钮, 不动任何位窗。

**按键系 (PlMotionInput = 输入接受窗)**:

| 轨 | 位 | 语义 | 置信度 |
|---|---|---|---|
| mFlag | b8 | 翻滚接受 | 确证 |
| mFlag | b14/b16/b30 | 连段预输入 | 确证 |
| mFlag | b17+b31 | 特殊派生窗 (见切→必殺攻撃, 成对) | 确证 |
| mFlag | b9/b15/b0 | 伴出位/未知 | 未解 |
| mFlag1 | 位簇 b2/b3/b6/b7/b18-b25/b30/b31 | R2+方向组合键解锁簇 (整簇实证, 单位未拆) | 确证 |
| mFlag1 | b12+b26 | 基线位 (原版动作自带, 非解锁成分) | 确证 |
| mFlag1 | b27/b28/b29/b13 | 簇次级位 (仅中段键出现, 作用未解) | 未解 |
| mFlag2 | b4-b9, b22/b23 | 未解 (值族 0x150/0x3F0/0x3C300) | 未解 |
| mFlag3-6 | — | 未解 (mFlag4 在 342 见 4键) | 未解 |

面板"任意位窗"(轨 0-6) 可对所有 bigFlags 轨画窗; 新观测到的语义直接改
`timl_window_editor.py` 里的 `BIT_SEMANTICS` 表 (标签+置信度), 面板/清单/报告同步生效。

### R2+方向组合键解锁配方 (2026-10-17 实证部署)

机制: mFlag1 位簇整簇置位后, **当前播放动作**期间输入层才写组合键属性 (R+↑ 等),
FSM 的 R+方向 链接才能路由; 无簇则恒不写 (10-13 局内 A/B 三连定案, 9-22 旧结论翻案)。
配方 = 该招 entry 的 PlMotionInput.mFlag1 非零键 OR `0xC3FC00CC` + FSM 节点接 R+方向边。
已部署实例 (气刃二家族, user 局内验证 R+↑ → MOT 衔接 361-363):

| entry | 招式 | mFlag1 键值 | 连段位族(mFlag) |
|---|---|---|---|
| 106 | 気刃斬り2 (act 67) | f24=0x1000 / **f44=0xCFFC10CC** / f54=0xCFFC30CC / f120=0xC7FC10CC | f24 起原样（未剥，user 时机下无碍） |
| 110 | 袈裟斩后气刃2 (act 70) | f38=0x1000 / **f58=0xCFFC30CC** | f38 剥 b14/b16/b30，f58 起 |
| 107 | 気刃斬り上げ (act 72) | f22/f30=基线 / **f42=0xCFFC30CC** / f100=0xCFFC30CC | f22/f30 剥 b14/b16/b30，f42 起 |
| 342 | 纳刀一文字 (act/req 109) | **无簇**（特殊一文字，不应有方向组合） | vanilla 原样 |
| 156 | 跳跃气刃二连斩二段目 (act 76/77) | f0=0xC7FC10CC / f9=0xCFFC30CC / f41=0 | — |

⚠ **切换窗纪律 (2026-10-27 定案)**: 允许切换的时机 = **连段位族 (b14/b16/b30, 含 [R] 门)
与簇同帧开启**，档取 vanilla 特殊派生窗帧 (106=f44 / 110=f58 / 107=f42; 106 因攻击结束
与档贴合而天然正确)。**b14 在 mFlag 低 16 位——剥高位时务必同剥低位**，漏剥则早按仍判成 R
(2026-10-27(7) 教训)。档前按 R / R+方向 一律不收（死区，与长版对称）。

blend 侧定位: entry 的 PlMotionInput action 走节点树接线查 (106/110/156 = `.149/.153/.163`,
勿信 .NNN 后缀); 轨 = `FreeHKTiml.m6E63FBC7` 双 fcurve (array_index 0/1 = 低/高 16 位),
同帧键直接改值即可。mFlag1 疑为 16 个 2-bit 三态字段 (AGENTS 10-13), 单字段最小配方未验证。
参考文件: `w03_00.lmt.r+up_dash` (md5 26e43fd4)。

## addon 修复 (2026-10-15)

1. **`freeHKActionNodes.py parseBigFlag` 键序 bug**: 合并双通道时按 "低通道全部帧 →
   高通道全部帧" 的 dict 插入序产出键列, 单边 (只改一个通道) 加键时导出**乱序键** —
   游戏要求键按帧有序。修复 = `sorted(channelFrames.items())`。已回归: 未编辑的
   0.0.2 重导出逐字节一致 (md5 f724d191...)。
2. (编辑器内) 关闭键继承 "当前值清目标位", 不写 0 — 写 0 会把同通道其他位一起关掉。

## 文件

- `_export_blend_lmt.py` — 参数化无头导出 (BLEND/OUT 环境变量)
- `_inspect_timl_blend.py` / `_dump_timl_action.py` — blend 内 TIML 形态检查
- `_acceptance_run.py` — 验收脚本 (单会话 ops+export)
- `_test_combo_unlock.py` — 组合键解锁工具无头自测 (幂等/关簇/还原/操作符/错误分支,
  2026-10-17 ALL PASS)
- `_verif_001/002.lmt`, `_verif_diff_001_002.txt`, `_acceptance.*` — 验证产物
- `E:\MHW-arts\TIML-Studio\` — 二进制解析/序列化 + `timl-diff.js` 比对器
  (`npm test` 27 项: 解析/规范序列化字节对照 vanilla/回环/配方/保存校验)

## TIML 声音/事件编辑器 (2026-10-17, timl_sound_editor.py, addon main 4cb534e)

N 面板 FreeHK → **"TIML 声音/事件"** (与窗口编辑共用 action 选择器, 默认折叠)。
编码 2026-10-17 声音专项的实证语义 (全文见 TIML_PARAM_REGISTRY.md):

- **mTrigger 触发语义** (EG02 喊声 A/B 确证): 键帧=触发帧, 键值=ReqNo 行位掩码
  (bit0=A..bit5=F), f0=0 基线键勿删。面板实时解码 "f130 → C(1019)"。
- **操作**: 加/改触发 (帧+掩码)、删触发 (=删那次发声)、挪触发 (双通道同步,
  保插值)、设 ReqNo 行值 (f0 常数键)。
- **角色速查** (按 action 名显示): EG02=喊叫载体 / EG06=金属摩擦 ⚠触发语义未
  isolated (收窗实验全灭, 慎删挪) / GameParameter=刀风=连续参数轨 (无 mTrigger,
  改时机=PO 曲线编辑器动 GameParam A/B 键) / EffectParameter1=踏步 FX (KeyFrame
  窗 + WorkNo id) / EG01=角色未 isolated。
- **安全护栏**: 插值仅限 exporter 七种 (BEZIER 会崩导出, 面板操作全部自动设
  SINE/LINEAR); 整数帧; bigflag 双通道自动同步。
- 无头: `bpy.ops.freehk.timl_snd_list(action_name=...)` / `timl_snd_trig_add`
  / `timl_snd_trig_del` / `timl_snd_trig_move` / `timl_snd_row_set`。
