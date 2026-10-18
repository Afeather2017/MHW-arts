# MHW 武器攻击箱研究：wp03.col 完整解析（2026-09-19）

研究问题：武器攻击判定盒存在哪、怎么改、"为什么跟着武器走"、Blender 有没有现成插件、
MHW-valor / 居合开刃(SR) 两个 mod 的作者各改了什么。

**结论一句话**：武器攻击箱不在 TIML 里也不在模型上，而是 `hm/wp/wp03/collision/wp03.col`
里的**参数化攻击体积**（Attack_Range + 宽/高/深 + ±90° 扫掠角）+ **ATK 伤害表** + 名字路由表。
TIML 的 `EventCollision00` 只决定**何时开窗**，`CollisionTimelineObject` 的
`バンク/ノードID` 通过 col 名字表路由到具体几何体和伤害行。删 TIML → 窗口没了 → 攻击失效，
但几何体本身一直在 col 里。

---

## 1. wp03.col 文件格式（实证解析 + Synthlight/MHW-Editor 010 模板双确认）

文件位置：`nativePC/hm/wp/wp03/collision/wp03.col`（原版 20408 字节）。
工具：`col_research/col_tool.py`（dump / bank / atk / setval，自动 .bak）。

```text
COL\0  头部: magic, "COL\0", magic2=111(组数), 0, unk=116, Name_Count=61
Names[61]              名字路由表（每项 = 字符串 + 19 字节）
CLND   @0x0a43         "Collider Node Param": (111, bank数13, type=1) + 21 字节
CLGM ×13               "Collider Geometry Param": 每行一个 bank
ATK\0  @0x1d22         (111, 59 条, type=1) 伤害/打击参数表，每条 218 字节
OAP\0  @0x4f78         64 字节 footer
```

### Names 条目（路由表，这是理解引用链的关键）
```c
string  name;      // 招式名（日文, null 结尾, UTF-8）—— wp03.col.csv 有英译
int     cgp;       // = CLGM bank 序号（几何体在哪个"武器长度档"）
int     move;      // = ATK 表行号（伤害数据）
int     unk1;      // 0
ushort  unk2;      // 0
ubyte   unk3;      // 0
uint    unk4;      // = 节点 ID！TIML CollisionTimelineObject 的 ノードID 就是它
```
实证：見切 LMT140 的 TIML ノードID=63 ↔ name[24] 見切り攻撃 unk4=63；
小居合 307 的 node=101 ↔ name[45] 居合斬り2 unk4=101。**ノードID → 名字表(unk4) → (bank=cgp, 伤害行=move)。**

### CLGM bank（几何体）
每个 bank = **一个武器模型长度档**（同一 LS 系列不同刀长），内含 **6 个固定槽位记录**，
每条 72 字节（010 模板 Collider_Geomtry_Param_Inner）：

| 偏移 | 名字 | 含义（已证实/推测） |
|---|---|---|
| +0 | u1,u2 | 1,0/1,1 标志 |
| +2 | **Attack_Range** | 攻击距离（主参数，MHW-Editor 也是唯一命名了的字段）|
| +6..14 | f6,f10,f14 | 恒 20,20,20（宽/高/深？）|
| +18,+20 | s18,s20 | **-1=无**；valor 新记录 s18=8 —— 疑似骨骼/节点绑定（未证实）|
| +30 | f30 | 各 bank 同槽位恒同值：3/118/153/69/240/154（特效或 req 相关 id？）|
| +42 | f42 | rec0 恒 240（第二距离参数？）|
| +48 | u48 | 8/9/10 —— **疑似 = EventCollision00 的 ReqNo A/B/C**（見切 ReqNo=1,2,10）|
| +54..66 | f54,f58,f62,f66 | **±90° 扫掠角**（横/纵角度范围）|
| +70 | u70 | = 槽位自身序号 |

**Attack_Range 语义 2026-09-19 游戏内实锤（用户实测）**：rng 是**判定体积的等比缩放参数**。
把突刺（bank1）rng 改 500（原版 50 的 10 倍）：攻击范围明显变大，用户感知约 3 倍，且
**身后的怪物也被打中** → 体积是以玩家为中心的椭球，平时数值小 + 前向 ±90° 扫掠，
看起来才像"只在刀前"。标定经验：感知尺寸 ≈ √rng（rng×10 ≈ 体积×3）。
先前的"bank4 和 bank0 相同=长度档"推测作废——bank 是**按招式调好的几何套组**
（bank0=普斩 50、bank1=突刺 50/57、bank6=必殺 65/77、bank8=见切 75/93、bank9=居合2 100/124、
bank10=骑乘 360；短 bank2/3=[10..17] 暂无招式指向，可能是预留/特殊槽）。
翻转倍实验：全 bank rng×2 对突刺无可感知变化（幅度被动作前扑淹没），rng=500 才明显。

### ATK 条目（218 字节，type=1 = Atk1，MHW-Editor 命名）
Index, Attack_Type, **Motion_Value(动作值)**, **Fixed_Damage**, Part_Modifier(部位补正),
Elem_Modifier, Status_Modifier, Stun, Exhaust, Mount, Soften(软化), Flinch,
**MindsEye(心眼)**, **HitLag_a/HitLag_b(打击停顿)** + HitLagSpd 倍率,
**Lingering(残留判定)**, **Frame_Delay(帧延迟)**, **No_Sharpness(不耗斩味)**,
**Energy_Gen(炼气增益)**, Cond_MV/Cond_Elem(条件动作值替换) 等。
（ATK 还有 type 0/2/3：击退/防御条/异常状态值/贯通等，wp03 全是 type 1。）

**Part_Modifier（+16）语义定案（2026-10-19，A/B 局内实锤）**：= **每击部位（破坏）积累乘数**，
每击破坏积累 ≈ 伤害 × Part_Modifier（再乘 Partbreaker 技能/怪物侧系数）。资源字段，非二进制写死。
证据：大居合 308-311（node86-89→行46-49）与无双斩（node89→行49）破坏恒 0——这 4 行继承原版
居合気刃1無白黄赤，原版 Part_Modifier 全 0.0（原版居合气刃1 本就不破坏）；刚1（见切→node63→
行25）Part_Modifier 1.0 → 能破坏。原版旁证：居合気刃2（原版大居合）= **1.5**、張り付き1/2（柔化）
= 0、乗りチャンス1-3 = 0。MV（血伤）不受影响。修复：行46-49 → 1.0（col_tool setval，2026-10-19）。

**cAttackParamPl 运行时布局**（decompile 2026-10-19；类工厂跳表 0x143F949E0[type]，类名串
0x142F12DD0 "cAttackParamPl"，Load FUN_14027d880 / Save FUN_14027dd20，实例 0x270 字节，
行对象指针数组在 ATK section obj+0xC8，count +0xB8/capacity +0xBC）：+0x08 Index、
+0x18 Attack_Type、+0x20 MV、+0x24 Fixed_Damage、**+0x28 Part_Modifier**、+0x68 Stun、
+0x6c Exhaust、+0x1c8 Soften、+0x1d0 Elem_Mod、+0x1d4 Status_Mod、+0x1d8 Mount、
+0x232 MindsEye(bool)、+0x240 Flinch(u32)。运行时字段偏移 ≠ 文件偏移（重新布局过），
DLL 侧读写用运行时偏移。

### 打击停顿 HitLag 速查（2026-09-19 全表归纳，单位=秒，a/b 两级但原版恒同值）
| 值 | 用在哪 |
|---|---|
| 0 | 无停顿（工具行/部分特殊行）|
| 0.02–0.06 | 轻攻击（突刺/纵斩/踏步斩/滑行斩 全家 0.06；气刃3一段二段 0.03 快速连击件；居合系 0.05）|
| 0.10 | 中（气刃斩1/2、见切反击、乘りチャンス）|
| 0.15 | 重（气刃大回旋原版值；SR 把气刃3一二段从 0.03 提到 0.15 = 重击化）|
| 0.20 | 很重（居合气刃2、張り付き2）|
| 0.30 | 超重（必殺/居合气刃斩、必殺(赤、乘りチャンス头/胴/尾）|
| 0.40 | 全表最大（必殺(空中) 跳劈）|

- HitLagSpd_a/b 原版全 1.0（仅"削除予定"行 0.0）——含义未知，**编辑时保持 1.0**。
- HitLag_a 与 HitLag_b 原版恒同值，两级的选择条件未知 → **编辑时两个一起改同值**（SR 也是这么干的）。
- 配方（**2026-09-19 用户实测通过**：突刺 HitLag 0.4 → 命中冻结明显）：
  `col_tool.py setval <col> atk <move> HitLag_a 0.3` + `... HitLag_b 0.3`。

### 多段判定 Lingering + Frame_Delay（多段猎魂的关键， Units 未验证）
原版 3 行开启：必殺多段(Lag0, Ling=1, FDelay=4.0)、必殺ディレイ(0,1,1.5)、張り付き3(0.02,1,3.0)。
→ **Lingering=1 + Frame_Delay=N = 同一判定盒对同一目标重复命中**，N 疑似再命中间隔（秒？帧？未验证）。
将来樱花气刃斩式多段冲刺技：开 Lingering + 调 Frame_Delay + TIML 开长窗，三件套即可多段化。

---

## 2. 攻击的完整数据链（编辑时四件套一起看）

```text
LMT/TIML  EventCollision00.mFlag      何时判定（帧窗口，0=关 1/2/3=槽组）
LMT/TIML  EventCollision00.ReqNo A/B/C  请求哪个判定槽（↔ CLGM 记录 u48？）
LMT/TIML  CollisionTimelineObject     バンク(bank) + ノードID(名字表.unk4) 路由
col/Names unk4=ノードID 的条目        → cgp 选 CLGM bank，move 选 ATK 行
col/CLGM  bank 的 6 槽记录            几何体：距离/角度/尺寸
col/ATK   move 行                     伤害：动作值/部位/停顿/炼气/软化
```
所以"删 TIML 攻击失效"是因为窗口和路由都在 TIML。几何与时机彻底解耦（交换实验证实）：
**TIML 管"何时"，CLGM 管"形状/大小"（rng 缩放），引擎在窗口内把体积贴着动画中的武器
扫掠**——位置跟武器走是引擎行为，不是数据；ATK 管伤害。突刺"点"和扫斩"弧"的差异
来自动画轨迹，不来自 col 数据。

### 未解之谜（下一步 A/B 就能钉死）
- **s18/s20（-1 vs 8）**：是否骨骼绑定？valor 给新记录填了 8，原版全 -1。
- **f30（3/118/153/69/240/154）**：跨 bank 同槽位恒同 → 是"槽位类型"的属性。
  疑似特效/joint/req id。u48=8/9/10 ↔ EventCollision00 ReqNo A=1,B=2,C=10 最可疑。
- **位置问题已解决（2026-09-19 用户交换实验）**：把突刺(3:79,LMT105)和气刃斩1(3:11,LMT104)
  的攻击箱互换后：**判定跟着各自武器动画的位置走**（戳一下=小前方、扫斩=180°前弧），
  命中时机变化只是几何与动画错位的表现，**生效窗口（TIML）不变**。结论：引擎在窗口内
  把判定体积贴着动画中的武器扫掠——数据里没有也不需要"位置"字段。突刺/扫斩的形态差异
  来自动画轨迹本身，CLGM 只做围绕轨迹的缩放。名字表标签与实物一致再添两例
  （突刺 3:79→node42 标签"突き"、气刃斩1 3:11→node56 标签"気刃斬り１"）。
  动作映射：3:79=LMT105=突刺，3:11=LMT104=气刃斩1（action-list.md 已收录 11→104）。
- 实验方法照抄 TIML 防御位那套：`col_tool.py setval` 改一个值 → 游戏里挥一刀 → 观察。

### 实验日志
| 日期 | 改动 | 结果 |
|---|---|---|
| 2026-09-19 | 全 59 记录 rng ×2（50→100 等） | 突刺（bank1, FSM 3:79）无可感知变化——幅度太小被动作前扑淹没 |
| 2026-09-19 | bank1 两记录 rng=500（10×） | **阳性**：范围明显变大（感知≈3×），身后怪物也被打中 → rng=判定椭球缩放，以玩家为中心 |
| 2026-09-19 | 测后已还原 | 部署文件恢复为 SR 版（wp03.col.sr-backup 同 md5 8fa4f67e） |
| 2026-09-19 | 用户互换突刺(3:79)与气刃斩1(3:11)的攻击箱 | **阳性**：判定跟武器动画走（戳=小前方、扫=180°），窗口不变 → 几何与时机解耦，位置无需数据 |
| 2026-09-19 | 突刺 ATK[10] HitLag_a/b 0.06→0.40 | **阳性**：命中冻结明显 → 打击停顿=ATK HitLag 字段（秒），已实测闭环；测后还原 SR 版 |
| 2026-10-19 | 行46-49（居合気刃1無白黄赤，现役=大居合/无双斩）Part_Modifier 0→1.0 | **阳性 user-verified**（改回 collision 目录后破坏恢复）；A/B 依据=刚1（行25, 1.0）能破坏 / 无双斩（行49, 0.0）破坏恒 0 |
| 2026-10-19 | 无双斩建专属行 42/45（node90/94 重指，LMT343 节点键 f0/f58 重指）| **阳性 user-verified**：单刀≈1000 伤符合预期；hit1 PartMod0 顺带治好"第一刀打断怪头→第二刀落空"——Part_Modifier 喂的是部位积累槽（破坏+部位踉跄共用），归零=该击不打部位 |

---

## 3. 两个 mod 作者各改了什么（vs 原版 wp03.col 的字节级 diff）

### 居合开刃 (SR, spirit-reckoning)：数值调优派，45 字节
全在 ATK 表 + 一处几何：
| 位置 | 改动 | 解读 |
|---|---|---|
| ATK[2] ジャンプ気刃斬り(ゲージ無し) | MV 16→25, Mount 40→0, HitLag 0.06→0.15, Energy_Gen 0.16→0.40, Cond_MV 30→0 | 跳跃气刃斩强化 |
| ATK[18] 気刃斬り３(一段) | MV 14→25, HitLag 0.03→0.15 | 气刃3一段重击化 |
| ATK[19] 気刃大回転斬り | MV 38→55, HitLag 0.15→0.30 | 大回旋大幅强化 |
| ATK[23] 気刃斬り３(二段) | MV 19→25, HitLag 0.03→0.15 | |
| ATK[24] 気刃斬り３(三段) | MV 34→**18**, Energy_Gen 0→0.25 | 三段降伤改为充能技 |
| ATK[55]/[57] | Soften 35→100 / 15→50 | 软化值 |
| **CLGM bank4[0].rng** | 50→65 | 唯一几何改动：加长该档攻击距离 |

→ **SR 证明：改伤害手感根本不用碰几何体，全在 ATK 行。**

### MHW-valor（南风焓）：结构改造派，+362 字节
1. 名字表 61→62 条，**新增 "南风焓"**：bank=13(新), move=59(新), node=102
2. **CLND 13→14 banks**：追加 bank13，含 1 条新记录 rng=**200.0**（刚1大前突距离）、
   **s18=8**（全文件唯一非 -1！）、f22=30、s52=-1
3. **見切り攻撃 cgp 8→13**：把见切的攻击箱重指向新 bank —— 复用见切动作打刚1判定
4. 居合斬り2 cgp 9→0（改回普通档）
5. ATK 59→60 条（新增 move 59 的伤害行）

→ **valor 证明：结构操作（加 bank/加名字/加 ATK 行/重指向）是可行的**，
col 没有全局校验和，追加 section 后游戏照常读。

---

## 4. Blender 插件现状（本机 4.3 已装插件盘点）

| 插件 | 能改什么 | 和攻击箱的关系 |
|---|---|---|
| **MHW_Model_Editor** | `.mod3/.mrl3/.ctc/.ccl` | **.ccl = 骨骼绑定胶囊体碰撞**（StartID/EndID 骨骼+球/胶囊, 64B/条）——那是**身体/挂件/环境**碰撞（acc/evm/shell），**不是武器攻击箱**；但如果你要"骨骼绑定的攻击判定"视觉参考，它是现成的 |
| **MHW-FreeKinetics** | `.lmt/.timl/.efx/.fhat/.pl` | LMT+TIML：攻击**窗口**（EventCollision00）在这里改 |
| Modding-Toolkit / RE-Chain-Editor / RE-Mesh-Editor | MHWI 模型/RE引擎 | 与 .col 无关 |
| （无）| `.col` | **没有任何插件解析 CLGM/ATK** —— 用 `col_tool.py`；格式规范 =
   `col_research/mhw-editor-src/010 Templates/col.bt`（sparse clone 自 Synthlight/MHW-Editor）|

MHW-Editor (Nexus/GitHub) 图形界面也能编辑 .col（CollisionTranslations 功能，
Attack Range 已命名），但其余字段在社区层面也是 Unk——我们的解析已对齐并超过它
（bank=长度档、u48↔ReqNo、node↔unk4 链是本次新结论）。

---

## 5. 实用配方（col_tool.py）

```bash
cd E:\MHW-arts\col_research
python col_tool.py dump  <col>            # 全览：名字表 + 各 bank rng + ATK 数
python col_tool.py bank  <col> 4          # 看 bank4 的 6 条记录
python col_tool.py atk   <col> 18 19 23   # 看气刃3一段/大回旋/二段的伤害行
# 改伤害（SR 同款）：
python col_tool.py setval <col> atk 19 Motion_Value 55
python col_tool.py setval <col> atk 19 HitLag_a 0.30
python col_tool.py setval <col> atk 24 Energy_Gen 0.25
# 改几何（SR 同款，bank=长度档, rec=槽位）：
python col_tool.py setval <col> rng 4 0 65
```
注意：只支持**等长数值编辑**（字符串/条目数不能变，否则全部偏移崩）。
valor 式的结构追加需要写重建器（解析器已具备，按 section 重打包即可，未实现）。

部署路径：`E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\collision\wp03.col`

---

## 6. 相关文件
- `col_research/col_tool.py` —— 解析/编辑 CLI（本研究的可执行成果）
- `col_research/mhw-editor-src/010 Templates/col.bt` —— 010 模板（官方字段名来源）
- `col_research/mhw-editor-src/CollisionTranslations/wp03.col.csv` —— 招式名英译表
- 解析脚本存档：col_final2.py（解析器）、col_valor.py（valor diff）、col_xref.py（node 交叉验证）
- 关联研究：`LMT_TIML_Hitbox_IFrame_SuperArmor_Sheath_Research.md`（TIML 窗口/防御位）
