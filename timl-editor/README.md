# TIML 窗口编辑器 (FreeKinetics 扩展) + 导出验证链

覆盖五个功能: **防御帧 (b28 角度格挡) / 无敌帧 (b26+2048门) / 攻击帧 (EventCollision.mFlag
整值窗) / 按键窗 (PlMotionInput 位窗) / 霸体帧 (b24)**。完全复用 FreeKinetics 的 TIML
表示 (TIML action → `FreeHKTiml.m<hash>` fcurve, bigFlags 类按 array_index 0/1 拆
低/高 16 位), 不引入任何新序列化 — 编辑结果直接走 addon 原有导出。

## 安装

已完成 (本机): `timl_window_editor.py` 已拷入
`%APPDATA%/Blender Foundation/Blender/4.3/scripts/addons/MHW-FreeKinetics/`,
并在 addon `__init__.py` 的 `modules` 列表挂了注册。重开 Blender 即生效。

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

**按键系 (PlMotionInput = 输入接受窗)**:

| 轨 | 位 | 语义 | 置信度 |
|---|---|---|---|
| mFlag | b8 | 翻滚接受 | 确证 |
| mFlag | b14/b16/b30 | 连段预输入 | 确证 |
| mFlag | b17+b31 | 特殊派生窗 (见切→必殺攻撃, 成对) | 确证 |
| mFlag | b9/b15/b0 | 伴出位/未知 | 未解 |
| mFlag1 | b12+b26 / b13+b27 | 组合键开关 (R2+方向 解锁) | 推断 |
| mFlag2 | b4-b9, b22/b23 | 未解 (值族 0x150/0x3F0/0x3C300) | 未解 |
| mFlag3-6 | — | 未解 (mFlag4 在 342 见 4键) | 未解 |

面板"任意位窗"(轨 0-6) 可对所有 bigFlags 轨画窗; 新观测到的语义直接改
`timl_window_editor.py` 里的 `BIT_SEMANTICS` 表 (标签+置信度), 面板/清单/报告同步生效。

## addon 修复 (2026-10-15)

1. **`freeHKActionNodes.py parseBigFlag` 键序 bug**: 合并双通道时按 "低通道全部帧 →
   高通道全部帧" 的 dict 插入序产出键列, 单边 (只改一个通道) 加键时导出**乱序键** —
   游戏要求键按帧有序。修复 = `sorted(channelFrames.items())`。已回归: 未编辑的
   0.0.2 重导出逐字节一致 (md5 f724d191...)。
2. (编辑器内) 关闭键继承 "当前值清目标位", 不写 0 — 写 0 会把同通道其他位一起关掉。

## 文件

- `timl_window_editor.py` — addon 扩展源 (部署拷贝在 addon 目录)
- `_export_blend_lmt.py` — 参数化无头导出 (BLEND/OUT 环境变量)
- `_inspect_timl_blend.py` / `_dump_timl_action.py` — blend 内 TIML 形态检查
- `_acceptance_run.py` — 验收脚本 (单会话 ops+export)
- `_verif_001/002.lmt`, `_verif_diff_001_002.txt`, `_acceptance.*` — 验证产物
- `E:\MHW-arts\TIML-Studio\` — 二进制解析/序列化 + `timl-diff.js` 比对器
  (`npm test` 27 项: 解析/规范序列化字节对照 vanilla/回环/配方/保存校验)
