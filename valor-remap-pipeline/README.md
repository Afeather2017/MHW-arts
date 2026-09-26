# valor-remap-pipeline — MHRS 动作 → MHW 太刀 remap 后处理链

把 ARP 重定向后的 `plw_LongSword_*_Armature_remap` 动作修到**游戏内可用**状态的三步处理，
外加检查脚本。终态基准 = `E:\MHW-arts\MHW-Valor.blend`（2026-09-27 用户局内验证通过：
位置正常、IK 跟随、武器左手绑定）。

## 运行环境

- Blender: `E:\MHW-arts\tools\blender-4.3.2-windows-x64\blender.exe`（FreeKinetics 扩展需已装）
- 目标 blend 默认 `E:\MHW-arts\MHW-Valor.blend`，用环境变量 `VALOR_BLEND` 指向别的文件
- 每个处理脚本运行前会自动写 `<blend>.bak-<时间戳>` 备份；验证不过**不保存**

```bash
cd /e/MHW-arts/valor-remap-pipeline
VALOR_BLEND="E:/MHW-arts/新文件.blend" ../../tools/blender-4.3.2-windows-x64/blender.exe -b --python 2_object_root_motion.py
VALOR_BLEND="E:/MHW-arts/新文件.blend" ../../tools/blender-4.3.2-windows-x64/blender.exe -b --python 3_rebake_ik.py
VALOR_BLEND="E:/MHW-arts/新文件.blend" ../../tools/blender-4.3.2-windows-x64/blender.exe -b --python 4_grip_left_from_lmt106.py
```

## 步骤顺序（必须按 2 → 3 → 4 → 5）

### 1.（前置）ARP 批量重定向 —— 生成 `*_remap` 动作
不在本文件夹内。两条路：
- 无头：`E:\MHW-arts\arp_remap_mhrs_to_mhw.py`（mhrs-valor-mod-motions.blend → -remapped.blend，
  依赖 `mhrs-to-mhw.bmap`，root 条目带 location）
- GUI：Auto-Rig Pro 配好后跑 `E:\MHW-arts\arp_batch_retarget_and_migrate.py`
- 旧链路的 `batch_migrate_plw_actions.py`（IK 烘焙+挂点迁移）**可跳过**：它的产物会被本链
  step2/3/4 全部覆盖；跑了也无害（本链就是在一个跑过它的文件上调通的）

### 2. `2_object_root_motion.py` — 根位移搬到 object 层（⚠️ 只能跑一次）
- 依据原生 `LMT::w03_00::*` 的 7 通道结构，把 MhBone_000 的 X/Z 水平位移搬到
  object 级 `location[0]/[1]`：`objX = boneX - 首帧值`、`objY = 首帧Z - boneZ`（镜像+归零，
  原生首帧恒 0）；`location[2]` 恒 0、四元数恒 identity、全 BEZIER
- 骨架侧 X/Z 清零（只留 loc Y = 高度），高度键转 BEZIER
- 删掉原生不存在的 IK 通道类型（IK 骨骼 scale、254 location）
- 内置护栏：检测到已迁移（object 曲线已存在且骨骼 X/Z 已平）会拒绝运行——**别绕过**，
  重跑会把 object 曲线洗成 0

### 3. `3_rebake_ik.py` — IK 重烘焙（必须在 step2 之后！）
- 原理：IK 骨骼（247-253）无父级，tether 烘焙写入的是骨架空间值。位移存放位置一变，
  烘焙值就失效——旧的"含位移烘焙 + object 位移"= IK 目标以 ~2 倍位移跑到角色前面
  （实测最高偏 3.9m）。所以**任何动了根位移的操作之后都要重跑本步**
- 身体 IK：247→掌057、248→掌040、249→踝020、250→踝016（FULL）、252→头004（LOC）；
  254 刻意不写（原生 LMT::w03_00 无 254 location）
- 地面锚点：251/253 钉在原点（在 frame 0 求 world，那时 object 位移为 0）
- 验证不过不保存：全身 IK↔目标骨骼距离 < 1mm（每动作抽 5 帧）+ 锚点恒在原点

### 4. `4_grip_left_from_lmt106.py` — 左手绑定默认值（幂等，可重复跑）
- 030（持剑挂点）= 原生 `LMT::w03_00::106` 原样拷贝：frame0 单常量键，
  loc=(0.1008, −0.0404, 0.5808)、quat=(0.0365, 0.0045, −0.0115, 0.9993)、scale=(1,**2**,1)
- 047（右手挂点，不激活）= 106 首帧 loc/quat 常量 + scale=(1,1,1)；106 的 047 是动画
  （那招有双手瞬间），刻意不照搬
- 跨游戏事实：Rise `L_Weapon_00` 握点偏移与 MHW 030 完全一致（±1e-4），两引擎太刀左手
  握法同约定
- 个别动作需要别的绑定态（如居合 (3,x)）时，用根目录 `author_grip_states.py` 的原生
  槽位拷贝思路单独覆盖；键写入配方见 `grip-states.md`

### 5. `5_close_stance_loops.py` — 归位类动作的位移闭合（必须在 step3 之后）
- Rise 的水平根位移在 `Root` 骨骼上（Cog 只有高度），且动作表是接龙编排：首键值非零
  （如 112 的 -0.2166 是上一个动作留下的链上坐标）、净位移不回零
- 判定：动作**终帧姿态**与原生 `LMT::w03_00::001` 站桩对比（IK 目标骨骼 247/248/249/250/252
  的旋转均差 <5°）→ 归位类。112 实测 0.6~0.95°；保留类的差 6°~42°，分界非常干净
- 闭合 = object `location[0]/[1]` 减线性坡道使**终点归零**（起点已是 0），中途摆动保留；
  真正带位移的动作保持开放（原生 154 个里 92 个 X 终值非零、最大 4.3m，引擎按动作累积位移）
- 效果实测：112 终帧头部 (0.095, -0.211) vs 原生 001 (0.095, -0.21)，毫米级吻合
- 依赖 step3：姿态判定读的是烘焙后的 IK 通道，顺序不能换

## 拼接两个动作 — `splice_actions.py`（独立脚本，可直接粘贴进 Blender 文本编辑器跑）

A 结束姿态 = B 起始姿态时，把 B 接到 A 后面生成**新动作**（A/B 原件不动）。
顶部 CONFIG 改两个动作名即可；`SEAM_GAP=0` 表示 B 的 f0 落在 A 的最后一帧。

**不需要重做 IK**：本项目约定下位移全在 object 层、IK 烘焙值是姿态空间（无位移），
拼接只需 ① B 整体时间平移 ② B 的 object location 加上 A 的末位移——脚本自动做这两件事，
IK/骨骼通道随时间平移直接有效。只有动作**不是**本管线产物（布局不同）才设
`REFRESH_IK = True`（调 FreeKinetics 重烘焙 247-253）。

输出信息里的 `seam-warn` = 某通道 B 的接缝键值与 A 相差 >0.0001（姿态没对齐），
此时保留 A 的值做 1 帧过渡；严格对齐的接缝不会有 warning。
`checks/verify_splice.py` 是接缝连续性的自动验证（默认动作对，可改）。
已验证：两个 Loop 动作拼接 0..160 帧，接缝零跳变、位移正确翻倍。

## checks/ — 检查脚本（只读）

| 脚本 | 用途 |
|---|---|
| `validate_pipeline_reproducibility.py` | 把 `VALOR_BLEND` 与基准 MHW-Valor.blend 逐动作对比（object 曲线/骨骼清零/IK 抽查/grip 值），输出 REPRODUCIBLE=true/false |
| `inspect_valor_blend.py` / `inspect_valor_blend2.py` | 动作清单、根通道、object 曲线、原生参照结构 |
| `inspect_body_scale.py` | body scale 通道普查（ARP 写的恒 1.0，无害保留） |
| `inspect_rise_sockets.py` / `inspect_rise_odd_sources.py` | Rise 侧挂点通道、异常源动作排查 |
| `inspect_lmt106.py` | 106 的 030/047 通道结构（grip 参照） |

## 已知注意事项

- 源动作里曾混入 `plw_LongSword_100_002_Armature_remap.001`（MHW 骨骼的旧 remap 副本，
  不是 Rise 源）。重建时若再出现按前缀误配的风险，先删掉它
- ARP 会给每根 body 骨骼写恒 1.0 的 scale 曲线——无害（原生没有但值等于 identity），
  勿批量删除（Blender 4.3 批量 remove fcurve 会崩）
- 机制细节文档：`E:\MHW-arts\bones.md`（骨骼/根位移/IK/grip 全套约定）、
  `E:\MHW-arts\grip-states.md`（绑定态键写入配方）、`E:\MHW-arts\action-list.md`（FSM id→LMT 槽位）

## 2026-09-27 回归验证记录

用 `MHW-Valor.backup-20260926-pre-objectloc.blend`（迁移前状态）按 2→3→4 全链重跑，
`validate_pipeline_reproducibility.py` 对比基准：REPRODUCIBLE=true（155/155 动作一致）。
即：本链从迁移前状态可以完整复现当前已验证的好状态。
