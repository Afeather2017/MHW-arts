# MHGU 14 武器公共动作 — 位置与清单 (2026-10-01)

结论先行：**MHGU 玩家公共动作全部在 `arc/player/quest/com.arc` 里,共 4 个 LMT:
`player/mot/co/co_0 / co_1 / co_2 / co_6 .lmt` (co = common),合计 500 槽 / 280 个
非空动作。** 武器包 (wXX.arc) 里只有各自的 `wXX.lmt` + `wXX_sa.lmt`,不含公共动作
副本;风格包 (wXX_m0~m5.arc / com_m0~m5.arc) 只有指令表 (plbasecmd,XFS 容器),
没有任何 LMT。

## romfs 源路径

```
<romfs>/arc/player/quest/com.arc        ← 战斗公共动作 (核心目标)
  player/mot/co/co_0.lmt                1.14 MB
  player/mot/co/co_1.lmt                0.91 MB
  player/mot/co/co_2.lmt                1.45 MB
  player/mot/co/co_6.lmt                0.15 MB
  effect/psl/player/mot/co/co_*.psl     ← 同名 4 份特效时间表 (迁移特效用)
  player/hit/pl_com_hitdata.hdp         ← 公共打击数据 (14 武器共用)
  player/hit/pl_com_hitsize.hts
  player/param/pl_com.fup               ← 公共参数

<romfs>/arc/player/lobby/com.arc        ← 集会所公共动作 (表情/动作游戏,战斗迁移用不到)
  player/mot/co/lco_0.lmt ~ lco_3.lmt
```

## LMT 统计 (v67,`_lmt_summary.py` 实测)

| 文件 | 槽位 | 非空 | 帧数范围 | 备注 |
|---|---|---|---|---|
| co_0.lmt | 171 | 122 | 7–277 | 公共主体 (待机/收刀/道具/受身类,槽 0 = 199f 待机) |
| co_1.lmt | 87 | 63 | 11–433 | 公共组 1 |
| co_2.lmt | 220 | 82 | 3–521 | 公共组 2 (槽位号最大到 199+) |
| co_6.lmt | 22 | 13 | 37–299 | 公共组 6 (小集合) |
| **合计** | 500 | **280** | | |
| lco_0.lmt | 68 | 19 | 17–277 | lobby,是 co_0 的子集 + 独有 |
| lco_1.lmt | 17 | 3 | 77–287 | lobby |
| lco_2.lmt | 220 | 27 | 17–521 | lobby,槽位与 co_2 同布局 (199–218 段独有) |
| lco_3.lmt | 226 | 64 | 3–833 | lobby 独有集合 |

co_0/1/2/6 的槽位编号不连续 (如 co_0 用 0-2,3-15,18,20,21,25...)——编号即游戏内
motion id,和 w07.lmt 一样是"槽位表"而不是紧凑列表。

## 全 romfs 排查结论 (无其他公共动作源)

| 位置 | 内容 | 有无公共 LMT |
|---|---|---|
| arc/player/quest/com.arc | **公共动作本体** | ✅ co_0/1/2/6 |
| arc/player/quest/wXX.arc (w00–w14, 缺 w05) | 各武器 `wXX.lmt`+`wXX_sa.lmt`+hitdata+特效 | ❌ 只有武器自己的 |
| arc/player/quest/w15.arc | 第 15 套玩家数据:`w15_0/2/3.lmt` + pma + param (疑似 Prowler/特殊形态,待确认) | 非 14 武器公共 |
| arc/player/quest/wbc.arc | fup/sep/tex 特效资源 | ❌ 无 LMT |
| arc/player/quest/com_m0~m5.arc | 6 风格公共指令表 plbasecmd (XFS) + sdl + frl | ❌ 无 LMT |
| arc/player/quest/wXX_m*.arc | 各武器 6 风格指令表 plbasecmd | ❌ 无 LMT |
| arc/player/lobby/com.arc | 集会所公共 `lco_0~3.lmt` + bug.lmt | (lobby 用) |
| arc/player/com/com.arc | 武器展示参数 plgmktype/模型/音效 | ❌ 无 LMT |
| arc/player/com/m|f/NNN/*.arc | 猎人模型部件 (arm/body/helm/leg/wst = mod/mrl/tex) | ❌ 无 LMT |
| arc/npc/mot.arc | NPC 动作 | 非玩家 |

武器目录只有 14 个:w00 w01 w02 w03 w04 w06 w07 w08 w09 w10 w11 w12 w13 w14
(w05 不存在;太刀 = w07,与既有迁移管线一致)。

## 本地已解包位置

```
research/mhgu/extracted_common/com/player/mot/co/co_*.lmt   ← 公共动作 (核心)
research/mhgu/extracted_common/com_m0~m5/                   ← 6 风格公共指令表
research/mhgu/extracted_lobby/player/mot/co/lco_*.lmt       ← 集会所公共动作
research/mhgu/extracted_w15/player/mot/w15/w15_*.lmt        ← w15 三件套
research/mhgu/extracted_w07_cmd/w07_m{,0..5}/               ← 太刀 6 风格指令表
research/mhgu/extracted/w07/                                ← 既有太刀动作 (w07.lmt / w07_sa.lmt)
research/mhgu/mhgu_filetype_hashes.txt                      ← ARC 扩展名 hash 表 (本地副本,
                                                               来自 research/MHGU-Modding/MHGU-Filetypes_Hashes.txt)
```

解包命令 (通用):

```bash
cd E:/MHW-arts/research/mhgu
python extract_arc.py "<romfs>/arc/player/quest/com.arc" extracted_common/com \
    --hashes mhgu_filetype_hashes.txt
```

统计/复检工具: `python _lmt_summary.py <lmt...>` (v67 头: entryCount@0x04,
u32 偏移表@0x0C,动作头 0x3C 字节,详见 MHGU_LMT_FORMAT.md)。

## 迁移注意 (与既有管线衔接)

- 公共 LMT 同为 v67/32 位偏移格式,`w07_sa → GLB` 的 sakura_batch 流程直接适用:
  把 co_*.lmt 加进 batch.json 的 secondary 列表、主文件仍用
  `m_body001_out.glb` (骨架同一套,co 系列动作每条 21 轨 = 全身骨)。
- co_6.lmt 槽 21 的头部异常 (0f/浮点位图案),解析时按 0 轨处理即可,属空槽变体。
- 武器 FSM/风格指令 (plbasecmd,XFS 容器) 里引用的 motion id 指向 wXX.lmt 还是
  co_*.lmt 尚未解码——精确对上"哪个公共动作 id = 哪个行为"需要下一步解析
  XFS 指令表 (FSM-Studio 的 XFS 解析器可复用)。
